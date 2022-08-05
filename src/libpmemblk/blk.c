// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2022, Intel Corporation */

/*
 * blk.c -- block memory pool entry points for libpmem
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <endian.h>
#include <stdbool.h>

#include "libpmem.h"
#include "libpmemblk.h"

#include "mmap.h"
#include "set.h"
#include "out.h"
#include "btt.h"
#include "blk.h"
#include "util.h"
#include "sys_util.h"
#include "util_pmem.h"
#include "valgrind_internal.h"

static const struct pool_attr Blk_create_attr = {
		BLK_HDR_SIG,
		BLK_FORMAT_MAJOR,
		BLK_FORMAT_FEAT_DEFAULT,
		{0}, {0}, {0}, {0}, {0}
};

static const struct pool_attr Blk_open_attr = {
		BLK_HDR_SIG,
		BLK_FORMAT_MAJOR,
		BLK_FORMAT_FEAT_CHECK,
		{0}, {0}, {0}, {0}, {0}
};

/*
 * lane_enter -- (internal) acquire a unique lane number
 */
static void
lane_enter(PMEMblkpool *pbp, unsigned *lane)
{
	unsigned mylane;

	mylane = util_fetch_and_add32(&pbp->next_lane, 1) % pbp->nlane;

	/* lane selected, grab the per-lane lock */
	util_mutex_lock(&pbp->locks[mylane]);

	*lane = mylane;
}

/*
 * lane_enter -- (internal) acquire a unique lane number
 */
static int
lane_try_enter(PMEMblkpool *pbp, unsigned *lane)
{
	unsigned mylane;

	mylane = util_fetch_and_add32(&pbp->next_lane, 1) % pbp->nlane;

	/* lane selected, grab the per-lane lock */
	if (util_mutex_trylock(&pbp->locks[mylane]) == 0) {
		*lane = mylane;
		return 0;
	} else {
		return -1;
	}
}

/*
 * lane_exit -- (internal) drop lane lock
 */
static void
lane_exit(PMEMblkpool *pbp, unsigned mylane)
{
	util_mutex_unlock(&pbp->locks[mylane]);
}

/*
 * nsread -- (internal) read data from the namespace encapsulating the BTT
 *
 * This routine is provided to btt_init() to allow the btt module to
 * do I/O on the memory pool containing the BTT layout.
 */
static int
nsread(void *ns, unsigned lane, void *buf, size_t count, uint64_t off)
{
	struct pmemblk *pbp = (struct pmemblk *)ns;

	LOG(13, "pbp %p lane %u count %zu off %" PRIu64, pbp, lane, count, off);

	if (off + count > pbp->datasize) {
		ERR("offset + count (%zu) past end of data area (%zu)",
				(size_t)off + count, pbp->datasize);
		errno = EINVAL;
		return -1;
	}

	memcpy(buf, (char *)pbp->data + off, count);

	return 0;
}

/*
 * nswrite -- (internal) write data to the namespace encapsulating the BTT
 *
 * This routine is provided to btt_init() to allow the btt module to
 * do I/O on the memory pool containing the BTT layout.
 */
static int
nswrite(void *ns, unsigned lane, const void *buf, size_t count,
		uint64_t off)
{
	struct pmemblk *pbp = (struct pmemblk *)ns;

	LOG(13, "pbp %p lane %u count %zu off %" PRIu64, pbp, lane, count, off);

	if (off + count > pbp->datasize) {
		ERR("offset + count (%zu) past end of data area (%zu)",
				(size_t)off + count, pbp->datasize);
		errno = EINVAL;
		return -1;
	}

	void *dest = (char *)pbp->data + off;

	if (pbp->is_pmem) {
		pmem_memcpy_nodrain(dest, buf, count);
		pmem_drain();
	} else {
		memcpy(dest, buf, count);
		pmem_msync(dest, count);
	}

	return 0;
}

/*
 * nsmap -- (internal) allow direct access to a range of a namespace
 *
 * The caller requests a range to be "mapped" but the return value
 * may indicate a smaller amount (in which case the caller is expected
 * to call back later for another mapping).
 *
 * This routine is provided to btt_init() to allow the btt module to
 * do I/O on the memory pool containing the BTT layout.
 */
static ssize_t
nsmap(void *ns, unsigned lane, void **addrp, size_t len, uint64_t off)
{
	struct pmemblk *pbp = (struct pmemblk *)ns;

	LOG(12, "pbp %p lane %u len %zu off %" PRIu64, pbp, lane, len, off);

	ASSERT(((ssize_t)len) >= 0);

	if (off + len >= pbp->datasize) {
		ERR("offset + len (%zu) past end of data area (%zu)",
				(size_t)off + len, pbp->datasize - 1);
		errno = EINVAL;
		return -1;
	}

	/*
	 * Since the entire file is memory-mapped, this callback
	 * can always provide the entire length requested.
	 */
	*addrp = (char *)pbp->data + off;

	LOG(12, "returning addr %p", *addrp);

	return (ssize_t)len;
}

