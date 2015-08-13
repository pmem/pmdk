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
 * obj.c -- transactional object store implementation
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <uuid/uuid.h>
#include <time.h>
#include <endian.h>
#include <stdlib.h>
#include <setjmp.h>
#include <inttypes.h>

#include "libpmem.h"
#include "libpmemobj.h"

#include "util.h"
#include "out.h"
#include "lane.h"
#include "redo.h"
#include "list.h"
#include "pmalloc.h"
#include "cuckoo.h"
#include "obj.h"
#include "valgrind_internal.h"

static struct cuckoo *pools;
__thread struct _pobj_pcache _pobj_cached_pool;

/*
 * obj_init -- initialization of obj
 *
 * Called by constructor.
 */
void
obj_init(void)
{
	LOG(3, NULL);
	pools = cuckoo_new();
	if (pools == NULL)
		FATAL("!cuckoo_new");
}

/*
 * obj_fini -- cleanup of obj
 *
 * Called by destructor.
 */
void
obj_fini(void)
{
	LOG(3, NULL);
	cuckoo_delete(pools);
}

/*
 * drain_empty -- (internal) empty function for drain on non-pmem memory
 */
static void
drain_empty(void)
{
	/* do nothing */
}

/*
 * nopmem_memcpy_persist -- (internal) memcpy followed by an msync
 */
static void *
nopmem_memcpy_persist(void *dest, const void *src, size_t len)
{
	memcpy(dest, src, len);
	pmem_msync(dest, len);
	return dest;
}

/*
 * nopmem_memset_persist -- (internal) memset followed by an msync
 */
static void *
nopmem_memset_persist(void *dest, int c, size_t len)
{
	memset(dest, c, len);
	pmem_msync(dest, len);
	return dest;
}

/*
 * pmemobj_get_uuid_lo -- (internal) evaluates XOR sum of least significant
 * 8 bytes with most significant 8 bytes.
 */
static uint64_t
pmemobj_get_uuid_lo(PMEMobjpool *pop)
{
	uint64_t uuid_lo = 0;

	for (int i = 0; i < 8; i++) {
		uuid_lo = (uuid_lo << 8) |
			(pop->hdr.poolset_uuid[i] ^
				pop->hdr.poolset_uuid[8 + i]);
	}

	return uuid_lo;
}

/*
 * pmemobj_boot -- (internal) boots the pmemobj pool
 */
static int
pmemobj_boot(PMEMobjpool *pop)
{
	LOG(3, "pop %p", pop);

	if ((errno = lane_boot(pop)) != 0) {
		ERR("!lane_boot");
		return errno;
	}

	if ((errno = heap_boot(pop)) != 0) {
		ERR("!heap_boot");
		return errno;
	}

	if ((errno = lane_recover(pop)) != 0) {
		ERR("!lane_recover");
		return errno;
	}

	return 0;
}

/*
 * pmemobj_map_common -- (internal) map a transactional memory pool
 *
 * This routine does all the work, but takes a rdonly flag so internal
 * calls can map a read-only pool if required.
 *
 * If empty flag is set, the file is assumed to be a new memory pool, and
 * new pool header is created.  Otherwise, a valid header must exist.
 */
