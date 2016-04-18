/*
 * Copyright 2014-2016, Intel Corporation
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
 *     * Neither the name of the copyright holder nor the names of its
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * obj.c -- transactional object store implementation
 */

#include <sys/param.h>
#include <errno.h>
#include <stdlib.h>

#include "libpmem.h"
#include "libpmemobj.h"

#include "util.h"
#include "out.h"
#include "lane.h"
#include "redo.h"
#include "memops.h"
#include "pmalloc.h"
#include "list.h"
#include "cuckoo.h"
#include "ctree.h"
#include "obj.h"
#include "sync.h"
#include "heap_layout.h"
#include "valgrind_internal.h"

static struct cuckoo *pools_ht; /* hash table used for searching by UUID */
static struct ctree *pools_tree; /* tree used for searching by address */

int _pobj_cache_invalidate;

#ifndef WIN32

__thread struct _pobj_pcache _pobj_cached_pool;

#else /* WIN32 */

struct _pobj_pcache {
	PMEMobjpool *pop;
	uint64_t uuid_lo;
	int invalidate;
};

static pthread_once_t Cached_pool_key_once = PTHREAD_ONCE_INIT;
static pthread_key_t Cached_pool_key;

static void
_Cached_pool_key_alloc(void)
{
	int pth_ret = pthread_key_create(&Cached_pool_key, free);
	if (pth_ret)
		FATAL("!pthread_key_create");
}

void *
pmemobj_direct(PMEMoid oid)
{
	if (oid.off == 0 || oid.pool_uuid_lo == 0)
		return NULL;

	struct _pobj_pcache *pcache = pthread_getspecific(Cached_pool_key);
	if (pcache == NULL) {
		pcache = malloc(sizeof (struct _pobj_pcache));
		int ret = pthread_setspecific(Cached_pool_key, pcache);
		if (ret)
			FATAL("!pthread_setspecific");
	}

	if (_pobj_cache_invalidate != pcache->invalidate ||
	    pcache->uuid_lo != oid.pool_uuid_lo) {
		pcache->invalidate = _pobj_cache_invalidate;

		if ((pcache->pop = pmemobj_pool_by_oid(oid)) == NULL) {
			pcache->uuid_lo = 0;
			return NULL;
		}

		pcache->uuid_lo = oid.pool_uuid_lo;
	}

	return (void *)((uintptr_t)pcache->pop + oid.off);
}

#endif /* WIN32 */

/*
 * User may decide to map all pools with MAP_PRIVATE flag using
 * PMEMOBJ_COW environment variable.
 */
static int Open_cow;

/*
 * obj_init -- initialization of obj
 *
 * Called by constructor.
 */
void
obj_init(void)
{
	LOG(3, NULL);

	COMPILE_ERROR_ON(sizeof (struct pmemobjpool) != 8192);

#ifdef USE_COW_ENV
	char *env = getenv("PMEMOBJ_COW");
	if (env)
		Open_cow = atoi(env);
#endif

#ifdef WIN32

	pthread_once(&Cached_pool_key_once, _Cached_pool_key_alloc);

#endif

	pools_ht = cuckoo_new();
	if (pools_ht == NULL)
		FATAL("!cuckoo_new");

	pools_tree = ctree_new();
	if (pools_tree == NULL)
		FATAL("!ctree_new");
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
	cuckoo_delete(pools_ht);
	ctree_delete(pools_tree);
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
	LOG(15, "dest %p src %p len %zu", dest, src, len);

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
	LOG(15, "dest %p c 0x%02x len %zu", dest, c, len);

	memset(dest, c, len);
	pmem_msync(dest, len);
	return dest;
}

/*
 * XXX - Consider removing obj_norep_*() wrappers to call *_local()
 * functions directly.  Alternatively, always use obj_rep_*(), even
 * if there are no replicas.  Verify the performance penalty.
 */

/*
 * obj_norep_memcpy_persist -- (internal) memcpy w/o replication
 */
static void *
obj_norep_memcpy_persist(PMEMobjpool *pop, void *dest, const void *src,
	size_t len)
{
	LOG(15, "pop %p dest %p src %p len %zu", pop, dest, src, len);

	return pop->memcpy_persist_local(dest, src, len);
}

/*
 * obj_norep_memset_persist -- (internal) memset w/o replication
 */
static void *
obj_norep_memset_persist(PMEMobjpool *pop, void *dest, int c, size_t len)
{
	LOG(15, "pop %p dest %p c 0x%02x len %zu", pop, dest, c, len);

	return pop->memset_persist_local(dest, c, len);
}

/*
 * obj_norep_persist -- (internal) persist w/o replication
 */
static void
obj_norep_persist(PMEMobjpool *pop, const void *addr, size_t len)
{
	LOG(15, "pop %p addr %p len %zu", pop, addr, len);

	pop->persist_local(addr, len);
}

/*
 * obj_norep_flush -- (internal) flush w/o replication
 */
static void
obj_norep_flush(PMEMobjpool *pop, const void *addr, size_t len)
{
	LOG(15, "pop %p addr %p len %zu", pop, addr, len);

	pop->flush_local(addr, len);
}

/*
 * obj_norep_drain -- (internal) drain w/o replication
 */
static void
obj_norep_drain(PMEMobjpool *pop)
{
	LOG(15, "pop %p", pop);

	pop->drain_local();
}

/*
 * obj_rep_memcpy_persist -- (internal) memcpy with replication
 */
static void *
obj_rep_memcpy_persist(PMEMobjpool *pop, void *dest, const void *src,
	size_t len)
{
	LOG(15, "pop %p dest %p src %p len %zu", pop, dest, src, len);

	PMEMobjpool *rep = pop->replica;
	while (rep) {
		void *rdest = (char *)rep + (uintptr_t)dest - (uintptr_t)pop;
		rep->memcpy_persist_local(rdest, src, len);
		rep = rep->replica;
	}
	return pop->memcpy_persist_local(dest, src, len);
}

/*
 * obj_rep_memset_persist -- (internal) memset with replication
 */
static void *
obj_rep_memset_persist(PMEMobjpool *pop, void *dest, int c, size_t len)
{
	LOG(15, "pop %p dest %p c 0x%02x len %zu", pop, dest, c, len);

	PMEMobjpool *rep = pop->replica;
	while (rep) {
		void *rdest = (char *)rep + (uintptr_t)dest - (uintptr_t)pop;
		rep->memset_persist_local(rdest, c, len);
		rep = rep->replica;
	}
	return pop->memset_persist_local(dest, c, len);
}

