/*
 * Copyright (c) 2014-2015, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY LOG OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * blk.c -- block memory pool entry points for libpmem
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <uuid/uuid.h>
#include <pthread.h>
#include <libpmem.h>
#include <libpmemblk.h>
#include "util.h"
#include "out.h"
#include "btt.h"
#include "blk.h"

/*
 * lane_enter -- (internal) acquire a unique lane number
 */
static int
lane_enter(PMEMblkpool *pbp)
{
	int mylane;

	mylane = __sync_fetch_and_add(&pbp->next_lane, 1) % pbp->nlane;

	/* lane selected, grab the per-lane lock */
	if ((errno = pthread_mutex_lock(&pbp->locks[mylane]))) {
		LOG(1, "!pthread_mutex_lock");
		return -1;
	}

	return mylane;
}

/*
 * lane_exit -- (internal) drop lane lock
 */
static void
lane_exit(PMEMblkpool *pbp, int mylane)
{
	int oerrno = errno;
	if ((errno = pthread_mutex_unlock(&pbp->locks[mylane])))
		LOG(1, "!pthread_mutex_unlock");
	errno = oerrno;
}

/*
 * nsread -- (internal) read data from the namespace encapsulating the BTT
 *
 * This routine is provided to btt_init() to allow the btt module to
 * do I/O on the memory pool containing the BTT layout.
 */
static int
nsread(void *ns, int lane, void *buf, size_t count, off_t off)
{
	struct pmemblk *pbp = (struct pmemblk *)ns;

	LOG(13, "pbp %p lane %d count %zu off %lld",
			pbp, lane, count, (long long)off);

	if (off + count > pbp->datasize) {
		LOG(1, "offset + count (%lld) past end of data area (%zu)",
				(long long)off + count, pbp->datasize);
		errno = EINVAL;
		return -1;
	}

	memcpy(buf, pbp->data + off, count);

	return 0;
}

/*
 * nswrite -- (internal) write data to the namespace encapsulating the BTT
 *
 * This routine is provided to btt_init() to allow the btt module to
 * do I/O on the memory pool containing the BTT layout.
 */