static PMEMobjpool *
pmemobj_map_common(int fd, const char *layout, size_t poolsize, int rdonly,
		int empty, int boot)
{
	LOG(3, "fd %d layout %s poolsize %zu rdonly %d empty %d",
			fd, layout, poolsize, rdonly, empty);

	void *addr;
	if ((addr = util_map(fd, poolsize, rdonly)) == NULL) {
		(void) close(fd);
		return NULL;	/* util_map() set errno, called LOG */
	}

	VALGRIND_REGISTER_PMEM_MAPPING(addr, poolsize);
	VALGRIND_REGISTER_PMEM_FILE(fd, addr, poolsize, 0);

	(void) close(fd);

	/* check if the mapped region is located in persistent memory */
	int is_pmem = pmem_is_pmem(addr, poolsize);

	/* opaque info lives at the beginning of mapped memory pool */
	struct pmemobjpool *pop = addr;
	struct pool_hdr hdr;

	/* pointer to pool descriptor */
	void *dscp = (void *)((uintptr_t)(&pop->hdr) +
						sizeof (struct pool_hdr));

	if (!empty) {
		memcpy(&hdr, &pop->hdr, sizeof (hdr));

		if (!util_convert_hdr(&hdr)) {
			errno = EINVAL;
			goto err;
		}

		/*
		 * valid header found
		 */
		if (strncmp(hdr.signature, OBJ_HDR_SIG, POOL_HDR_SIG_LEN)) {
			ERR("wrong pool type: \"%s\"", hdr.signature);
			errno = EINVAL;
			goto err;
		}

		if (hdr.major != OBJ_FORMAT_MAJOR) {
			ERR("obj pool version %d (library expects %d)",
				hdr.major, OBJ_FORMAT_MAJOR);
			errno = EINVAL;
			goto err;
		}

		/* XXX - pools sets / replicas */
		if (memcmp(hdr.uuid, hdr.prev_part_uuid, POOL_HDR_UUID_LEN) ||
		    memcmp(hdr.uuid, hdr.next_part_uuid, POOL_HDR_UUID_LEN) ||
		    memcmp(hdr.uuid, hdr.prev_repl_uuid, POOL_HDR_UUID_LEN) ||
		    memcmp(hdr.uuid, hdr.next_repl_uuid, POOL_HDR_UUID_LEN)) {
			ERR("wrong UUID");
			errno = EINVAL;
			goto err;
		}

		if (util_check_arch_flags(&hdr.arch_flags)) {
			ERR("wrong architecture flags");
			errno = EINVAL;
			goto err;
		}

		if (layout &&
		    strncmp(pop->layout, layout, PMEMOBJ_MAX_LAYOUT)) {
			ERR("wrong layout (\"%s\"), "
				"pool created with layout \"%s\"",
				layout, pop->layout);
			errno = EINVAL;
			goto err;
		}

		if (!util_checksum(dscp, OBJ_DSC_P_SIZE, &pop->checksum, 0)) {
			ERR("invalid checksum of pool descriptor");
			errno = EINVAL;
			goto err;
		}

		/* XXX check rest of required metadata */

		int retval = util_feature_check(&hdr, OBJ_FORMAT_INCOMPAT,
							OBJ_FORMAT_RO_COMPAT,
							OBJ_FORMAT_COMPAT);
		if (retval < 0)
		    goto err;
		else if (retval == 0)
		    rdonly = 1;
	} else {
		LOG(3, "creating new transactional memory pool");

		ASSERTeq(rdonly, 0);

		struct pool_hdr *hdrp = &pop->hdr;

		/* check if the pool header is all zeros */
		if (!util_is_zeroed(hdrp, sizeof (*hdrp))) {
			ERR("Non-empty file detected");
			errno = EINVAL;
			goto err;
		}

		/* check length of layout */
		if (layout && (strlen(layout) >= PMEMOBJ_MAX_LAYOUT)) {
				ERR("Layout too long");
				errno = EINVAL;
				goto err;
		}

		/* create pool's header */
		strncpy(hdrp->signature, OBJ_HDR_SIG, POOL_HDR_SIG_LEN);
		hdrp->major = htole32(OBJ_FORMAT_MAJOR);
		hdrp->compat_features = htole32(OBJ_FORMAT_COMPAT);
		hdrp->incompat_features = htole32(OBJ_FORMAT_INCOMPAT);
		hdrp->ro_compat_features = htole32(OBJ_FORMAT_RO_COMPAT);
		uuid_generate(hdrp->uuid);
		/* XXX - pools sets / replicas */
		uuid_generate(hdrp->poolset_uuid);
		memcpy(hdrp->prev_part_uuid, hdrp->uuid, POOL_HDR_UUID_LEN);
		memcpy(hdrp->next_part_uuid, hdrp->uuid, POOL_HDR_UUID_LEN);
		memcpy(hdrp->prev_repl_uuid, hdrp->uuid, POOL_HDR_UUID_LEN);
		memcpy(hdrp->next_repl_uuid, hdrp->uuid, POOL_HDR_UUID_LEN);
		hdrp->crtime = htole64((uint64_t)time(NULL));

		if (util_get_arch_flags(&hdrp->arch_flags)) {
			ERR("Reading architecture flags failed\n");
			errno = EINVAL;
			goto err;
		}

		hdrp->arch_flags.alignment_desc =
			htole64(hdrp->arch_flags.alignment_desc);
		hdrp->arch_flags.e_machine =
			htole16(hdrp->arch_flags.e_machine);

		util_checksum(hdrp, sizeof (*hdrp), &hdrp->checksum, 1);

		/* store pool's header */
		pmem_msync(hdrp, sizeof (*hdrp));

		/* initialize run_id, it will be incremented later */
		pop->run_id = 0;
		pmem_msync(&pop->run_id, sizeof (pop->run_id));

		/* zero all lanes */
		void *lanes_layout = (void *)((uintptr_t)pop +
							OBJ_LANES_OFFSET);

		memset(lanes_layout, 0,
			OBJ_NLANES * sizeof (struct lane_layout));
		pmem_msync(lanes_layout, OBJ_NLANES *
			sizeof (struct lane_layout));

		/* initialization of the obj_store */
		uint64_t obj_store_offset = OBJ_LANES_OFFSET +
			OBJ_NLANES * sizeof (struct lane_layout);
		uint64_t obj_store_size = (PMEMOBJ_NUM_OID_TYPES + 1) *
			sizeof (struct object_store_item);
			/* + 1 - for root object */
		void *store = (void *)((uintptr_t)pop + obj_store_offset);
		memset(store, 0, obj_store_size);
		pmem_msync(store, obj_store_size);

		/* create the persistent part of pool's descriptor */
		memset(dscp, 0, OBJ_DSC_P_SIZE);
		if (layout)
			strncpy(pop->layout, layout, PMEMOBJ_MAX_LAYOUT - 1);
		pop->lanes_offset = OBJ_LANES_OFFSET;
		pop->nlanes = OBJ_NLANES;
		pop->obj_store_offset = obj_store_offset;
		pop->obj_store_size = obj_store_size;
		pop->heap_offset = pop->obj_store_offset +
			pop->obj_store_size;
		pop->heap_size = poolsize - pop->heap_offset;

		if ((errno = heap_init(pop)) != 0) {
			ERR("!heap_init");
			goto err;
		}

		util_checksum(dscp, OBJ_DSC_P_SIZE, &pop->checksum, 1);

		/* store the persistent part of pool's descriptor (2kB) */
		pmem_msync(dscp, OBJ_DSC_P_SIZE);
	}

	/* run_id is made unique by incrementing the previous value */
	pop->run_id += 2;
	if (pop->run_id == 0)
		pop->run_id += 2;
	pmem_msync(&pop->run_id, sizeof (pop->run_id));

	VALGRIND_REMOVE_PMEM_MAPPING(&pop->addr,
			sizeof (struct pmemobjpool) -
			sizeof (struct pool_hdr) -
			OBJ_DSC_P_SIZE);

	/*
	 * Use some of the memory pool area for run-time info.  This
	 * run-time state is never loaded from the file, it is always
	 * created here, so no need to worry about byte-order.
	 */
	pop->addr = addr;
	pop->size = poolsize;
	pop->rdonly = rdonly;
	pop->lanes = NULL;
	pop->is_pmem = is_pmem;

	pop->uuid_lo = pmemobj_get_uuid_lo(pop);
	pop->store = (struct object_store *)
			((uintptr_t)pop + pop->obj_store_offset);

	if (pop->is_pmem) {
		pop->persist = pmem_persist;
		pop->flush = pmem_flush;
		pop->drain = pmem_drain;
		pop->memcpy_persist = pmem_memcpy_persist;
		pop->memset_persist = pmem_memset_persist;
	} else {
		pop->persist = (persist_fn)pmem_msync;
		pop->flush = (flush_fn)pmem_msync;
		pop->drain = drain_empty;
		pop->memcpy_persist = nopmem_memcpy_persist;
		pop->memset_persist = nopmem_memset_persist;
	}

	if (boot) {
		if ((errno = pmemobj_boot(pop)) != 0)
			goto err;

		if ((errno = cuckoo_insert(pools, pop->uuid_lo, pop)) != 0) {
			ERR("!cuckoo_insert");
			goto err;
		}

	}

	/* XXX the rest of run-time info */

	/*
	 * If possible, turn off all permissions on the pool header page.
	 *
	 * The prototype PMFS doesn't allow this when large pages are in
	 * use. It is not considered an error if this fails.
	 */
	util_range_none(addr, sizeof (struct pool_hdr));


	LOG(3, "pop %p", pop);
	return pop;

err:
	LOG(4, "error clean up");
	int oerrno = errno;
	VALGRIND_REMOVE_PMEM_MAPPING(addr, poolsize);
	util_unmap(addr, poolsize);
	errno = oerrno;
	return NULL;
}