/*
 * nssync -- (internal) flush changes made to a namespace range
 *
 * This is used in conjunction with the addresses handed out by
 * nsmap() above.  There's no need to sync things written via
 * nswrite() since those changes are flushed each time nswrite()
 * is called.
 *
 * This routine is provided to btt_init() to allow the btt module to
 * do I/O on the memory pool containing the BTT layout.
 */
static void
nssync(void *ns, unsigned lane, void *addr, size_t len)
{
	struct pmemblk *pbp = (struct pmemblk *)ns;

	LOG(12, "pbp %p lane %u addr %p len %zu", pbp, lane, addr, len);

	if (pbp->is_pmem)
		pmem_persist(addr, len);
	else
		pmem_msync(addr, len);
}

/*
 * nszero -- (internal) zero data in the namespace encapsulating the BTT
 *
 * This routine is provided to btt_init() to allow the btt module to
 * zero the memory pool containing the BTT layout.
 */
static int
nszero(void *ns, unsigned lane, size_t count, uint64_t off)
{
	struct pmemblk *pbp = (struct pmemblk *)ns;

	LOG(13, "pbp %p lane %u count %zu off %" PRIu64, pbp, lane, count, off);

	if (off + count > pbp->datasize) {
		ERR("offset + count (%zu) past end of data area (%zu)",
				(size_t)off + count, pbp->datasize);
		errno = EINVAL;
		return -1;
	}

	void *dest = (char *)pbp->data + off;
	pmem_memset_persist(dest, 0, count);

	return 0;
}

/* callbacks for btt_init() */
static struct ns_callback ns_cb = {
	.nsread = nsread,
	.nswrite = nswrite,
	.nszero = nszero,
	.nsmap = nsmap,
	.nssync = nssync,
	.ns_is_zeroed = 0
};

/* Asynchronous blk operations */
#ifdef PMEMBLK_USE_MINIASYNC

#include <libminiasync/vdm.h>
#include "libpmemblk/btt_async.h"

/*
 * START of nsread_async_future
 */
/*
 * TODO: nsread -- (internal) read data from the namespace encapsulating the BTT
 * in an asynchronous way using libminiasync vdm
 */
static enum future_state
nsread_async_impl(struct future_context *ctx,
	struct future_notifier *notifier)
{
	struct nsread_async_data *data =
		future_context_get_data(ctx);
	struct nsread_async_output *output =
		future_context_get_output(ctx);

	if (!data->memcpy_started) {
		struct pmemblk *pbp = (struct pmemblk *)data->ns;

		LOG(13, "pbp %p lane %u count %zu off %" PRIu64, pbp,
			data->lane, data->count, data->off);

		if (data->off + data->count > pbp->datasize) {
			ERR("offset + count (%zu) past end of data area (%zu)",
				(size_t)data->off + data->count, pbp->datasize);
			/* TODO: should we set it in async features? */
			errno = EINVAL;
			output->return_value = -1;
			return FUTURE_STATE_COMPLETE;
		}
		data->op = vdm_memcpy(data->vdm, data->buf,
					(char *)pbp->data + data->off,
					data->count, 0);
		data->memcpy_started = 1;
	}

	if (future_poll(FUTURE_AS_RUNNABLE(&data->op), NULL)
		== FUTURE_STATE_COMPLETE) {
		output->return_value = 0;
		return FUTURE_STATE_COMPLETE;
	} else {
		return FUTURE_STATE_RUNNING;
	}
}

static struct nsread_async_future
nsread_async(void *ns, unsigned lane, void *buf,
	size_t count, uint64_t off, struct vdm *vdm) {
	struct nsread_async_future future = {
		.data.ns = ns,
		.data.lane = lane,
		.data.buf = buf,
		.data.count = count,
		.data.off = off,
		.data.memcpy_started = 0,
		.data.vdm = vdm,

		.output.return_value = -1,
	};

	FUTURE_INIT(&future, nsread_async_impl);
	return future;
}
/*
 * END of nsread_async_future
 */

/*
 * START of nswrite_async_future
 */
static enum future_state
nswrite_async_impl(struct future_context *ctx,
		struct future_notifier *notifier)
{
	struct nswrite_async_data *data =
		future_context_get_data(ctx);
	struct nswrite_async_output *output =
			future_context_get_output(ctx);

	struct pmemblk *pbp = (struct pmemblk *)data->ns;
	unsigned lane = data->lane;
	void *buf = data->buf;
	size_t count = data->count;
	uint64_t off = data->off;
	struct vdm *vdm = data->vdm;

	void *dest = (char *)pbp->data + off;