/*
 * obj_rep_persist -- (internal) persist with replication
 */
static void
obj_rep_persist(PMEMobjpool *pop, const void *addr, size_t len)
{
	LOG(15, "pop %p addr %p len %zu", pop, addr, len);

	PMEMobjpool *rep = pop->replica;
	while (rep) {
		void *raddr = (char *)rep + (uintptr_t)addr - (uintptr_t)pop;
		rep->memcpy_persist_local(raddr, addr, len);
		rep = rep->replica;
	}
	pop->persist_local(addr, len);
}

/*
 * obj_rep_flush -- (internal) flush with replication
 */
static void
obj_rep_flush(PMEMobjpool *pop, const void *addr, size_t len)
{
	LOG(15, "pop %p addr %p len %zu", pop, addr, len);

	PMEMobjpool *rep = pop->replica;
	while (rep) {
		void *raddr = (char *)rep + (uintptr_t)addr - (uintptr_t)pop;
		memcpy(raddr, addr, len);
		rep->flush_local(raddr, len);
		rep = rep->replica;
	}
	pop->flush_local(addr, len);
}

/*
 * obj_rep_drain -- (internal) drain with replication
 */
static void
obj_rep_drain(PMEMobjpool *pop)
{
	LOG(15, "pop %p", pop);

	PMEMobjpool *rep = pop->replica;
	while (rep) {
		rep->drain_local();
		rep = rep->replica;
	}
	pop->drain_local();
}

#ifdef USE_VG_MEMCHECK
/*
 * pmemobj_vg_register_object -- (internal) notify Valgrind about object
 */
static void
pmemobj_vg_register_object(struct pmemobjpool *pop, PMEMoid oid, int is_root)
{
	LOG(4, "pop %p oid.off 0x%016jx is_root %d", pop, oid.off, is_root);
	void *addr = pmemobj_direct(oid);

	size_t sz;
	if (is_root)
		sz = pmemobj_root_size(pop);
	else
		sz = pmemobj_alloc_usable_size(oid);

	size_t headers = sizeof (struct allocation_header) + OBJ_OOB_SIZE;

	VALGRIND_DO_MEMPOOL_ALLOC(pop, addr, sz);
	VALGRIND_DO_MAKE_MEM_DEFINED(pop, addr - headers, sz + headers);
}

/*
 * Arbitrary value. When there's more undefined regions than MAX_UNDEFS, it's
 * not worth reporting everything - developer should fix the code.
 */
#define	MAX_UNDEFS 1000

/*
 * pmemobj_vg_check_no_undef -- (internal) check whether there are any undefined
 *				regions
 */
static void
pmemobj_vg_check_no_undef(struct pmemobjpool *pop)
{
	LOG(4, "pop %p", pop);

	struct {
		void *start, *end;
	} undefs[MAX_UNDEFS];
	int num_undefs = 0;

	VALGRIND_DO_DISABLE_ERROR_REPORTING;
	char *addr_start = pop->addr;
	char *addr_end = addr_start + pop->size;

	while (addr_start < addr_end) {
		char *noaccess = (char *)VALGRIND_CHECK_MEM_IS_ADDRESSABLE(
					addr_start, addr_end - addr_start);
		if (noaccess == NULL)
			noaccess = addr_end;

		while (addr_start < noaccess) {
			char *undefined =
				(char *)VALGRIND_CHECK_MEM_IS_DEFINED(
					addr_start, noaccess - addr_start);

			if (undefined) {
				addr_start = undefined;

#ifdef VALGRIND_CHECK_MEM_IS_UNDEFINED
				addr_start = (char *)
					VALGRIND_CHECK_MEM_IS_UNDEFINED(
					addr_start, noaccess - addr_start);
				if (addr_start == NULL)
					addr_start = noaccess;
#else
				while (addr_start < noaccess &&
						VALGRIND_CHECK_MEM_IS_DEFINED(
								addr_start, 1))
					addr_start++;
#endif

				if (num_undefs < MAX_UNDEFS) {
					undefs[num_undefs].start = undefined;
					undefs[num_undefs].end = addr_start - 1;
					num_undefs++;
				}
			} else
				addr_start = noaccess;
		}

#ifdef VALGRIND_CHECK_MEM_IS_UNADDRESSABLE
		addr_start = (char *)VALGRIND_CHECK_MEM_IS_UNADDRESSABLE(
				addr_start, addr_end - addr_start);
		if (addr_start == NULL)
			addr_start = addr_end;
#else
		while (addr_start < addr_end &&
				(char *)VALGRIND_CHECK_MEM_IS_ADDRESSABLE(
						addr_start, 1) == addr_start)
			addr_start++;
#endif
	}
	VALGRIND_DO_ENABLE_ERROR_REPORTING;

	if (num_undefs) {
		/*
		 * How to resolve this error:
		 * If it's part of the free space Valgrind should be told about
		 * it by VALGRIND_DO_MAKE_MEM_NOACCESS request. If it's
		 * allocated - initialize it or use VALGRIND_DO_MAKE_MEM_DEFINED
		 * request.
		 */

		VALGRIND_PRINTF("Part of the pool is left in undefined state on"
				" boot. This is pmemobj's bug.\nUndefined"
				" regions:\n");
		for (int i = 0; i < num_undefs; ++i)
			VALGRIND_PRINTF("   [%p, %p]\n", undefs[i].start,
					undefs[i].end);
		if (num_undefs == MAX_UNDEFS)
			VALGRIND_PRINTF("   ...\n");

		/* Trigger error. */
		VALGRIND_CHECK_MEM_IS_DEFINED(undefs[0].start, 1);
	}
}

/*
 * pmemobj_vg_boot -- (internal) notify Valgrind about pool objects
 */
static void
pmemobj_vg_boot(struct pmemobjpool *pop)
{
	if (!On_valgrind)
		return;
	LOG(4, "pop %p", pop);

	PMEMoid oid;
	size_t rs = pmemobj_root_size(pop);
	if (rs) {
		oid = pmemobj_root(pop, rs);
		pmemobj_vg_register_object(pop, oid, 1);
	}

	for (oid = pmemobj_first(pop);
			!OID_IS_NULL(oid); oid = pmemobj_next(oid)) {
		pmemobj_vg_register_object(pop, oid, 0);
	}

	if (getenv("PMEMOBJ_VG_CHECK_UNDEF"))
		pmemobj_vg_check_no_undef(pop);
}

#endif

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

	if ((errno = lane_recover_and_section_boot(pop)) != 0) {
		ERR("!lane_recover_and_section_boot");
		return errno;
	}

	return 0;
}