/*
 * pmemobj_create -- create a transactional memory pool
 */
PMEMobjpool *
pmemobj_create(const char *path, const char *layout, size_t poolsize,
		mode_t mode)
{
	LOG(3, "path %s layout %s poolsize %zu mode %d",
			path, layout, poolsize, mode);

	int created = 0;
	int fd;
	if (poolsize != 0) {
		/* create a new memory pool file */
		fd = util_pool_create(path, poolsize, PMEMOBJ_MIN_POOL, mode);
		created = 1;
	} else {
		/* open an existing file */
		fd = util_pool_open(path, &poolsize, PMEMOBJ_MIN_POOL);
	}
	if (fd == -1)
		return NULL;	/* errno set by util_pool_create/open() */

	PMEMobjpool *pop = pmemobj_map_common(fd, layout, poolsize, 0, 1, 1);
	if (pop == NULL && created)
		unlink(path);	/* delete file if pool creation failed */

	return pop;
}

/*
 * pmemobj_open -- open a transactional memory pool
 */
PMEMobjpool *
pmemobj_open(const char *path, const char *layout)
{
	LOG(3, "path %s layout %s", path, layout);

	size_t poolsize = 0;
	int fd;

	if ((fd = util_pool_open(path, &poolsize, PMEMOBJ_MIN_POOL)) == -1)
		return NULL;	/* errno set by util_pool_open() */

	return pmemobj_map_common(fd, layout, poolsize, 0, 0, 1);
}

/*
 * pmemobj_create_part -- XXX
 */
PMEMobjpool *
pmemobj_create_part(const char *path, const char *layout, size_t partsize,
	mode_t mode, int part_index, int nparts, int replica_index,
	int nreplica)
{
	/* XXX */
	return NULL;
}

/*
 * pmemobj_close_cleanup -- (internal) cleanup the pool and unmap
 */
static void
pmemobj_cleanup(PMEMobjpool *pop)
{
	LOG(3, "pop %p", pop);

	if ((errno = heap_cleanup(pop)) != 0)
		ERR("!heap_cleanup");

	/* cleanup run-time state */
	if ((errno = lane_cleanup(pop)) != 0)
		ERR("!lane_cleanup");

	VALGRIND_REMOVE_PMEM_MAPPING(pop->addr, pop->size);
	util_unmap(pop->addr, pop->size);
}