	int ret = 0;
	if (!data->internal.memcpy_started) {
		LOG(13, "pbp %p lane %u count %zu off %" PRIu64, pbp, lane,
				count, off);

		if (off + count > pbp->datasize) {
			ERR("offset + count (%zu) past end of data area (%zu)",
					(size_t)off + count, pbp->datasize);
			errno = EINVAL;
			ret = -1;
			goto set_output;
		}

		/* XXX: use pmem2_mem* operation instead of vdm_operation */
		int eADR = pmem_has_auto_flush();
		int durable = vdm_is_supported(vdm, VDM_F_MEM_DURABLE);

		int vdm_flags;
		int needs_flushing;
		if (eADR) {
			/* eADR can take care of persistence */
			vdm_flags = 0;
			needs_flushing = 0;
		} else if (pbp->is_pmem && durable) {
			/* Data mover engine can take care of persistence */
			vdm_flags = VDM_F_MEM_DURABLE;
			needs_flushing = 0;
		} else {
			/* We need to persist the write manually */
			vdm_flags = 0;
			needs_flushing = 1;
		}

		data->internal.needs_flushing = needs_flushing;
		data->internal.memcpy_fut =
				vdm_memcpy(vdm, dest, buf, count, vdm_flags);
		data->internal.memcpy_started = 1;
	}

	if (future_poll(FUTURE_AS_RUNNABLE(&data->internal.memcpy_fut), NULL) !=
			FUTURE_STATE_COMPLETE) {
		return FUTURE_STATE_RUNNING;
	}

	if (data->internal.needs_flushing) {
		if (pbp->is_pmem) {
			pmem_drain();
		} else {
			pmem_msync(dest, count);
		}
	}

	ret = 0;

set_output:
	output->return_value = ret;

	return FUTURE_STATE_COMPLETE;
}

static struct nswrite_async_future
nswrite_async(void *ns, unsigned lane, void *buf, size_t count, uint64_t off,
		struct vdm *vdm)
{
	struct nswrite_async_future future = {
		.data.ns = ns,
		.data.lane = lane,
		.data.buf = buf,
		.data.count = count,
		.data.off = off,
		.data.vdm = vdm,
		.data.internal.memcpy_started = 0,

		.output.return_value = -1,
	};

	FUTURE_INIT(&future, nswrite_async_impl);

	return future;
}
/*
 * END of nswrite_async_future
 */

/*
 * START of the pmemblk_read_async future
 */
static enum future_state
pmemblk_read_async_impl(struct future_context *ctx,
	struct future_notifier *notifier)
{
	struct pmemblk_read_async_data *data =
		future_context_get_data(ctx);
	struct pmemblk_read_async_output *output =
		future_context_get_output(ctx);

	PMEMblkpool *pbp = data->pbp;
	void *buf = data->buf;
	long long blockno = data->blockno;
	int *stage = &data->stage;

	if (*stage <= PMEMBLK_READ_WAITING_FOR_LANE) {
		if (lane_try_enter(pbp, &data->internal.lane) == -1) {
			*stage = PMEMBLK_READ_WAITING_FOR_LANE;
			return FUTURE_STATE_RUNNING;
		}
		data->internal.btt_read_fut = btt_read_async(pbp->bttp,
			data->internal.lane, blockno, buf, pbp->vdm, stage);
	}

	/*
	 * XXX: The lane lock is kept even when 'pmemblk_write_async_future'
	 * cannot make progress.
	 */
	if (future_poll(
		FUTURE_AS_RUNNABLE(&data->internal.btt_read_fut), NULL)
			!= FUTURE_STATE_COMPLETE) {
		return FUTURE_STATE_RUNNING;
	}

	lane_exit(pbp, data->internal.lane);
	output->return_value = 0;
	*stage = PMEMBLK_READ_COMPLETE;

	return FUTURE_STATE_COMPLETE;
}

struct pmemblk_read_async_future
pmemblk_read_async(PMEMblkpool *pbp, void *buf, long long blockno)
{
	struct pmemblk_read_async_future future = {
		.data.pbp = pbp,
		.data.buf = buf,
		.data.blockno = blockno,
		.data.internal.lane = -1,
		.data.stage = PMEMBLK_READ_INITIALIZED,

		.output.return_value = -1,
	};

	FUTURE_INIT(&future, pmemblk_read_async_impl);

	return future;
}
/*
 * END of the pmemblk_read_async future
 */

/*
 * START of the pmemblk_write_async_future future
 */
static enum future_state
pmemblk_write_async_impl(struct future_context *ctx,
		struct future_notifier *notifier)
{
	struct pmemblk_write_async_data *data =
		future_context_get_data(ctx);
	struct pmemblk_write_async_output *output =
		future_context_get_output(ctx);

	PMEMblkpool *pbp = data->pbp;
	void *buf = data->buf;
	long long blockno = data->blockno;
	int *stage = &data->stage;