/*
 * pmemobj_descr_create -- (internal) create obj pool descriptor
 */
static int
pmemobj_descr_create(PMEMobjpool *pop, const char *layout, size_t poolsize)
{
	LOG(3, "pop %p layout %s poolsize %zu", pop, layout, poolsize);

	ASSERTeq(poolsize % Pagesize, 0);

	/* opaque info lives at the beginning of mapped memory pool */
	void *dscp = (void *)((uintptr_t)(&pop->hdr) +
				sizeof (struct pool_hdr));

	/* create the persistent part of pool's descriptor */
	memset(dscp, 0, OBJ_DSC_P_SIZE);
	if (layout)
		strncpy(pop->layout, layout, PMEMOBJ_MAX_LAYOUT - 1);

	/* initialize run_id, it will be incremented later */
	pop->run_id = 0;
	pmem_msync(&pop->run_id, sizeof (pop->run_id));

	pop->lanes_offset = OBJ_LANES_OFFSET;
	pop->nlanes = OBJ_NLANES;
	pop->root_offset = 0;

	/* zero all lanes */
	void *lanes_layout = (void *)((uintptr_t)pop +
						pop->lanes_offset);

	memset(lanes_layout, 0,
		pop->nlanes * sizeof (struct lane_layout));
	pmem_msync(lanes_layout, pop->nlanes *
		sizeof (struct lane_layout));

	pop->heap_offset = pop->lanes_offset +
		pop->nlanes * sizeof (struct lane_layout);
	pop->heap_offset = (pop->heap_offset + Pagesize - 1) & ~(Pagesize - 1);
	pop->heap_size = poolsize - pop->heap_offset;

	/* initialize heap prior to storing the checksum */
	if ((errno = heap_init(pop)) != 0) {
		ERR("!heap_init");
		return -1;
	}

	util_checksum(dscp, OBJ_DSC_P_SIZE, &pop->checksum, 1);

	/* store the persistent part of pool's descriptor (2kB) */
	pmem_msync(dscp, OBJ_DSC_P_SIZE);

	return 0;
}

/*
 * pmemobj_descr_check -- (internal) validate obj pool descriptor
 */
static int
pmemobj_descr_check(PMEMobjpool *pop, const char *layout, size_t poolsize)
{
	LOG(3, "pop %p layout %s poolsize %zu", pop, layout, poolsize);

	void *dscp = (void *)((uintptr_t)(&pop->hdr) +
				sizeof (struct pool_hdr));

	if (!util_checksum(dscp, OBJ_DSC_P_SIZE, &pop->checksum, 0)) {
		ERR("invalid checksum of pool descriptor");
		errno = EINVAL;
		return -1;
	}

	if (layout &&
	    strncmp(pop->layout, layout, PMEMOBJ_MAX_LAYOUT)) {
		ERR("wrong layout (\"%s\"), "
			"pool created with layout \"%s\"",
			layout, pop->layout);
		errno = EINVAL;
		return -1;
	}

	if (pop->size < poolsize) {
		ERR("replica size smaller than pool size: %zu < %zu",
			pop->size, poolsize);
		errno = EINVAL;
		return -1;
	}

	if (pop->heap_offset + pop->heap_size != poolsize) {
		ERR("heap size does not match pool size: %zu != %zu",
			pop->heap_offset + pop->heap_size, poolsize);
		errno = EINVAL;
		return -1;
	}

	if (pop->heap_offset % Pagesize ||
	    pop->heap_size % Pagesize) {
		ERR("unaligned heap: off %ju, size %zu",
			pop->heap_offset, pop->heap_size);
		errno = EINVAL;
		return -1;
	}

	return 0;
}

/*
 * pmemobj_replica_init -- (internal) initialize runtime part of the replica
 */
static int
pmemobj_replica_init(PMEMobjpool *pop, int is_pmem)
{
	LOG(3, "pop %p is_pmem %d", pop, is_pmem);

	/*
	 * Use some of the memory pool area for run-time info.  This
	 * run-time state is never loaded from the file, it is always
	 * created here, so no need to worry about byte-order.
	 */
	pop->is_pmem = is_pmem;
	pop->replica = NULL;

	if (pop->is_pmem) {
		pop->persist_local = pmem_persist;
		pop->flush_local = pmem_flush;
		pop->drain_local = pmem_drain;
		pop->memcpy_persist_local = pmem_memcpy_persist;
		pop->memset_persist_local = pmem_memset_persist;
	} else {
		pop->persist_local = (persist_local_fn)pmem_msync;
		pop->flush_local = (flush_local_fn)pmem_msync;
		pop->drain_local = drain_empty;
		pop->memcpy_persist_local = nopmem_memcpy_persist;
		pop->memset_persist_local = nopmem_memset_persist;
	}

	/* initially, use variants w/o replication */
	pop->persist = obj_norep_persist;
	pop->flush = obj_norep_flush;
	pop->drain = obj_norep_drain;
	pop->memcpy_persist = obj_norep_memcpy_persist;
	pop->memset_persist = obj_norep_memset_persist;

	return 0;
}

/*
 * pmemobj_runtime_init -- (internal) initialize runtime part of the pool header
 */
static int
pmemobj_runtime_init(PMEMobjpool *pop, int rdonly, int boot)
{
	LOG(3, "pop %p rdonly %d boot %d", pop, rdonly, boot);

	if (pop->replica != NULL) {
		/* switch to functions that replicate data */
		pop->persist = obj_rep_persist;
		pop->flush = obj_rep_flush;
		pop->drain = obj_rep_drain;
		pop->memcpy_persist = obj_rep_memcpy_persist;
		pop->memset_persist = obj_rep_memset_persist;
	}

	/* run_id is made unique by incrementing the previous value */
	pop->run_id += 2;
	if (pop->run_id == 0)
		pop->run_id += 2;
	pop->persist(pop, &pop->run_id, sizeof (pop->run_id));

	/*
	 * Use some of the memory pool area for run-time info.  This
	 * run-time state is never loaded from the file, it is always
	 * created here, so no need to worry about byte-order.
	 */
	pop->rdonly = rdonly;
	pop->lanes = NULL;

	pop->uuid_lo = pmemobj_get_uuid_lo(pop);

	if (boot) {
		if ((errno = pmemobj_boot(pop)) != 0)
			return -1;

		if ((errno = cuckoo_insert(pools_ht, pop->uuid_lo, pop)) != 0) {
			ERR("!cuckoo_insert");
			return -1;
		}

		if ((errno = ctree_insert(pools_tree, (uint64_t)pop, pop->size))
				!= 0) {
			ERR("!ctree_insert");
			return -1;
		}
	}

	/*
	 * If possible, turn off all permissions on the pool header page.
	 *
	 * The prototype PMFS doesn't allow this when large pages are in
	 * use. It is not considered an error if this fails.
	 */
	util_range_none(pop->addr, sizeof (struct pool_hdr));

	return 0;
}