/*
 * pmemobj_close -- close a transactional memory pool
 */
void
pmemobj_close(PMEMobjpool *pop)
{
	LOG(3, "pop %p", pop);

	if (cuckoo_remove(pools, pop->uuid_lo) != pop) {
		ERR("!cuckoo_remove");
	}

	if (_pobj_cached_pool.pop == pop) {
		_pobj_cached_pool.pop = NULL;
		_pobj_cached_pool.uuid_lo = 0;
	}

	pmemobj_cleanup(pop);
}

/*
 * pmemobj_check -- transactional memory pool consistency check
 */
int
pmemobj_check(const char *path, const char *layout)
{
	LOG(3, "path %s layout %s", path, layout);

	size_t poolsize = 0;
	int fd;

	if ((fd = util_pool_open(path, &poolsize, PMEMOBJ_MIN_POOL)) == -1)
		return -1;	/* errno set by util_pool_open() */

	/* map the pool read-only */
	PMEMobjpool *pop = pmemobj_map_common(fd, layout, poolsize, 1, 0, 0);

	if (pop == NULL)
		return -1;	/* errno set by pmemobj_map_common() */

	int consistent = 1;

	if (pop->run_id % 2) {
		ERR("invalid run_id %ju", pop->run_id);
		consistent = 0;
	}

	if ((errno = lane_check(pop)) != 0) {
		LOG(3, "!lane_check");
		consistent = 0;
	}

	if ((errno = heap_check(pop)) != 0) {
		LOG(3, "!heap_check");
		consistent = 0;
	}

	if (consistent && (errno = pmemobj_boot(pop)) != 0) {
		LOG(3, "!pmemobj_boot");
		consistent = 0;
	}

	if (consistent) {
		pmemobj_close(pop);
	} else {
		VALGRIND_REMOVE_PMEM_MAPPING(pop, poolsize);
		util_unmap(pop, poolsize);
	}

	/* XXX validate metadata */


	if (consistent)
		LOG(4, "pool consistency check OK");

	return consistent;
}

/*
 * pmemobj_pool -- returns the pool handle associated with the oid
 */
PMEMobjpool *
pmemobj_pool(PMEMoid oid)
{
	return cuckoo_get(pools, oid.pool_uuid_lo);
}


/* arguments for constructor_alloc_bytype */
struct carg_bytype {
	uint16_t user_type;
	void (*constructor)(PMEMobjpool *pop, void *ptr, void *arg);
	void *arg;
};

/*
 * constructor_alloc_bytype -- (internal) constructor for obj_alloc_construct
 */
static void
constructor_alloc_bytype(PMEMobjpool *pop, void *ptr, void *arg)
{
	LOG(3, "pop %p ptr %p arg %p", pop, ptr, arg);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct oob_header *pobj = OOB_HEADER_FROM_PTR(ptr);
	struct carg_bytype *carg = arg;

	pobj->internal_type = TYPE_ALLOCATED;
	pobj->user_type = carg->user_type;
	pop->persist(pobj, OBJ_OOB_SIZE);

	if (carg->constructor)
		carg->constructor(pop, ptr, carg->arg);
}

/*
 * obj_alloc_construct -- (internal) allocates a new object with constructor
 */
static int
obj_alloc_construct(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
	unsigned int type_num, void (*constructor)(PMEMobjpool *pop, void *ptr,
	void *arg), void *arg)
{
	if (type_num >= PMEMOBJ_NUM_OID_TYPES) {
		errno = EINVAL;
		ERR("!obj_alloc_construct");
		LOG(2, "type_num has to be in range [0, %i]",
			PMEMOBJ_NUM_OID_TYPES - 1);
		return -1;
	}

	struct list_head *lhead = &pop->store->bytype[type_num].head;
	struct carg_bytype carg;

	carg.user_type = type_num;
	carg.constructor = constructor;
	carg.arg = arg;

	return list_insert_new(pop, lhead, 0, NULL, OID_NULL, 0, size,
				constructor_alloc_bytype, &carg, oidp);
}

/*
 * pmemobj_alloc -- allocates a new object
 */
int
pmemobj_alloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
	unsigned int type_num, void (*constructor)(PMEMobjpool *pop, void *ptr,
	void *arg), void *arg)
{
	LOG(3, "pop %p oidp %p size %zu type_num %u constructor %p arg %p",
		pop, oidp, size, type_num, constructor, arg);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();

	if (size == 0) {
		ERR("allocation with size 0");
		errno = EINVAL;
		return -1;
	}

	return obj_alloc_construct(pop, oidp, size, type_num, constructor, arg);
}

/* arguments for constructor_zalloc */
struct carg_alloc {
	size_t size;
};

/* arguments for constructor_realloc and constructor_zrealloc */
struct carg_realloc {
	void *ptr;
	size_t old_size;
	size_t new_size;
	unsigned int user_type;
};

/*
 * constructor_zalloc -- (internal) constructor for pmemobj_zalloc
 */
static void
constructor_zalloc(PMEMobjpool *pop, void *ptr, void *arg)
{
	LOG(3, "pop %p ptr %p arg %p", pop, ptr, arg);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct carg_alloc *carg = arg;

	pop->memset_persist(ptr, 0, carg->size);
}