static int
nswrite(void *ns, int lane, const void *buf, size_t count, off_t off)
{
	struct pmemblk *pbp = (struct pmemblk *)ns;

	LOG(13, "pbp %p lane %d count %zu off %lld",
			pbp, lane, count, (long long)off);

	if (off + count > pbp->datasize) {
		LOG(1, "offset + count (%lld) past end of data area (%zu)",
				(long long)off + count, pbp->datasize);
		errno = EINVAL;
		return -1;
	}

	void *dest = pbp->data + off;

#ifdef DEBUG
	/* grab debug write lock */
	if ((errno = pthread_mutex_lock(&pbp->write_lock))) {
		LOG(1, "!pthread_mutex_lock");
		return -1;
	}
#endif

	/* unprotect the memory (debug version only) */
	RANGE_RW(dest, count);

	memcpy(dest, buf, count);

	/* protect the memory again (debug version only) */
	RANGE_RO(dest, count);

#ifdef DEBUG
	/* release debug write lock */
	if ((errno = pthread_mutex_unlock(&pbp->write_lock)))
		LOG(1, "!pthread_mutex_unlock");
#endif

	if (pbp->is_pmem)
		pmem_persist(dest, count);
	else
		pmem_msync(dest, count);

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
nsmap(void *ns, int lane, void **addrp, size_t len, off_t off)
{
	struct pmemblk *pbp = (struct pmemblk *)ns;

	LOG(12, "pbp %p lane %d len %zu off %lld",
			pbp, lane, len, (long long)off);

	if (off + len >= pbp->datasize) {
		LOG(1, "offset + len (%lld) past end of data area (%zu)",
				(long long)off + len, pbp->datasize - 1);
		errno = EINVAL;
		return -1;
	}

	/*
	 * Since the entire file is memory-mapped, this callback
	 * can always provide the entire length requested.
	 */
	*addrp = pbp->data + off;

	LOG(12, "returning addr %p", *addrp);

	return len;
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
nssync(void *ns, int lane, void *addr, size_t len)
{
	struct pmemblk *pbp = (struct pmemblk *)ns;

	LOG(12, "pbp %p lane %d addr %p len %zu", pbp, lane, addr, len);

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
nszero(void *ns, int lane, size_t count, off_t off)
{
	struct pmemblk *pbp = (struct pmemblk *)ns;

	LOG(13, "pbp %p lane %d count %zu off %lld",
			pbp, lane, count, (long long)off);

	if (off + count > pbp->datasize) {
		LOG(1, "offset + count (%lld) past end of data area (%zu)",
				(long long)off + count, pbp->datasize);
		errno = EINVAL;
		return -1;
	}

	void *dest = pbp->data + off;

	/* unprotect the memory (debug version only) */
	RANGE_RW(dest, count);

	pmem_memset_persist(dest, 0, count);

	/* protect the memory again (debug version only) */
	RANGE_RO(dest, count);

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

/*
 * pmemblk_map_common -- (internal) map a block memory pool
 *
 * This routine does all the work, but takes a rdonly flag so internal
 * calls can map a read-only pool if required.
 *
 * If empty flag is set, the file is assumed to be a new memory pool, and
 * a new pool header is created.  Otherwise, a valid pool header must exist.
 *
 * Passing in bsize == 0 means a valid pool header must exist (which
 * will supply the block size).
 */
static PMEMblkpool *
pmemblk_map_common(int fd, size_t poolsize, size_t bsize, int rdonly,
		int initialize, int zeroed)
{
	LOG(3, "fd %d poolsize %zu bsize %zu rdonly %d initialize %d zeroed %d",
			fd, poolsize, bsize, rdonly, initialize, zeroed);

	/* things free by "goto err" if not NULL */
	struct btt *bttp = NULL;
	pthread_mutex_t *locks = NULL;

	void *addr;
	if ((addr = util_map(fd, poolsize, rdonly)) == NULL) {
		(void) close(fd);
		return NULL;	/* util_map() set errno, called LOG */
	}

	(void) close(fd);

	/* check if the mapped region is located in persistent memory */
	int is_pmem = pmem_is_pmem(addr, poolsize);

	/* opaque info lives at the beginning of mapped memory pool */
	struct pmemblk *pbp = addr;

	if (!initialize) {
		struct pool_hdr hdr;

		memcpy(&hdr, &pbp->hdr, sizeof (hdr));

		if (!util_convert_hdr(&hdr)) {
			errno = EINVAL;
			goto err;
		}

		/*
		 * valid header found
		 */
		if (strncmp(hdr.signature, BLK_HDR_SIG, POOL_HDR_SIG_LEN)) {
			LOG(1, "wrong pool type: \"%s\"", hdr.signature);

			errno = EINVAL;
			goto err;
		}

		if (hdr.major != BLK_FORMAT_MAJOR) {
			LOG(1, "blk pool version %d (library expects %d)",
				hdr.major, BLK_FORMAT_MAJOR);

			errno = EINVAL;
			goto err;
		}

		size_t hdr_bsize = le32toh(pbp->bsize);
		if (bsize && bsize != hdr_bsize) {
			LOG(1, "wrong bsize (%zu), pool created with bsize %zu",
					bsize, hdr_bsize);
			errno = EINVAL;
			goto err;
		}
		bsize = hdr_bsize;
		LOG(3, "using block size from header: %zu", bsize);

		int retval = util_feature_check(&hdr, BLK_FORMAT_INCOMPAT,
							BLK_FORMAT_RO_COMPAT,
							BLK_FORMAT_COMPAT);
		if (retval < 0)
		    goto err;
		else if (retval == 0)
		    rdonly = 1;
	} else {
		LOG(3, "creating new blk memory pool");

		ASSERTeq(rdonly, 0);

		struct pool_hdr *hdrp = &pbp->hdr;

		/* check if the pool header is all zero */
		if (!util_is_zeroed(hdrp, sizeof (*hdrp))) {
			LOG(1, "Non-zero pool header detected");
			errno = EINVAL;
			goto err;
		}

		/* check if bsize is valid */
		if (bsize == 0) {
			LOG(1, "Invalid block size %zu", bsize);
			errno = EINVAL;
			goto err;
		}

		/* create the required metadata first */
		pbp->bsize = htole32(bsize);
		pmem_msync(&pbp->bsize, sizeof (bsize));

		pbp->is_zeroed = zeroed;
		pmem_msync(&pbp->is_zeroed, sizeof (pbp->is_zeroed));

		/* create pool header */
		strncpy(hdrp->signature, BLK_HDR_SIG, POOL_HDR_SIG_LEN);
		hdrp->major = htole32(BLK_FORMAT_MAJOR);
		hdrp->compat_features = htole32(BLK_FORMAT_COMPAT);
		hdrp->incompat_features = htole32(BLK_FORMAT_INCOMPAT);
		hdrp->ro_compat_features = htole32(BLK_FORMAT_RO_COMPAT);
		uuid_generate(hdrp->uuid);
		hdrp->crtime = htole64((uint64_t)time(NULL));
		util_checksum(hdrp, sizeof (*hdrp), &hdrp->checksum, 1);

		/* store pool's header */
		pmem_msync(hdrp, sizeof (*hdrp));
	}

	/*
	 * Use some of the memory pool area for run-time info.  This
	 * run-time state is never loaded from the file, it is always
	 * created here, so no need to worry about byte-order.
	 */
	pbp->addr = addr;
	pbp->size = poolsize;
	pbp->rdonly = rdonly;
	pbp->is_pmem = is_pmem;
	pbp->data = addr + roundup(sizeof (*pbp), BLK_FORMAT_DATA_ALIGN);
	pbp->datasize = (pbp->addr + pbp->size) - pbp->data;

	LOG(4, "data area %p data size %zu bsize %zu",
		pbp->data, pbp->datasize, bsize);

	int ncpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (ncpus < 1)
		ncpus = 1;

	ns_cb.ns_is_zeroed = pbp->is_zeroed;

	bttp = btt_init(pbp->datasize, (uint32_t)bsize, pbp->hdr.uuid,
			ncpus * 2, pbp, &ns_cb);

	if (bttp == NULL)
		goto err;	/* btt_init set errno, called LOG */

	pbp->bttp = bttp;

	pbp->nlane = btt_nlane(pbp->bttp);
	pbp->next_lane = 0;
	if ((locks = Malloc(pbp->nlane * sizeof (*locks))) == NULL) {
		LOG(1, "!Malloc for lane locks");
		goto err;
	}

	for (int i = 0; i < pbp->nlane; i++)
		if ((errno = pthread_mutex_init(&locks[i], NULL))) {
			LOG(1, "!pthread_mutex_init");
			goto err;
		}

	pbp->locks = locks;

#ifdef DEBUG
	/* initialize debug lock */
	if ((errno = pthread_mutex_init(&pbp->write_lock, NULL))) {
		LOG(1, "!pthread_mutex_init");
		goto err;
	}
#endif

	/*
	 * If possible, turn off all permissions on the pool header page.
	 *
	 * The prototype PMFS doesn't allow this when large pages are in
	 * use. It is not considered an error if this fails.
	 */
	util_range_none(addr, sizeof (struct pool_hdr));

	/* the data area should be kept read-only for debug version */
	RANGE_RO(pbp->data, pbp->datasize);

	LOG(3, "pbp %p", pbp);
	return pbp;

err:
	LOG(4, "error clean up");
	int oerrno = errno;
	if (locks)
		Free((void *)locks);
	if (bttp)
		btt_fini(bttp);
	util_unmap(addr, poolsize);
	errno = oerrno;
	return NULL;
}

/*
 * pmemblk_create -- create a block memory pool
 */
PMEMblkpool *
pmemblk_create(const char *path, size_t bsize, size_t poolsize,
		mode_t mode)
{
	LOG(3, "path %s bsize %zu poolsize %zu mode %d",
			path, bsize, poolsize, mode);

	int created = 0;
	int fd;
	if (poolsize != 0) {
		/* create a new memory pool file */
		fd = util_pool_create(path, poolsize, PMEMBLK_MIN_POOL, mode);
		created = 1;
	} else {
		/* open an existing file */
		fd = util_pool_open(path, &poolsize, PMEMBLK_MIN_POOL);
	}
	if (fd == -1)
		return NULL;	/* errno set by util_pool_create/open() */

	return pmemblk_map_common(fd, poolsize, bsize, 0, 1, created);
}

/*
 * pmemblk_open -- open a block memory pool
 */
PMEMblkpool *
pmemblk_open(const char *path, size_t bsize)
{
	LOG(3, "path %s bsize %zu", path, bsize);

	size_t poolsize = 0;
	int fd;

	if ((fd = util_pool_open(path, &poolsize, PMEMBLK_MIN_POOL)) == -1)
		return NULL;	/* errno set by util_pool_open() */

	return pmemblk_map_common(fd, poolsize, bsize, 0, 0, 0);
}

/*
 * pmemblk_close -- close a block memory pool
 */
void
pmemblk_close(PMEMblkpool *pbp)
{
	LOG(3, "pbp %p", pbp);

	btt_fini(pbp->bttp);
	if (pbp->locks) {
		for (int i = 0; i < pbp->nlane; i++)
			pthread_mutex_destroy(&pbp->locks[i]);
		Free((void *)pbp->locks);
	}

#ifdef DEBUG
	/* destroy debug lock */
	pthread_mutex_destroy(&pbp->write_lock);
#endif

	util_unmap(pbp->addr, pbp->size);
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
pmemblk_read(PMEMblkpool *pbp, void *buf, off_t blockno)
{
	LOG(3, "pbp %p buf %p blockno %lld", pbp, buf, (long long)blockno);

	int lane = lane_enter(pbp);

	if (lane < 0)
		return -1;

	int err = btt_read(pbp->bttp, lane, blockno, buf);

	lane_exit(pbp, lane);

	return err;
}

/*
 * pmemblk_write -- write a block (atomically) in a block memory pool
 */
int
pmemblk_write(PMEMblkpool *pbp, const void *buf, off_t blockno)
{
	LOG(3, "pbp %p buf %p blockno %lld", pbp, buf, (long long)blockno);

	if (pbp->rdonly) {
		LOG(1, "EROFS (pool is read-only)");
		errno = EROFS;
		return -1;
	}

	int lane = lane_enter(pbp);

	if (lane < 0)
		return -1;

	int err = btt_write(pbp->bttp, lane, blockno, buf);

	lane_exit(pbp, lane);

	return err;
}

/*
 * pmemblk_set_zero -- zero a block in a block memory pool
 */
int
pmemblk_set_zero(PMEMblkpool *pbp, off_t blockno)
{
	LOG(3, "pbp %p blockno %lld", pbp, (long long)blockno);

	if (pbp->rdonly) {
		LOG(1, "EROFS (pool is read-only)");
		errno = EROFS;
		return -1;
	}

	int lane = lane_enter(pbp);

	if (lane < 0)
		return -1;

	int err = btt_set_zero(pbp->bttp, lane, blockno);

	lane_exit(pbp, lane);

	return err;
}

/*
 * pmemblk_set_error -- set the error state on a block in a block memory pool
 */
int
pmemblk_set_error(PMEMblkpool *pbp, off_t blockno)
{
	LOG(3, "pbp %p blockno %lld", pbp, (long long)blockno);

	if (pbp->rdonly) {
		LOG(1, "EROFS (pool is read-only)");
		errno = EROFS;
		return -1;
	}

	int lane = lane_enter(pbp);

	if (lane < 0)
		return -1;

	int err = btt_set_error(pbp->bttp, lane, blockno);

	lane_exit(pbp, lane);

	return err;
}

/*
 * pmemblk_check -- block memory pool consistency check
 */
int
pmemblk_check(const char *path)
{
	LOG(3, "path \"%s\"", path);

	size_t poolsize = 0;
	int fd;

	if ((fd = util_pool_open(path, &poolsize, PMEMBLK_MIN_POOL)) == -1)
		return -1;	/* errno set by util_pool_open() */

	/* map the pool read-only */
	PMEMblkpool *pbp = pmemblk_map_common(fd, poolsize, 0, 1, 0, 0);

	if (pbp == NULL)
		return -1;	/* errno set by pmemblk_map_common() */

	int retval = btt_check(pbp->bttp);
	int oerrno = errno;
	pmemblk_close(pbp);
	errno = oerrno;

	return retval;
}