/*
 * pmemobj_create -- create a transactional memory pool (set)
 */
PMEMobjpool *
pmemobj_create(const char *path, const char *layout, size_t poolsize,
		mode_t mode)
{
	LOG(3, "path %s layout %s poolsize %zu mode %o",
			path, layout, poolsize, mode);

	/* check length of layout */
	if (layout && (strlen(layout) >= PMEMOBJ_MAX_LAYOUT)) {
		ERR("Layout too long");
		errno = EINVAL;
		return NULL;
	}

	struct pool_set *set;

	if (util_pool_create(&set, path, poolsize, PMEMOBJ_MIN_POOL,
			OBJ_HDR_SIG, OBJ_FORMAT_MAJOR,
			OBJ_FORMAT_COMPAT, OBJ_FORMAT_INCOMPAT,
			OBJ_FORMAT_RO_COMPAT) != 0) {
		LOG(2, "cannot create pool or pool set");
		return NULL;
	}

	ASSERT(set->nreplicas > 0);

	PMEMobjpool *pop;
	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *rep = set->replica[r];
		pop = rep->part[0].addr;

		VALGRIND_REMOVE_PMEM_MAPPING(&pop->addr,
			sizeof (struct pmemobjpool) -
			((uintptr_t)&pop->addr - (uintptr_t)&pop->hdr));

		pop->addr = pop;
		pop->size = rep->repsize;

		/* create pool descriptor */
		if (pmemobj_descr_create(pop, layout, set->poolsize) != 0) {
			LOG(2, "descriptor creation failed");
			goto err;
		}

		/* initialize replica runtime - is_pmem, funcs, ... */
		if (pmemobj_replica_init(pop, rep->is_pmem) != 0) {
			ERR("pool initialization failed");
			goto err;
		}

		/* link replicas */
		if (r < set->nreplicas - 1)
			pop->replica = set->replica[r + 1]->part[0].addr;
	}

	pop = set->replica[0]->part[0].addr;
	pop->is_master_replica = 1;

	for (unsigned r = 1; r < set->nreplicas; r++) {
		PMEMobjpool *rep = set->replica[r]->part[0].addr;
		rep->is_master_replica = 0;
	}

	VALGRIND_DO_CREATE_MEMPOOL(pop, 0, 0);

	/* initialize runtime parts - lanes, obj stores, ... */
	if (pmemobj_runtime_init(pop, 0, 1 /* boot*/) != 0) {
		ERR("pool initialization failed");
		goto err;
	}

	if (util_poolset_chmod(set, mode))
		goto err;

	util_poolset_fdclose(set);

	util_poolset_free(set);

	LOG(3, "pop %p", pop);

	return pop;

err:
	LOG(4, "error clean up");
	int oerrno = errno;
	util_poolset_close(set, 1);
	errno = oerrno;
	return NULL;
}

/*
 * pmemobj_check_basic -- (internal) basic pool consistency check
 *
 * Used to check if all the replicas are consistent prior to pool recovery.
 */
static int
pmemobj_check_basic(PMEMobjpool *pop)
{
	LOG(3, "pop %p", pop);

	int consistent = 1;

	if (pop->run_id % 2) {
		ERR("invalid run_id %ju", pop->run_id);
		consistent = 0;
	}

	if ((errno = lane_check(pop)) != 0) {
		LOG(2, "!lane_check");
		consistent = 0;
	}

	if ((errno = heap_check(pop)) != 0) {
		LOG(2, "!heap_check");
		consistent = 0;
	}

	return consistent;
}

/*
 * pmemobj_open_common -- open a transactional memory pool (set)
 *
 * This routine does all the work, but takes a cow flag so internal
 * calls can map a read-only pool if required.
 */
static PMEMobjpool *
pmemobj_open_common(const char *path, const char *layout, int cow, int boot)
{
	LOG(3, "path %s layout %s cow %d", path, layout, cow);

	struct pool_set *set;

	if (util_pool_open(&set, path, cow, PMEMOBJ_MIN_POOL,
			OBJ_HDR_SIG, OBJ_FORMAT_MAJOR,
			OBJ_FORMAT_COMPAT, OBJ_FORMAT_INCOMPAT,
			OBJ_FORMAT_RO_COMPAT) != 0) {
		LOG(2, "cannot open pool or pool set");
		return NULL;
	}

	ASSERT(set->nreplicas > 0);

	/* read-only mode is not supported in libpmemobj */
	if (set->rdonly) {
		ERR("read-only mode is not supported");
		errno = EINVAL;
		goto err;
	}

	PMEMobjpool *pop;
	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *rep = set->replica[r];
		pop = rep->part[0].addr;

		VALGRIND_REMOVE_PMEM_MAPPING(&pop->addr,
			sizeof (struct pmemobjpool) -
			((uintptr_t)&pop->addr - (uintptr_t)&pop->hdr));

		pop->addr = pop;
		pop->size = rep->repsize;

		if (pmemobj_descr_check(pop, layout, set->poolsize) != 0) {
			LOG(2, "descriptor check failed");
			goto err;
		}

		/* initialize replica runtime - is_pmem, funcs, ... */
		if (pmemobj_replica_init(pop, rep->is_pmem) != 0) {
			ERR("pool initialization failed");
			goto err;
		}

		/* link replicas */
		if (r < set->nreplicas - 1)
			pop->replica = set->replica[r + 1]->part[0].addr;
	}

	/*
	 * If there is more than one replica, check if all of them are
	 * consistent (recoverable).
	 * On success, choose any replica and copy entire lanes (redo logs)
	 * to all the other replicas to synchronize them.
	 */
	if (set->nreplicas > 1) {
		for (unsigned r = 0; r < set->nreplicas; r++) {
			pop = set->replica[r]->part[0].addr;
			if (pmemobj_check_basic(pop) == 0) {
				ERR("inconsistent replica #%u", r);
				goto err;
			}
		}

		/* copy lanes */
		pop = set->replica[0]->part[0].addr;
		void *src = (void *)((uintptr_t)pop + pop->lanes_offset);
		size_t len = pop->nlanes * sizeof (struct lane_layout);

		for (unsigned r = 1; r < set->nreplicas; r++) {
			pop = set->replica[r]->part[0].addr;
			void *dst = (void *)((uintptr_t)pop +
						pop->lanes_offset);
			pop->memcpy_persist_local(dst, src, len);
		}
	}

	pop = set->replica[0]->part[0].addr;
	pop->is_master_replica = 1;

	for (unsigned r = 1; r < set->nreplicas; r++) {
		PMEMobjpool *rep = set->replica[r]->part[0].addr;
		rep->is_master_replica = 0;
	}