/*
 * pmemobj_zalloc -- allocates a new zeroed object
 */
int
pmemobj_zalloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
		unsigned int type_num)
{
	LOG(3, "pop %p oidp %p size %zu type_num %u",
			pop, oidp, size, type_num);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();

	if (size == 0) {
		ERR("allocation with size 0");
		errno = EINVAL;
		return -1;
	}

	struct carg_alloc carg;
	carg.size = size;

	return obj_alloc_construct(pop, oidp, size, type_num,
					constructor_zalloc, &carg);
}

/*
 * obj_free -- (internal) free an object
 */
static void
obj_free(PMEMobjpool *pop, PMEMoid *oidp)
{
	struct oob_header *pobj = OOB_HEADER_FROM_OID(pop, *oidp);

	ASSERT(pobj->user_type < PMEMOBJ_NUM_OID_TYPES);

	void *lhead = &pop->store->bytype[pobj->user_type].head;
	if (list_remove_free(pop, lhead, 0, NULL, oidp))
		LOG(2, "list_remove_free failed");
}

/*
 * obj_realloc_common -- (internal) common routine for resizing
 *                          existing objects
 */
static int
obj_realloc_common(PMEMobjpool *pop, struct object_store *store,
	PMEMoid *oidp, size_t size, unsigned int type_num,
	void (*constr_alloc)(PMEMobjpool *pop, void *ptr, void *arg),
	void (*constr_realloc)(PMEMobjpool *pop, void *ptr, void *arg))
{

	/* if OID is NULL just allocate memory */
	if (OBJ_OID_IS_NULL(*oidp)) {
		struct carg_alloc carg;
		carg.size = size;

		/* if size is 0 - do nothing */
		if (size == 0)
			return 0;

		return obj_alloc_construct(pop, oidp, size, type_num,
						constr_alloc, &carg);
	}

	/* if size is 0 just free */
	if (size == 0) {
		obj_free(pop, oidp);
		return 0;
	}

	struct carg_realloc carg;
	carg.ptr = OBJ_OFF_TO_PTR(pop, oidp->off);
	carg.new_size = size;
	carg.old_size = pmemobj_alloc_usable_size(*oidp);
	carg.user_type = type_num;

	struct oob_header *pobj = OOB_HEADER_FROM_OID(pop, *oidp);
	uint16_t user_type_old = pobj->user_type;

	ASSERT(user_type_old < PMEMOBJ_NUM_OID_TYPES);

	if (type_num >= PMEMOBJ_NUM_OID_TYPES) {
		errno = EINVAL;
		ERR("!obj_realloc_construct");
		LOG(2, "type_num has to be in range [0, %u]",
		    PMEMOBJ_NUM_OID_TYPES - 1);
		return -1;
	}

	struct list_head *lhead_old = &store->bytype[user_type_old].head;
	if (type_num == user_type_old) {
		int ret = list_realloc(pop, lhead_old, 0, NULL, size,
				constr_realloc, &carg, 0, 0, oidp);
		if (ret)
			LOG(2, "list_realloc failed");

		return ret;
	} else {
		struct list_head *lhead_new = &store->bytype[type_num].head;
		uint64_t user_type_offset = OOB_OFFSET_OF(*oidp, user_type);
		int ret = list_realloc_move(pop, lhead_old, lhead_new, 0, NULL,
				size, constr_realloc, &carg, user_type_offset,
				type_num, oidp);
		if (ret)
			LOG(2, "list_realloc_move failed");

		return ret;
	}
}

/*
 * constructor_realloc -- (internal) constructor for pmemobj_realloc
 */
static void
constructor_realloc(PMEMobjpool *pop, void *ptr, void *arg)
{
	LOG(3, "pop %p ptr %p arg %p", pop, ptr, arg);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct carg_realloc *carg = arg;
	struct oob_header *pobj = OOB_HEADER_FROM_PTR(ptr);

	if (ptr != carg->ptr) {
		size_t cpy_size = carg->new_size > carg->old_size ?
			carg->old_size : carg->new_size;

		pop->memcpy_persist(ptr, carg->ptr, cpy_size);

		pobj->internal_type = TYPE_ALLOCATED;
		pobj->user_type = carg->user_type;
		pop->persist(pobj, sizeof (*pobj));
	}
}

/*
 * constructor_zrealloc -- (internal) constructor for pmemobj_zrealloc
 */
static void
constructor_zrealloc(PMEMobjpool *pop, void *ptr, void *arg)
{
	LOG(3, "pop %p ptr %p arg %p", pop, ptr, arg);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct carg_realloc *carg = arg;
	struct oob_header *pobj = OOB_HEADER_FROM_PTR(ptr);

	if (ptr != carg->ptr) {
		size_t cpy_size = carg->new_size > carg->old_size ?
			carg->old_size : carg->new_size;

		pop->memcpy_persist(ptr, carg->ptr, cpy_size);

		pobj->internal_type = TYPE_ALLOCATED;
		pobj->user_type = carg->user_type;
		pop->persist(pobj, sizeof (*pobj));
	}

	if (carg->new_size > carg->old_size) {
		size_t grow_len = carg->new_size - carg->old_size;
		void *new_data_ptr = (void *)((uintptr_t)ptr + carg->old_size);

		pop->memset_persist(new_data_ptr, 0, grow_len);
	}
}