	int ret;
	if (*stage <= PMEMBLK_WRITE_WAITING_FOR_LANE) {
		if (pbp->rdonly) {
			ERR("EROFS (pool is read-only)");
			errno = EROFS;
			ret = -1;
			goto set_output;
		}

		if (blockno < 0) {
			ERR("negative block number");
			errno = EINVAL;
			ret = -1;
			goto set_output;
		}

		if (lane_try_enter(pbp, &data->internal.lane) == -1) {
			*stage = PMEMBLK_READ_WAITING_FOR_LANE;
			return FUTURE_STATE_RUNNING;
		}
		data->internal.btt_write_fut = btt_write_async(pbp->bttp,
			data->internal.lane, blockno, buf, pbp->vdm, stage);
	}

	/*
	 * XXX: The lane lock is kept even when 'pmemblk_write_async_future'
	 * cannot make progress.
	 */
	if (future_poll(
		FUTURE_AS_RUNNABLE(&data->internal.btt_write_fut), NULL)
			!= FUTURE_STATE_COMPLETE) {
		return FUTURE_STATE_RUNNING;
	}

	lane_exit(pbp, data->internal.lane);
	ret = data->internal.btt_write_fut.output.return_value;
	*stage = PMEMBLK_WRITE_COMPLETE;

set_output:
	output->return_value = ret;

	return FUTURE_STATE_COMPLETE;
}

struct pmemblk_write_async_future
pmemblk_write_async(PMEMblkpool *pbp, void *buf, long long blockno)
{
	struct pmemblk_write_async_future future = {
		.data.pbp = pbp,
		.data.buf = buf,
		.data.blockno = blockno,
		.data.internal.lane = -1,
		.data.stage = PMEMBLK_WRITE_INITIALIZED,

		.output.return_value = -1,
	};

	FUTURE_INIT(&future, pmemblk_write_async_impl);

	return future;
}
/*
 * END of the pmemblk_write_async_future future
 */

/* async callbacks for btt_init() */
static struct ns_callback_async ns_cb_async = {
	.nsread = nsread_async,
	.nswrite = nswrite_async,
	.nszero = NULL,
	.nsmap = NULL,
	.nssync = NULL,
	.ns_is_zeroed = 0
};
#else
static struct ns_callback_async ns_cb_async;
#endif

/*
 * blk_descr_create -- (internal) create block memory pool descriptor
 */
static void
blk_descr_create(PMEMblkpool *pbp, uint32_t bsize, int zeroed)
{
	LOG(3, "pbp %p bsize %u zeroed %d", pbp, bsize, zeroed);

	/* create the required metadata */
	pbp->bsize = htole32(bsize);
	util_persist(pbp->is_pmem, &pbp->bsize, sizeof(bsize));

	pbp->is_zeroed = zeroed;
	util_persist(pbp->is_pmem, &pbp->is_zeroed, sizeof(pbp->is_zeroed));
}

/*
 * blk_descr_check -- (internal) validate block memory pool descriptor
 */
static int
blk_descr_check(PMEMblkpool *pbp, size_t *bsize)
{
	LOG(3, "pbp %p bsize %zu", pbp, *bsize);

	size_t hdr_bsize = le32toh(pbp->bsize);
	if (*bsize && *bsize != hdr_bsize) {
		ERR("wrong bsize (%zu), pool created with bsize %zu",
				*bsize, hdr_bsize);
		errno = EINVAL;
		return -1;
	}
	*bsize = hdr_bsize;
	LOG(3, "using block size from header: %zu", *bsize);

	return 0;
}

/*
 * blk_runtime_init -- (internal) initialize block memory pool runtime data
 */
static int
blk_runtime_init(PMEMblkpool *pbp, size_t bsize, int rdonly)
{
	LOG(3, "pbp %p bsize %zu rdonly %d",
			pbp, bsize, rdonly);

	/* remove volatile part of header */
	VALGRIND_REMOVE_PMEM_MAPPING(&pbp->addr,
			sizeof(struct pmemblk) -
			sizeof(struct pool_hdr) -
			sizeof(pbp->bsize) -
			sizeof(pbp->is_zeroed));

	/*
	 * Use some of the memory pool area for run-time info.  This
	 * run-time state is never loaded from the file, it is always
	 * created here, so no need to worry about byte-order.
	 */
	pbp->rdonly = rdonly;
	pbp->data = (char *)pbp->addr +
			roundup(sizeof(*pbp), BLK_FORMAT_DATA_ALIGN);
	ASSERT(((char *)pbp->addr + pbp->size) >= (char *)pbp->data);
	pbp->datasize = (size_t)
			(((char *)pbp->addr + pbp->size) - (char *)pbp->data);

	LOG(4, "data area %p data size %zu bsize %zu",
		pbp->data, pbp->datasize, bsize);

	long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (ncpus < 1)
		ncpus = 1;

	ns_cb.ns_is_zeroed = pbp->is_zeroed;

	/* things free by "goto err" if not NULL */
	struct btt *bttp = NULL;
	os_mutex_t *locks = NULL;

	bttp = btt_init(pbp->datasize, (uint32_t)bsize, pbp->hdr.poolset_uuid,
			(unsigned)ncpus * 2, pbp, &ns_cb, &ns_cb_async);

	if (bttp == NULL)
		goto err;	/* btt_init set errno, called LOG */

	pbp->bttp = bttp;

	pbp->nlane = btt_nlane(pbp->bttp);
	pbp->next_lane = 0;
	if ((locks = Malloc(pbp->nlane * sizeof(*locks))) == NULL) {
		ERR("!Malloc for lane locks");
		goto err;
	}

	for (unsigned i = 0; i < pbp->nlane; i++)
		util_mutex_init(&locks[i]);

	pbp->locks = locks;

	/*
	 * If possible, turn off all permissions on the pool header page.
	 *
	 * The prototype PMFS doesn't allow this when large pages are in
	 * use. It is not considered an error if this fails.
	 */
	RANGE_NONE(pbp->addr, sizeof(struct pool_hdr), pbp->is_dev_dax);

	return 0;

err:
	LOG(4, "error clean up");
	int oerrno = errno;
	if (bttp)
		btt_fini(bttp);
	errno = oerrno;
	return -1;
}