#ifdef USE_VG_MEMCHECK
	heap_vg_open(pop);
#endif

	VALGRIND_DO_CREATE_MEMPOOL(pop, 0, 0);

	/* initialize runtime parts - lanes, obj stores, ... */
	if (pmemobj_runtime_init(pop, 0, boot) != 0) {
		ERR("pool initialization failed");
		goto err;
	}

	util_poolset_fdclose(set);
	util_poolset_free(set);

#ifdef USE_VG_MEMCHECK
	if (boot)
		pmemobj_vg_boot(pop);
#endif

	LOG(3, "pop %p", pop);

	return pop;

err:
	LOG(4, "error clean up");
	int oerrno = errno;
	util_poolset_close(set, 0);
	errno = oerrno;
	return NULL;
}

/*
 * pmemobj_open -- open a transactional memory pool
 */
PMEMobjpool *
pmemobj_open(const char *path, const char *layout)
{
	LOG(3, "path %s layout %s", path, layout);

	return pmemobj_open_common(path, layout, Open_cow, 1);
}

/*
 * pmemobj_cleanup -- (internal) cleanup the pool and unmap
 */
static void
pmemobj_cleanup(PMEMobjpool *pop)
{
	LOG(3, "pop %p", pop);

	heap_cleanup(pop);

	lane_cleanup(pop);

	VALGRIND_DO_DESTROY_MEMPOOL(pop);

	/* unmap all the replicas */
	PMEMobjpool *rep;
	do {
		rep = pop->replica;
		VALGRIND_REMOVE_PMEM_MAPPING(pop->addr, pop->size);
		util_unmap(pop->addr, pop->size);
		pop = rep;
	} while (pop);
}

/*
 * pmemobj_close -- close a transactional memory pool
 */
void
pmemobj_close(PMEMobjpool *pop)
{
	LOG(3, "pop %p", pop);

	_pobj_cache_invalidate++;

	if (cuckoo_remove(pools_ht, pop->uuid_lo) != pop) {
		ERR("cuckoo_remove");
	}

	if (ctree_remove(pools_tree, (uint64_t)pop, 1) != (uint64_t)pop) {
		ERR("ctree_remove");
	}

#ifndef WIN32

	if (_pobj_cached_pool.pop == pop) {
		_pobj_cached_pool.pop = NULL;
		_pobj_cached_pool.uuid_lo = 0;
	}

#else /* WIN32 */

	struct _pobj_pcache *pcache = pthread_getspecific(Cached_pool_key);
	if (pcache != NULL) {
		if (pcache->pop == pop) {
			pcache->pop = NULL;
			pcache->uuid_lo = 0;
		}
	}

#endif /* WIN32 */

	pmemobj_cleanup(pop);
}

/*
 * pmemobj_check -- transactional memory pool consistency check
 */
int
pmemobj_check(const char *path, const char *layout)
{
	LOG(3, "path %s layout %s", path, layout);

	PMEMobjpool *pop = pmemobj_open_common(path, layout, 1, 0);
	if (pop == NULL)
		return -1;	/* errno set by pmemobj_open_common() */

	int consistent = 1;

	/*
	 * For replicated pools, basic consistency check is performed
	 * in pmemobj_open_common().
	 */
	if (pop->replica == NULL)
		consistent = pmemobj_check_basic(pop);

	if (consistent && (errno = pmemobj_boot(pop)) != 0) {
		LOG(3, "!pmemobj_boot");
		consistent = 0;
	}

	if (consistent) {
		pmemobj_cleanup(pop);
	} else {
		/* unmap all the replicas */
		PMEMobjpool *rep;
		do {
			rep = pop->replica;
			VALGRIND_REMOVE_PMEM_MAPPING(pop->addr, pop->size);
			util_unmap(pop->addr, pop->size);
			pop = rep;
		} while (pop);
	}

	if (consistent)
		LOG(4, "pool consistency check OK");

	return consistent;
}

/*
 * pmemobj_pool_by_oid -- returns the pool handle associated with the oid
 */
PMEMobjpool *
pmemobj_pool_by_oid(PMEMoid oid)
{
	LOG(3, "oid.off 0x%016jx", oid.off);

	return cuckoo_get(pools_ht, oid.pool_uuid_lo);
}

/*
 * pmemobj_pool_by_ptr -- returns the pool handle associated with the address
 */
PMEMobjpool *
pmemobj_pool_by_ptr(const void *addr)
{
	LOG(3, "addr %p", addr);

	uint64_t key = (uint64_t)addr;
	size_t pool_size = ctree_find_le(pools_tree, &key);

	if (pool_size == 0)
		return NULL;

	ASSERT((uint64_t)addr >= key);
	uint64_t addr_off = (uint64_t)addr - key;

	if (pool_size <= addr_off)
		return NULL;

	return (PMEMobjpool *)key;
}

/* arguments for constructor_alloc_bytype */
struct carg_bytype {
	type_num_t user_type;
	int zero_init;
	pmemobj_constr constructor;
	void *arg;
};

/*
 * constructor_alloc_bytype -- (internal) constructor for obj_alloc_construct
 */
static int
constructor_alloc_bytype(PMEMobjpool *pop, void *ptr,
	size_t usable_size, void *arg)
{
	LOG(3, "pop %p ptr %p arg %p", pop, ptr, arg);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct oob_header *pobj = OOB_HEADER_FROM_PTR(ptr);
	struct carg_bytype *carg = arg;

	/* zero-initialize OOB list, could be replaced with nodrain variant */
	pop->memset_persist(pop, &pobj->oob, 0, sizeof (pobj->oob));

	pobj->type_num = carg->user_type;
	pobj->size = 0;

	pop->flush(pop, pobj, sizeof (*pobj));

	if (carg->zero_init)
		pop->memset_persist(pop, ptr, 0, usable_size);
	else
		pop->drain(pop);

	int ret = 0;
	if (carg->constructor)
		ret = carg->constructor(pop, ptr, carg->arg);
	return ret;
}

/*
 * obj_alloc_construct -- (internal) allocates a new object with constructor
 */