/*
 * constructor_zrealloc_root -- (internal) constructor for pmemobj_root
 */
static void
constructor_zrealloc_root(PMEMobjpool *pop, void *ptr, void *arg)
{
	LOG(3, "pop %p ptr %p arg %p", pop, ptr, arg);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	VALGRIND_ADD_TO_TX(OOB_HEADER_FROM_PTR(ptr),
		((struct carg_realloc *)arg)->new_size + OBJ_OOB_SIZE);
	constructor_zrealloc(pop, ptr, arg);
	VALGRIND_REMOVE_FROM_TX(OOB_HEADER_FROM_PTR(ptr),
		((struct carg_realloc *)arg)->new_size + OBJ_OOB_SIZE);
}

/*
 * pmemobj_realloc -- resizes an existing object
 */
int
pmemobj_realloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
		unsigned int type_num)
{
	LOG(3, "pop %p oid.off 0x%016jx size %zu type_num %u",
		pop, oidp->off, size, type_num);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();
	ASSERT(OBJ_OID_IS_VALID(pop, *oidp));

	return obj_realloc_common(pop, pop->store, oidp, size, type_num,
			NULL, constructor_realloc);
}

/*
 * pmemobj_zrealloc -- resizes an existing object, any new space is zeroed.
 */
int
pmemobj_zrealloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
		unsigned int type_num)
{
	LOG(3, "pop %p oid.off 0x%016jx size %zu type_num %u",
		pop, oidp->off, size, type_num);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();
	ASSERT(OBJ_OID_IS_VALID(pop, *oidp));

	return obj_realloc_common(pop, pop->store, oidp, size, type_num,
			constructor_zalloc, constructor_zrealloc);
}

/* arguments for constructor_strdup */
struct carg_strdup {
	size_t size;
	const char *s;
};

/*
 * constructor_strdup -- (internal) constructor of pmemobj_strndup
 */
static void
constructor_strdup(PMEMobjpool *pop, void *ptr, void *arg)
{
	LOG(3, "pop %p ptr %p arg %p", pop, ptr, arg);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct carg_strdup *carg = arg;

	/* copy string */
	pop->memcpy_persist(ptr, carg->s, carg->size);
}

/*
 * pmemobj_strndup -- allocates a new object with duplicate of the string s.
 */
int
pmemobj_strdup(PMEMobjpool *pop, PMEMoid *oidp, const char *s,
		unsigned int type_num)
{
	LOG(3, "pop %p oidp %p string %s type_num %u", pop, oidp, s, type_num);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();

	if (type_num >= PMEMOBJ_NUM_OID_TYPES) {
		errno = EINVAL;
		ERR("!pmemobj_strdup");
		LOG(2, "type_num has to be in range [0, %i]",
		    PMEMOBJ_NUM_OID_TYPES - 1);
		return -1;
	}

	if (NULL == s) {
		errno = EINVAL;
		return -1;
	}

	struct carg_strdup carg;
	carg.size = (strlen(s) + 1) * sizeof (char);
	carg.s = s;

	return obj_alloc_construct(pop, oidp, carg.size, type_num,
					constructor_strdup, &carg);
}

/*
 * pmemobj_free -- frees an existing object
 */
void
pmemobj_free(PMEMoid *oidp)
{
	LOG(3, "oid.off 0x%016jx", oidp->off);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();

	if (oidp->off == 0)
		return;

	PMEMobjpool *pop = cuckoo_get(pools, oidp->pool_uuid_lo);

	ASSERTne(pop, NULL);
	ASSERT(OBJ_OID_IS_VALID(pop, *oidp));

	obj_free(pop, oidp);
}

/*
 * pmemobj_alloc_usable_size -- returns usable size of object
 */
size_t
pmemobj_alloc_usable_size(PMEMoid oid)
{
	LOG(3, "oid.off 0x%016jx", oid.off);

	if (oid.off == 0)
		return 0;

	PMEMobjpool *pop = cuckoo_get(pools, oid.pool_uuid_lo);

	ASSERTne(pop, NULL);
	ASSERT(OBJ_OID_IS_VALID(pop, oid));

	return (pmalloc_usable_size(pop, oid.off - OBJ_OOB_SIZE) -
			OBJ_OOB_SIZE);
}

/*
 * pmemobj_memcpy_persist -- pmemobj version of memcpy
 */
void *
pmemobj_memcpy_persist(PMEMobjpool *pop, void *dest, const void *src,
	size_t len)
{
	return pop->memcpy_persist(dest, src, len);
}

/*
 * pmemobj_memset_persist -- pmemobj version of memset
 */
void *
pmemobj_memset_persist(PMEMobjpool *pop, void *dest, int c, size_t len)
{
	return pop->memset_persist(dest, c, len);
}

/*
 * pmemobj_persist -- pmemobj version of pmem_persist
 */