/*
 * pmemblk_createU -- create a block memory pool
 */
#ifndef _WIN32
static inline
#endif
PMEMblkpool *
pmemblk_createU(const char *path, size_t bsize, size_t poolsize, mode_t mode)
{
	LOG(3, "path %s bsize %zu poolsize %zu mode %o",
			path, bsize, poolsize, mode);

	/* check if bsize is valid */
	if (bsize == 0) {
		ERR("Invalid block size %zu", bsize);
		errno = EINVAL;
		return NULL;
	}

	if (bsize > UINT32_MAX) {
		ERR("Invalid block size %zu", bsize);
		errno = EINVAL;
		return NULL;
	}

	struct pool_set *set;
	struct pool_attr adj_pool_attr = Blk_create_attr;

	/* force set SDS feature */
	if (SDS_at_create)
		adj_pool_attr.features.incompat |= POOL_FEAT_SDS;
	else
		adj_pool_attr.features.incompat &= ~POOL_FEAT_SDS;

	if (util_pool_create(&set, path, poolsize, PMEMBLK_MIN_POOL,
			PMEMBLK_MIN_PART, &adj_pool_attr, NULL,
			REPLICAS_DISABLED) != 0) {
		LOG(2, "cannot create pool or pool set");
		return NULL;
	}

	ASSERT(set->nreplicas > 0);

	struct pool_replica *rep = set->replica[0];
	PMEMblkpool *pbp = rep->part[0].addr;

	VALGRIND_REMOVE_PMEM_MAPPING(&pbp->addr,
			sizeof(struct pmemblk) -
			((uintptr_t)&pbp->addr - (uintptr_t)&pbp->hdr));

	pbp->addr = pbp;
	pbp->size = rep->repsize;
	pbp->set = set;
	pbp->is_pmem = rep->is_pmem;
	pbp->is_dev_dax = rep->part[0].is_dev_dax;

	/* is_dev_dax implies is_pmem */
	ASSERT(!pbp->is_dev_dax || pbp->is_pmem);

	/* create pool descriptor */
	blk_descr_create(pbp, (uint32_t)bsize, set->zeroed);

	/* initialize runtime parts */
	if (blk_runtime_init(pbp, bsize, 0) != 0) {
		ERR("pool initialization failed");
		goto err;
	}

	if (util_poolset_chmod(set, mode))
		goto err;

	util_poolset_fdclose(set);

	LOG(3, "pbp %p", pbp);
	return pbp;

err:
	LOG(4, "error clean up");
	int oerrno = errno;
	util_poolset_close(set, DELETE_CREATED_PARTS);
	errno = oerrno;
	return NULL;
}

/*
 * pmemblk_xcreateU -- create a block memory pool that uses provided data mover
 *                     for asynchronus memory movement operations
 */
#ifndef _WIN32
static inline
#endif
PMEMblkpool *
pmemblk_xcreateU(const char *path, size_t bsize, size_t poolsize, mode_t mode,
		struct vdm *vdm)
{
	PMEMblkpool *pbp = pmemblk_createU(path, bsize, poolsize, mode);
	if (!pbp)
		return pbp;

	/*
	 * XXX: Create default blk data mover to be used in case provided
	 * vdm is NULL.
	 */
	LOG(3, "vdm %p", vdm);
	pbp->vdm = vdm ? vdm : NULL;

	return pbp;
}

#ifndef _WIN32
/*
 * pmemblk_create -- create a block memory pool
 */
PMEMblkpool *
pmemblk_create(const char *path, size_t bsize, size_t poolsize, mode_t mode)
{
	return pmemblk_createU(path, bsize, poolsize, mode);
}

/*
 * pmemblk_xcreate -- create a block memory pool that uses provided data mover
 *                    for asynchronus memory movement operations
 */