static int
obj_alloc_construct(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
	type_num_t type_num, int zero_init,
	pmemobj_constr constructor,
	void *arg)
{
	if (size > PMEMOBJ_MAX_ALLOC_SIZE) {
		ERR("requested size too large");
		errno = ENOMEM;
		return -1;
	}

	struct carg_bytype carg;

	carg.user_type = type_num;
	carg.zero_init = zero_init;
	carg.constructor = constructor;
	carg.arg = arg;

	struct operation_entry e = {&oidp->pool_uuid_lo, pop->uuid_lo,
		OPERATION_SET};

	return palloc_operation(pop, 0, oidp != NULL ? &oidp->off : NULL,
		size + OBJ_OOB_SIZE,
		constructor_alloc_bytype, &carg,
		oidp != NULL ? &e : NULL, oidp != NULL ? 1 : 0);
}

/*
 * pmemobj_alloc -- allocates a new object
 */
int
pmemobj_alloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
	uint64_t type_num, pmemobj_constr constructor, void *arg)
{
	LOG(3, "pop %p oidp %p size %zu type_num %llx constructor %p arg %p",
		pop, oidp, size, (unsigned long long)type_num,
		constructor, arg);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();

	if (size == 0) {
		ERR("allocation with size 0");
		errno = EINVAL;
		return -1;
	}

	return obj_alloc_construct(pop, oidp, size, type_num,
			0, constructor, arg);
}

/* arguments for constructor_realloc and constructor_zrealloc */
struct carg_realloc {
	void *ptr;
	size_t old_size;
	size_t new_size;
	int zero_init;
	type_num_t user_type;
	pmemobj_constr constructor;
	void *arg;
};

/*
 * pmemobj_zalloc -- allocates a new zeroed object
 */
int
pmemobj_zalloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
		uint64_t type_num)
{
	LOG(3, "pop %p oidp %p size %zu type_num %llx",
			pop, oidp, size, (unsigned long long)type_num);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();

	if (size == 0) {
		ERR("allocation with size 0");
		errno = EINVAL;
		return -1;
	}

	return obj_alloc_construct(pop, oidp, size, type_num,
					1, NULL, NULL);
}

/*
 * obj_free -- (internal) free an object
 */
static void
obj_free(PMEMobjpool *pop, PMEMoid *oidp)
{
	ASSERT(oidp != NULL);

	struct operation_entry e = {&oidp->pool_uuid_lo, 0, OPERATION_SET};
	palloc_operation(pop, oidp->off, &oidp->off, 0, NULL, NULL, &e, 1);
}

/*
 * constructor_realloc -- (internal) constructor for pmemobj_realloc
 */
static int
constructor_realloc(PMEMobjpool *pop, void *ptr, size_t usable_size, void *arg)
{
	LOG(3, "pop %p ptr %p arg %p", pop, ptr, arg);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct carg_realloc *carg = arg;
	struct oob_header *pobj = OOB_HEADER_FROM_PTR(ptr);

	/* zero-initialize OOB list, could be replaced with nodrain variant */
	pop->memset_persist(pop, &pobj->oob, 0, sizeof (pobj->oob));

	if (ptr != carg->ptr) {
		pobj->type_num = carg->user_type;
		pobj->size = 0;
		pop->flush(pop, pobj, sizeof (*pobj));
	}

	if (!carg->zero_init)
		return 0;

	if (usable_size > carg->old_size) {
		size_t grow_len = usable_size - carg->old_size;
		void *new_data_ptr = (void *)((uintptr_t)ptr + carg->old_size);

		pop->memset_persist(pop, new_data_ptr, 0, grow_len);
	}

	return 0;
}

/*
 * obj_realloc_common -- (internal) common routine for resizing
 *                          existing objects
 */
static int
obj_realloc_common(PMEMobjpool *pop,
	PMEMoid *oidp, size_t size, type_num_t type_num, int zero_init)
{
	/* if OID is NULL just allocate memory */
	if (OBJ_OID_IS_NULL(*oidp)) {
		/* if size is 0 - do nothing */
		if (size == 0)
			return 0;

		return obj_alloc_construct(pop, oidp, size, type_num,
				zero_init, NULL, NULL);
	}

	if (size > PMEMOBJ_MAX_ALLOC_SIZE) {
		ERR("requested size too large");
		errno = ENOMEM;
		return -1;
	}

	/* if size is 0 just free */
	if (size == 0) {
		obj_free(pop, oidp);
		return 0;
	}

	struct oob_header *pobj = OOB_HEADER_FROM_OID(pop, *oidp);
	type_num_t user_type_old = pobj->type_num;

	struct carg_realloc carg;
	carg.ptr = OBJ_OFF_TO_PTR(pop, oidp->off);
	carg.new_size = size;
	carg.old_size = pmemobj_alloc_usable_size(*oidp);
	carg.user_type = type_num;
	carg.constructor = NULL;
	carg.arg = NULL;
	carg.zero_init = zero_init;

	int ret;
	if (type_num == user_type_old) {
		ret = palloc_operation(pop, oidp->off, &oidp->off,
			size + OBJ_OOB_SIZE,
			constructor_realloc, &carg, NULL, 0);
	} else {
		struct operation_entry entry = {&pobj->type_num, type_num,
			OPERATION_SET};
		ret = palloc_operation(pop, oidp->off, &oidp->off,
			size + OBJ_OOB_SIZE,
			constructor_realloc, &carg, &entry, 1);
	}

	return ret;
}

/*
 * constructor_zrealloc_root -- (internal) constructor for pmemobj_root
 */
static int
constructor_zrealloc_root(PMEMobjpool *pop, void *ptr,
	size_t usable_size, void *arg)
{
	LOG(3, "pop %p ptr %p arg %p", pop, ptr, arg);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct carg_realloc *carg = arg;

	VALGRIND_ADD_TO_TX(OOB_HEADER_FROM_PTR(ptr),
		usable_size + OBJ_OOB_SIZE);

	struct oob_header *pobj = OOB_HEADER_FROM_PTR(ptr);

	constructor_realloc(pop, ptr, usable_size, arg);
	if (ptr != carg->ptr) {
		pobj->size = carg->new_size | OBJ_INTERNAL_OBJECT_MASK;
		pop->flush(pop, &pobj->size, sizeof (pobj->size));
	}

	int ret = 0;
	if (carg->constructor)
		ret = carg->constructor(pop, ptr, carg->arg);

	VALGRIND_REMOVE_FROM_TX(OOB_HEADER_FROM_PTR(ptr),
		carg->new_size + OBJ_OOB_SIZE);

	return ret;
}