void
pmemobj_persist(PMEMobjpool *pop, void *addr, size_t len)
{
	pop->persist(addr, len);
}

/*
 * pmemobj_flush -- pmemobj version of pmem_flush
 */
void
pmemobj_flush(PMEMobjpool *pop, void *addr, size_t len)
{
	pop->flush(addr, len);
}

/*
 * pmemobj_drain -- pmemobj version of pmem_drain
 */
void
pmemobj_drain(PMEMobjpool *pop)
{
	pop->drain();
}

/*
 * pmemobj_type_num -- returns type number of object
 */
unsigned int
pmemobj_type_num(PMEMoid oid)
{
	LOG(3, "oid.off 0x%016jx", oid.off);

	if (OBJ_OID_IS_NULL(oid))
		return -1;

	void *ptr = pmemobj_direct(oid);

	struct oob_header *oobh = OOB_HEADER_FROM_PTR(ptr);
	return oobh->user_type;
}

/* arguments for constructor_alloc_root */
struct carg_root {
	size_t size;
};

/*
 * constructor_alloc_root -- (internal) constructor for obj_alloc_root
 */
static void
constructor_alloc_root(PMEMobjpool *pop, void *ptr, void *arg)
{
	LOG(3, "pop %p ptr %p arg %p", pop, ptr, arg);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct oob_header *ro = OOB_HEADER_FROM_PTR(ptr);
	struct carg_root *carg = arg;

	/* temporarily add atomic root allocation to pmemcheck transaction */
	VALGRIND_ADD_TO_TX(ro, OBJ_OOB_SIZE + carg->size);

	pop->memset_persist(ptr, 0, carg->size);

	ro->internal_type = TYPE_ALLOCATED;
	ro->user_type = POBJ_ROOT_TYPE_NUM;
	ro->size = carg->size;

	VALGRIND_REMOVE_FROM_TX(ro, OBJ_OOB_SIZE + carg->size);

	pop->persist(ro, OBJ_OOB_SIZE);
}

/*
 * obj_alloc_root -- (internal) allocate root object
 */
static int
obj_alloc_root(PMEMobjpool *pop, struct object_store *store, size_t size)
{
	LOG(3, "pop %p store %p size %zu", pop, store, size);

	struct list_head *lhead = &store->root.head;
	struct carg_root carg;

	carg.size = size;

	return list_insert_new(pop, lhead, 0, NULL, OID_NULL, 0,
				size, constructor_alloc_root, &carg, NULL);
}

/*
 * obj_realloc_root -- (internal) reallocate root object
 */
static int
obj_realloc_root(PMEMobjpool *pop, struct object_store *store, size_t size,
	size_t old_size)
{
	LOG(3, "pop %p store %p size %zu old_size %zu",
		pop, store, size, old_size);

	struct list_head *lhead = &store->root.head;
	uint64_t size_offset = OOB_OFFSET_OF(lhead->pe_first, size);
	struct carg_realloc carg;

	carg.ptr = OBJ_OFF_TO_PTR(pop, lhead->pe_first.off);
	carg.old_size = old_size;
	carg.new_size = size;
	carg.user_type = POBJ_ROOT_TYPE_NUM;

	return list_realloc(pop, lhead, 0, NULL, size,
				constructor_zrealloc_root, &carg,
				size_offset, size, &lhead->pe_first);
}

/*
 * pmemobj_root_size -- returns size of the root object
 */
size_t
pmemobj_root_size(PMEMobjpool *pop)
{
	LOG(3, "pop %p", pop);

	if (pop->store->root.head.pe_first.off) {
		struct oob_header *ro = OOB_HEADER_FROM_OID(pop,
						pop->store->root.head.pe_first);
		return ro->size;
	} else
		return 0;
}

/*
 * pmemobj_root -- returns root object
 */
PMEMoid
pmemobj_root(PMEMobjpool *pop, size_t size)
{
	LOG(3, "pop %p size %zu", pop, size);

	PMEMoid root;

	pmemobj_mutex_lock(pop, &pop->rootlock);
	if (pop->store->root.head.pe_first.off == 0)
		/* root object list is empty */
		obj_alloc_root(pop, pop->store, size);
	else {
		size_t old_size = pmemobj_root_size(pop);
		if (size > old_size)
			if (obj_realloc_root(pop, pop->store, size, old_size)) {
				pmemobj_mutex_unlock(pop, &pop->rootlock);
				LOG(2, "obj_realloc_root failed");
				return OID_NULL;
			}
	}
	root = pop->store->root.head.pe_first;
	pmemobj_mutex_unlock(pop, &pop->rootlock);
	return root;
}

/*
 * pmemobj_first - returns first object of specified type
 */
PMEMoid
pmemobj_first(PMEMobjpool *pop, unsigned int type_num)
{
	LOG(3, "pop %p type_num %u", pop, type_num);

	if (type_num >= PMEMOBJ_NUM_OID_TYPES) {
		errno = EINVAL;
		ERR("!pmemobj_first");
		LOG(2, "type_num has to be in range [0, %i]",
		    PMEMOBJ_NUM_OID_TYPES - 1);
		return OID_NULL;
	}

	return pop->store->bytype[type_num].head.pe_first;
}