PMEMblkpool *
pmemblk_xcreate(const char *path, size_t bsize, size_t poolsize, mode_t mode,
		struct vdm *vdm)
{
	return pmemblk_xcreateU(path, bsize, poolsize, mode, vdm);
}
#else
/*
 * pmemblk_createW -- create a block memory pool
 */
PMEMblkpool *
pmemblk_createW(const wchar_t *path, size_t bsize, size_t poolsize,
	mode_t mode)
{
	char *upath = util_toUTF8(path);
	if (upath == NULL)
		return NULL;

	PMEMblkpool *ret = pmemblk_createU(upath, bsize, poolsize, mode);

	util_free_UTF8(upath);
	return ret;
}

/*
 * pmemblk_xcreateW -- create a block memory pool that uses provided data mover
 *                     for asynchronus memory movement operations
 */
PMEMblkpool *
pmemblk_xcreateW(const wchar_t *path, size_t bsize, size_t poolsize,
		mode_t mode, struct vdm *vdm)
{
	char *upath = util_toUTF8(path);
	if (upath == NULL)
		return NULL;

	PMEMblkpool *ret = pmemblk_xcreateU(upath, bsize, poolsize, mode, vdm);

	util_free_UTF8(upath);
	return ret;
}
#endif

/*
 * blk_open_common -- (internal) open a block memory pool
 *
 * This routine does all the work, but takes a cow flag so internal
 * calls can map a read-only pool if required.
 *
 * Passing in bsize == 0 means a valid pool header must exist (which
 * will supply the block size).
 */
static PMEMblkpool *
blk_open_common(const char *path, size_t bsize, unsigned flags)
{
	LOG(3, "path %s bsize %zu flags 0x%x", path, bsize, flags);

	struct pool_set *set;

	if (util_pool_open(&set, path, PMEMBLK_MIN_PART, &Blk_open_attr,
			NULL, NULL, flags) != 0) {
		LOG(2, "cannot open pool or pool set");
		return NULL;
	}

	ASSERT(set->nreplicas > 0);

	struct pool_replica *rep = set->replica[0];
	PMEMblkpool *pbp = rep->part[0].addr;

	VALGRIND_REMOVE_PMEM_MAPPING(&pbp->addr,
			sizeof(struct pmemblk) -
			((uintptr_t)&pbp->addr - (uintptr_t)&pbp->hdr));

	pbp->addr = pbp;
	pbp->size = rep->repsize;
	pbp->set = set;
	pbp->is_pmem = rep->is_pmem;
	pbp->is_dev_dax = rep->part[0].is_dev_dax;

	/* is_dev_dax implies is_pmem */
	ASSERT(!pbp->is_dev_dax || pbp->is_pmem);

	if (set->nreplicas > 1) {
		errno = ENOTSUP;
		ERR("!replicas not supported");
		goto err;
	}

	/* validate pool descriptor */
	if (blk_descr_check(pbp, &bsize) != 0) {
		LOG(2, "descriptor check failed");
		goto err;
	}

	/* initialize runtime parts */
	if (blk_runtime_init(pbp, bsize, set->rdonly) != 0) {
		ERR("pool initialization failed");
		goto err;
	}

	util_poolset_fdclose(set);

	LOG(3, "pbp %p", pbp);
	return pbp;

err:
	LOG(4, "error clean up");
	int oerrno = errno;
	util_poolset_close(set, DO_NOT_DELETE_PARTS);
	errno = oerrno;
	return NULL;
}

/*
 * pmemblk_openU -- open a block memory pool
 */
#ifndef _WIN32
static inline
#endif
PMEMblkpool *
pmemblk_openU(const char *path, size_t bsize)
{
	LOG(3, "path %s bsize %zu", path, bsize);

	return blk_open_common(path, bsize, COW_at_open ? POOL_OPEN_COW : 0);
}

/*
 * pmemblk_xopenU -- open a block memory pool that uses provided data mover for
 *                   asynchronus memory movement operations
 */
#ifndef _WIN32
static inline
#endif
PMEMblkpool *
pmemblk_xopenU(const char *path, size_t bsize, struct vdm *vdm)
{
	PMEMblkpool *pbp = pmemblk_openU(path, bsize);
	if (!pbp)
		return pbp;

	/*
	 * XXX: Create default blk data mover to be used in case provided
	 * vdm is NULL.
	 */
	LOG(3, "vdm %p", vdm);
	pbp->vdm = vdm ? vdm : NULL;

	return pbp;
}

#ifndef _WIN32
/*
 * pmemblk_open -- open a block memory pool
 */
PMEMblkpool *
pmemblk_open(const char *path, size_t bsize)
{
	return pmemblk_openU(path, bsize);
}

/*
 * pmemblk_xopen -- open a block memory pool that uses provided data mover
 *                  for asynchronus memory movement operations
 */