/*
 * pmemobj_realloc -- resizes an existing object
 */
int
pmemobj_realloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
		uint64_t type_num)
{
	ASSERTne(oidp, NULL);

	LOG(3, "pop %p oid.off 0x%016jx size %zu type_num %lu",
		pop, oidp->off, size, type_num);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();
	ASSERT(OBJ_OID_IS_VALID(pop, *oidp));

	return obj_realloc_common(pop, oidp, size, (type_num_t)type_num, 0);
}

/*
 * pmemobj_zrealloc -- resizes an existing object, any new space is zeroed.
 */
int
pmemobj_zrealloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
		uint64_t type_num)
{
	ASSERTne(oidp, NULL);

	LOG(3, "pop %p oid.off 0x%016jx size %zu type_num %lu",
		pop, oidp->off, size, type_num);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();
	ASSERT(OBJ_OID_IS_VALID(pop, *oidp));

	return obj_realloc_common(pop, oidp, size, (type_num_t)type_num, 1);
}

/* arguments for constructor_strdup */
struct carg_strdup {
	size_t size;
	const char *s;
};

/*
 * constructor_strdup -- (internal) constructor of pmemobj_strdup
 */
static int
constructor_strdup(PMEMobjpool *pop, void *ptr, void *arg)
{
	LOG(3, "pop %p ptr %p arg %p", pop, ptr, arg);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct carg_strdup *carg = arg;

	/* copy string */
	pop->memcpy_persist(pop, ptr, carg->s, carg->size);

	return 0;
}

/*
 * pmemobj_strdup -- allocates a new object with duplicate of the string s.
 */
int
pmemobj_strdup(PMEMobjpool *pop, PMEMoid *oidp, const char *s,
		uint64_t type_num)
{
	LOG(3, "pop %p oidp %p string %s type_num %lu", pop, oidp, s, type_num);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();

	if (NULL == s) {
		errno = EINVAL;
		return -1;
	}

	struct carg_strdup carg;
	carg.size = (strlen(s) + 1) * sizeof (char);
	carg.s = s;

	return obj_alloc_construct(pop, oidp, carg.size,
		(type_num_t)type_num, 0, constructor_strdup, &carg);
}

/*
 * pmemobj_free -- frees an existing object
 */
void
pmemobj_free(PMEMoid *oidp)
{
	ASSERTne(oidp, NULL);

	LOG(3, "oid.off 0x%016jx", oidp->off);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();

	if (oidp->off == 0)
		return;

	PMEMobjpool *pop = pmemobj_pool_by_oid(*oidp);

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

	PMEMobjpool *pop = pmemobj_pool_by_oid(oid);

	ASSERTne(pop, NULL);
	ASSERT(OBJ_OID_IS_VALID(pop, oid));

	return (pmalloc_usable_size(pop, oid.off) - OBJ_OOB_SIZE);
}

/*
 * pmemobj_memcpy_persist -- pmemobj version of memcpy
 */
void *
pmemobj_memcpy_persist(PMEMobjpool *pop, void *dest, const void *src,
	size_t len)
{
	LOG(15, "pop %p dest %p src %p len %zu", pop, dest, src, len);

	return pop->memcpy_persist(pop, dest, src, len);
}

/*
 * pmemobj_memset_persist -- pmemobj version of memset
 */
void *
pmemobj_memset_persist(PMEMobjpool *pop, void *dest, int c, size_t len)
{
	LOG(15, "pop %p dest %p c 0x%02x len %zu", pop, dest, c, len);

	return pop->memset_persist(pop, dest, c, len);
}

/*
 * pmemobj_persist -- pmemobj version of pmem_persist
 */
void
pmemobj_persist(PMEMobjpool *pop, const void *addr, size_t len)
{
	LOG(15, "pop %p addr %p len %zu", pop, addr, len);

	pop->persist(pop, addr, len);
}

/*
 * pmemobj_flush -- pmemobj version of pmem_flush
 */
void
pmemobj_flush(PMEMobjpool *pop, const void *addr, size_t len)
{
	LOG(15, "pop %p addr %p len %zu", pop, addr, len);

	pop->flush(pop, addr, len);
}

/*
 * pmemobj_drain -- pmemobj version of pmem_drain
 */
void
pmemobj_drain(PMEMobjpool *pop)
{
	LOG(15, "pop %p", pop);

	pop->drain(pop);
}

/*
 * pmemobj_type_num -- returns type number of object
 */
uint64_t
pmemobj_type_num(PMEMoid oid)
{
	LOG(3, "oid.off 0x%016jx", oid.off);

	ASSERT(!OID_IS_NULL(oid));

	void *ptr = pmemobj_direct(oid);

	struct oob_header *oobh = OOB_HEADER_FROM_PTR(ptr);
	return oobh->type_num;
}

/* arguments for constructor_alloc_root */
struct carg_root {
	size_t size;
	pmemobj_constr constructor;
	void *arg;
};

/*
 * constructor_alloc_root -- (internal) constructor for obj_alloc_root
 */
static int
constructor_alloc_root(PMEMobjpool *pop, void *ptr,
	size_t usable_size, void *arg)
{
	LOG(3, "pop %p ptr %p arg %p", pop, ptr, arg);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	int ret = 0;

	struct oob_header *ro = OOB_HEADER_FROM_PTR(ptr);
	struct carg_root *carg = arg;

	/* temporarily add atomic root allocation to pmemcheck transaction */
	VALGRIND_ADD_TO_TX(ro, OBJ_OOB_SIZE + usable_size);

	/* zero-initialize OOB list */
	pop->memset_persist(pop, &ro->oob, 0, sizeof (ro->oob));

	if (carg->constructor)
		ret = carg->constructor(pop, ptr, carg->arg);
	else
		pop->memset_persist(pop, ptr, 0, usable_size);

	ro->type_num = POBJ_ROOT_TYPE_NUM;
	ro->size = carg->size | OBJ_INTERNAL_OBJECT_MASK;

	VALGRIND_REMOVE_FROM_TX(ro, OBJ_OOB_SIZE + usable_size);

	pop->persist(pop, &ro->size,
		/* there's no padding between these, so we can add sizes */
		sizeof (ro->size) + sizeof (ro->type_num));

	return ret;
}

/*
 * obj_alloc_root -- (internal) allocate root object
 */
static int
obj_alloc_root(PMEMobjpool *pop, size_t size,
	pmemobj_constr constructor, void *arg)
{
	LOG(3, "pop %p size %zu", pop, size);

	struct carg_root carg;

	carg.size = size;
	carg.constructor = constructor;
	carg.arg = arg;

	return pmalloc_construct(pop, &pop->root_offset,
		size + OBJ_OOB_SIZE, constructor_alloc_root, &carg);
}