/*
 * pmemobj_next - returns next object of specified type
 */
PMEMoid
pmemobj_next(PMEMoid oid)
{
	LOG(3, "oid.off 0x%016jx", oid.off);

	if (oid.off == 0)
		return OID_NULL;

	PMEMobjpool *pop = cuckoo_get(pools, oid.pool_uuid_lo);

	ASSERTne(pop, NULL);
	ASSERT(OBJ_OID_IS_VALID(pop, oid));

	struct oob_header *pobj = OOB_HEADER_FROM_OID(pop, oid);
	uint16_t user_type = pobj->user_type;

	ASSERT(user_type < PMEMOBJ_NUM_OID_TYPES);

	if (pobj->oob.pe_next.off !=
			pop->store->bytype[user_type].head.pe_first.off)
		return pobj->oob.pe_next;
	else
		return OID_NULL;
}


/*
 * pmemobj_list_insert -- adds object to a list
 */
int
pmemobj_list_insert(PMEMobjpool *pop, size_t pe_offset, void *head,
		    PMEMoid dest, int before, PMEMoid oid)
{
	LOG(3, "pop %p pe_offset %zu head %p dest.off 0x%016jx before %d"
	    " oid.off 0x%016jx",
	    pop, pe_offset, head, dest.off, before, oid.off);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();
	ASSERT(OBJ_OID_IS_VALID(pop, oid));
	ASSERT(OBJ_OID_IS_VALID(pop, dest));

	return list_insert(pop, pe_offset, head, dest, before, oid);
}

/*
 * pmemobj_list_insert_new -- adds new object to a list
 */
PMEMoid
pmemobj_list_insert_new(PMEMobjpool *pop, size_t pe_offset, void *head,
			PMEMoid dest, int before, size_t size,
			unsigned int type_num,
			void (*constructor)(PMEMobjpool *pop, void *ptr,
			void *arg), void *arg)
{
	LOG(3, "pop %p pe_offset %zu head %p dest.off 0x%016jx before %d"
	    " size %zu type_num %u",
	    pop, pe_offset, head, dest.off, before, size, type_num);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();
	ASSERT(OBJ_OID_IS_VALID(pop, dest));

	if (type_num >= PMEMOBJ_NUM_OID_TYPES) {
		errno = EINVAL;
		ERR("!pmemobj_list_insert_new");
		LOG(2, "type_num has to be in range [0, %i]",
		    PMEMOBJ_NUM_OID_TYPES - 1);
		return OID_NULL;
	}

	struct list_head *lhead = &pop->store->bytype[type_num].head;
	struct carg_bytype carg;

	carg.user_type = type_num;
	carg.constructor = constructor;
	carg.arg = arg;

	PMEMoid retoid = OID_NULL;
	list_insert_new(pop, lhead,
			pe_offset, head, dest, before,
			size, constructor_alloc_bytype, &carg, &retoid);
	return retoid;
}

/*
 * pmemobj_list_remove -- removes object from a list
 */
int
pmemobj_list_remove(PMEMobjpool *pop, size_t pe_offset, void *head,
		    PMEMoid oid, int free)
{
	LOG(3, "pop %p pe_offset %zu head %p oid.off 0x%016jx free %d",
	    pop, pe_offset, head, oid.off, free);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();
	ASSERT(OBJ_OID_IS_VALID(pop, oid));

	if (free) {
		struct oob_header *pobj = OOB_HEADER_FROM_OID(pop, oid);

		ASSERT(pobj->user_type < PMEMOBJ_NUM_OID_TYPES);

		void *lhead = &pop->store->bytype[pobj->user_type].head;
		return list_remove_free(pop, lhead, pe_offset, head, &oid);
	} else
		return list_remove(pop, pe_offset, head, oid);
}

/*
 * pmemobj_list_move -- moves object between lists
 */
int
pmemobj_list_move(PMEMobjpool *pop, size_t pe_old_offset, void *head_old,
			size_t pe_new_offset, void *head_new,
			PMEMoid dest, int before, PMEMoid oid)
{
	LOG(3, "pop %p pe_old_offset %zu pe_new_offset %zu"
	    " head_old %p head_new %p dest.off 0x%016jx"
	    " before %d oid.off 0x%016jx",
	    pop, pe_old_offset, pe_new_offset,
	    head_old, head_new, dest.off, before, oid.off);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();

	ASSERT(OBJ_OID_IS_VALID(pop, oid));
	ASSERT(OBJ_OID_IS_VALID(pop, dest));

	return list_move(pop, pe_old_offset, head_old,
				pe_new_offset, head_new,
				dest, before, oid);
}

/*
 * _pobj_debug_notice -- logs notice message if used inside a transaction
 */
void
_pobj_debug_notice(const char *api_name, const char *file, int line)
{
#ifdef	DEBUG
	if (pmemobj_tx_stage() != TX_STAGE_NONE) {
		if (file)
			LOG(4, "Notice: non-transactional API"
				" used inside a transaction (%s in %s:%d)",
				api_name, file, line);
		else
			LOG(4, "Notice: non-transactional API"
				" used inside a transaction (%s)", api_name);
	}
#endif /* DEBUG */
}