PMEMblkpool *
pmemblk_xopen(const char *path, size_t bsize, struct vdm *vdm)
{
	return pmemblk_xopenU(path, bsize, vdm);
}
#else
/*
 * pmemblk_openW -- open a block memory pool
 */
PMEMblkpool *
pmemblk_openW(const wchar_t *path, size_t bsize)
{
	char *upath = util_toUTF8(path);
	if (upath == NULL)
		return NULL;

	PMEMblkpool *ret = pmemblk_openU(upath, bsize);

	util_free_UTF8(upath);
	return ret;
}

/*
 * pmemblk_xopenW -- open a block memory pool that uses provided data mover
 *                   for asynchronus memory movement operations
 */
PMEMblkpool *
pmemblk_xopenW(const wchar_t *path, size_t bsize, struct vdm *vdm)
{
	char *upath = util_toUTF8(path);
	if (upath == NULL)
		return NULL;

	PMEMblkpool *ret = pmemblk_xopenU(upath, bsize, vdm);

	util_free_UTF8(upath);
	return ret;
}
#endif

/*
 * pmemblk_close -- close a block memory pool
 */
void
pmemblk_close(PMEMblkpool *pbp)
{
	LOG(3, "pbp %p", pbp);

	btt_fini(pbp->bttp);
	if (pbp->locks) {
		for (unsigned i = 0; i < pbp->nlane; i++)
			util_mutex_destroy(&pbp->locks[i]);
		Free((void *)pbp->locks);
	}

	util_poolset_close(pbp->set, DO_NOT_DELETE_PARTS);
}

/*
 * pmemblk_bsize -- return size of block for specified pool
 */
size_t
pmemblk_bsize(PMEMblkpool *pbp)
{
	LOG(3, "pbp %p", pbp);

	return le32toh(pbp->bsize);
}

/*
 * pmemblk_nblock -- return number of usable blocks in a block memory pool
 */
size_t
pmemblk_nblock(PMEMblkpool *pbp)
{
	LOG(3, "pbp %p", pbp);

	return btt_nlba(pbp->bttp);
}

/*
 * pmemblk_read -- read a block in a block memory pool
 */
int
pmemblk_read(PMEMblkpool *pbp, void *buf, long long blockno)
{
	LOG(3, "pbp %p buf %p blockno %lld", pbp, buf, blockno);

	if (blockno < 0) {
		ERR("negative block number");
		errno = EINVAL;
		return -1;
	}

	unsigned lane;

	lane_enter(pbp, &lane);

	int err = btt_read(pbp->bttp, lane, (uint64_t)blockno, buf);

	lane_exit(pbp, lane);

	return err;
}

/*
 * pmemblk_write -- write a block (atomically) in a block memory pool
 */
int
pmemblk_write(PMEMblkpool *pbp, const void *buf, long long blockno)
{
	LOG(3, "pbp %p buf %p blockno %lld", pbp, buf, blockno);

	if (pbp->rdonly) {
		ERR("EROFS (pool is read-only)");
		errno = EROFS;
		return -1;
	}

	if (blockno < 0) {
		ERR("negative block number");
		errno = EINVAL;
		return -1;
	}

	unsigned lane;

	lane_enter(pbp, &lane);

	int err = btt_write(pbp->bttp, lane, (uint64_t)blockno, buf);

	lane_exit(pbp, lane);

	return err;
}

/*
 * pmemblk_set_zero -- zero a block in a block memory pool
 */
int
pmemblk_set_zero(PMEMblkpool *pbp, long long blockno)
{
	LOG(3, "pbp %p blockno %lld", pbp, blockno);

	if (pbp->rdonly) {
		ERR("EROFS (pool is read-only)");
		errno = EROFS;
		return -1;
	}

	if (blockno < 0) {
		ERR("negative block number");
		errno = EINVAL;
		return -1;
	}

	unsigned lane;

	lane_enter(pbp, &lane);

	int err = btt_set_zero(pbp->bttp, lane, (uint64_t)blockno);

	lane_exit(pbp, lane);

	return err;
}

/*
 * pmemblk_set_error -- set the error state on a block in a block memory pool
 */
int
pmemblk_set_error(PMEMblkpool *pbp, long long blockno)
{
	LOG(3, "pbp %p blockno %lld", pbp, blockno);

	if (pbp->rdonly) {
		ERR("EROFS (pool is read-only)");
		errno = EROFS;
		return -1;
	}

	if (blockno < 0) {
		ERR("negative block number");
		errno = EINVAL;
		return -1;
	}

	unsigned lane;

	lane_enter(pbp, &lane);

	int err = btt_set_error(pbp->bttp, lane, (uint64_t)blockno);

	lane_exit(pbp, lane);

	return err;
}

/*
 * pmemblk_checkU -- block memory pool consistency check
 */