/*
 * obj_realloc_root -- (internal) reallocate root object
 */
static int
obj_realloc_root(PMEMobjpool *pop, size_t size,
	size_t old_size,
	pmemobj_constr constructor, void *arg)
{
	LOG(3, "pop %p size %zu old_size %zu", pop, size, old_size);

	struct carg_realloc carg;

	carg.ptr = OBJ_OFF_TO_PTR(pop, pop->root_offset);
	carg.old_size = old_size;
	carg.new_size = size;
	carg.user_type = POBJ_ROOT_TYPE_NUM;
	carg.constructor = constructor;
	carg.zero_init = 1;
	carg.arg = arg;

	return palloc_operation(pop, pop->root_offset, &pop->root_offset,
		size + OBJ_OOB_SIZE, constructor_zrealloc_root, &carg, NULL, 0);
}

/*
 * pmemobj_root_size -- returns size of the root object
 */
size_t
pmemobj_root_size(PMEMobjpool *pop)
{
	LOG(3, "pop %p", pop);

	if (pop->root_offset) {
		struct oob_header *ro =
			OOB_HEADER_FROM_OFF(pop, pop->root_offset);
		return ro->size & ~OBJ_INTERNAL_OBJECT_MASK;
	} else
		return 0;
}

/*
 * pmemobj_root_construct -- returns root object
 */
PMEMoid
pmemobj_root_construct(PMEMobjpool *pop, size_t size,
	pmemobj_constr constructor, void *arg)
{
	LOG(3, "pop %p size %zu constructor %p args %p", pop, size, constructor,
		arg);

	if (size > PMEMOBJ_MAX_ALLOC_SIZE) {
		ERR("requested size too large");
		errno = ENOMEM;
		return OID_NULL;
	}

	PMEMoid root;

	pmemobj_mutex_lock_nofail(pop, &pop->rootlock);

	if (pop->root_offset == 0)
		obj_alloc_root(pop, size, constructor, arg);
	else {
		size_t old_size = pmemobj_root_size(pop);
		if (size > old_size && obj_realloc_root(pop, size,
				old_size, constructor, arg)) {
			pmemobj_mutex_unlock_nofail(pop, &pop->rootlock);
			LOG(2, "obj_realloc_root failed");
			return OID_NULL;
		}
	}
	root.pool_uuid_lo = pop->uuid_lo;
	root.off = pop->root_offset;

	pmemobj_mutex_unlock_nofail(pop, &pop->rootlock);
	return root;
}

/*
 * pmemobj_root -- returns root object
 */
PMEMoid
pmemobj_root(PMEMobjpool *pop, size_t size)
{
	LOG(3, "pop %p size %zu", pop, size);

	return pmemobj_root_construct(pop, size, NULL, NULL);
}

/*
 * pmemobj_first - returns first object of specified type
 */
PMEMoid
pmemobj_first(PMEMobjpool *pop)
{
	LOG(3, "pop %p", pop);

	PMEMoid ret = {0, 0};

	uint64_t off = pmalloc_first(pop);
	if (off != 0) {
		ret.off = off + OBJ_OOB_SIZE;
		ret.pool_uuid_lo = pop->uuid_lo;

		struct oob_header *oobh = OOB_HEADER_FROM_OFF(pop, ret.off);
		if (oobh->size & OBJ_INTERNAL_OBJECT_MASK) {
			return pmemobj_next(ret);
		}
	}

	return ret;
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

	PMEMobjpool *pop = pmemobj_pool_by_oid(oid);

	ASSERTne(pop, NULL);
	ASSERT(OBJ_OID_IS_VALID(pop, oid));

	PMEMoid ret = {0, 0};
	uint64_t off = pmalloc_next(pop, oid.off);
	if (off != 0) {
		ret.off = off + OBJ_OOB_SIZE;
		ret.pool_uuid_lo = pop->uuid_lo;

		struct oob_header *oobh = OOB_HEADER_FROM_OFF(pop, ret.off);
		if (oobh->size & OBJ_INTERNAL_OBJECT_MASK) {
			return pmemobj_next(ret);
		}
	}

	return ret;
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

	if (pe_offset >= pop->size) {
		ERR("pe_offset (%lu) too big", pe_offset);
		return EINVAL;
	}

	return list_insert(pop, (ssize_t)pe_offset, head, dest, before, oid);
}

/*
 * pmemobj_list_insert_new -- adds new object to a list
 */
PMEMoid
pmemobj_list_insert_new(PMEMobjpool *pop, size_t pe_offset, void *head,
			PMEMoid dest, int before, size_t size,
			uint64_t type_num,
			pmemobj_constr constructor, void *arg)
{
	LOG(3, "pop %p pe_offset %zu head %p dest.off 0x%016jx before %d"
	    " size %zu type_num %lu",
	    pop, pe_offset, head, dest.off, before, size, type_num);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();
	ASSERT(OBJ_OID_IS_VALID(pop, dest));

	if (size > PMEMOBJ_MAX_ALLOC_SIZE) {
		ERR("requested size too large");
		errno = ENOMEM;
		return OID_NULL;
	}

	if (pe_offset >= pop->size) {
		ERR("pe_offset (%lu) too big", pe_offset);
		errno = EINVAL;
		return OID_NULL;
	}

	struct list_head *lhead = NULL;
	struct carg_bytype carg;

	carg.user_type = (type_num_t)type_num;
	carg.constructor = constructor;
	carg.arg = arg;
	carg.zero_init = 0;

	PMEMoid retoid = OID_NULL;
	list_insert_new_user(pop, lhead,
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

	if (pe_offset >= pop->size) {
		ERR("pe_offset (%lu) too big", pe_offset);
		return EINVAL;
	}

	if (free) {
		return list_remove_free_user(pop, NULL, pe_offset, head, &oid);
	} else {
		return list_remove(pop, (ssize_t)pe_offset, head, oid);
	}
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

	if (pe_old_offset >= pop->size) {
		ERR("pe_old_offset (%lu) too big", pe_old_offset);
		return EINVAL;
	}

	if (pe_new_offset >= pop->size) {
		ERR("pe_new_offset (%lu) too big", pe_new_offset);
		return EINVAL;
	}

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


#ifdef WIN32
/*
 * libpmemobj constructor/destructor functions
 */
MSVC_CONSTR(libpmemobj_init)
MSVC_DESTR(libpmemobj_fini)
#endif