#ifndef _WIN32
static inline
#endif
int
pmemblk_checkU(const char *path, size_t bsize)
{
	LOG(3, "path \"%s\" bsize %zu", path, bsize);

	/* map the pool read-only */
	PMEMblkpool *pbp = blk_open_common(path, bsize, POOL_OPEN_COW);
	if (pbp == NULL)
		return -1;	/* errno set by blk_open_common() */

	int retval = btt_check(pbp->bttp);
	int oerrno = errno;
	pmemblk_close(pbp);
	errno = oerrno;

	return retval;
}

#ifndef _WIN32
/*
 * pmemblk_check -- block memory pool consistency check
 */
int
pmemblk_check(const char *path, size_t bsize)
{
	return pmemblk_checkU(path, bsize);
}
#else
/*
 * pmemblk_checkW -- block memory pool consistency check
 */
int
pmemblk_checkW(const wchar_t *path, size_t bsize)
{
	char *upath = util_toUTF8(path);
	if (upath == NULL)
		return -1;

	int ret = pmemblk_checkU(upath, bsize);

	util_free_UTF8(upath);
	return ret;
}
#endif

/*
 * pmemblk_ctl_getU -- programmatically executes a read ctl query
 */
#ifndef _WIN32
static inline
#endif
int
pmemblk_ctl_getU(PMEMblkpool *pbp, const char *name, void *arg)
{
	LOG(3, "pbp %p name %s arg %p", pbp, name, arg);
	return ctl_query(pbp == NULL ? NULL : pbp->ctl, pbp,
			CTL_QUERY_PROGRAMMATIC, name, CTL_QUERY_READ, arg);
}

/*
 * pmemblk_ctl_setU -- programmatically executes a write ctl query
 */
#ifndef _WIN32
static inline
#endif
int
pmemblk_ctl_setU(PMEMblkpool *pbp, const char *name, void *arg)
{
	LOG(3, "pbp %p name %s arg %p", pbp, name, arg);
	return ctl_query(pbp == NULL ? NULL : pbp->ctl, pbp,
		CTL_QUERY_PROGRAMMATIC, name, CTL_QUERY_WRITE, arg);
}

/*
 * pmemblk_ctl_execU -- programmatically executes a runnable ctl query
 */
#ifndef _WIN32
static inline
#endif
int
pmemblk_ctl_execU(PMEMblkpool *pbp, const char *name, void *arg)
{
	LOG(3, "pbp %p name %s arg %p", pbp, name, arg);
	return ctl_query(pbp == NULL ? NULL : pbp->ctl, pbp,
		CTL_QUERY_PROGRAMMATIC, name, CTL_QUERY_RUNNABLE, arg);
}

#ifndef _WIN32
/*
 * pmemblk_ctl_get -- programmatically executes a read ctl query
 */
int
pmemblk_ctl_get(PMEMblkpool *pbp, const char *name, void *arg)
{
	return pmemblk_ctl_getU(pbp, name, arg);
}

/*
 * pmemblk_ctl_set -- programmatically executes a write ctl query
 */
int
pmemblk_ctl_set(PMEMblkpool *pbp, const char *name, void *arg)
{
	return pmemblk_ctl_setU(pbp, name, arg);
}

/*
 * pmemblk_ctl_exec -- programmatically executes a runnable ctl query
 */
int
pmemblk_ctl_exec(PMEMblkpool *pbp, const char *name, void *arg)
{
	return pmemblk_ctl_execU(pbp, name, arg);
}
#else
/*
 * pmemblk_ctl_getW -- programmatically executes a read ctl query
 */
int
pmemblk_ctl_getW(PMEMblkpool *pbp, const wchar_t *name, void *arg)
{
	char *uname = util_toUTF8(name);
	if (uname == NULL)
		return -1;

	int ret = pmemblk_ctl_getU(pbp, uname, arg);
	util_free_UTF8(uname);

	return ret;
}

/*
 * pmemblk_ctl_setW -- programmatically executes a write ctl query
 */
int
pmemblk_ctl_setW(PMEMblkpool *pbp, const wchar_t *name, void *arg)
{
	char *uname = util_toUTF8(name);
	if (uname == NULL)
		return -1;

	int ret = pmemblk_ctl_setU(pbp, uname, arg);
	util_free_UTF8(uname);

	return ret;
}

/*
 * pmemblk_ctl_execW -- programmatically executes a runnable ctl query
 */
int
pmemblk_ctl_execW(PMEMblkpool *pbp, const wchar_t *name, void *arg)
{
	char *uname = util_toUTF8(name);
	if (uname == NULL)
		return -1;

	int ret = pmemblk_ctl_execU(pbp, uname, arg);
	util_free_UTF8(uname);

	return ret;
}
#endif

#if FAULT_INJECTION
void
pmemblk_inject_fault_at(enum pmem_allocation_type type, int nth,
							const char *at)
{
	core_inject_fault_at(type, nth, at);
}

int
pmemblk_fault_injection_enabled(void)
{
	return core_fault_injection_enabled();
}
#endif
