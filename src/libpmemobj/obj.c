/*
 * Copyright 2014-2019, Intel Corporation
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
#include <inttypes.h>
#include <limits.h>
#include <wchar.h>
#include <stdbool.h>

#include "valgrind_internal.h"
#include "libpmem.h"
#include "memblock.h"
#include "critnib.h"
#include "list.h"
#include "mmap.h"
#include "obj.h"
#include "ctl_global.h"
#include "ravl.h"

#include "heap_layout.h"
#include "os.h"
#include "os_thread.h"
#include "pmemops.h"
#include "set.h"
#include "sync.h"
#include "tx.h"
#include "sys_util.h"

/*
 * The variable from which the config is directly loaded. The string
 * cannot contain any comments or extraneous white characters.
 */
#define OBJ_CONFIG_ENV_VARIABLE "PMEMOBJ_CONF"

/*
 * The variable that points to a config file from which the config is loaded.
 */
#define OBJ_CONFIG_FILE_ENV_VARIABLE "PMEMOBJ_CONF_FILE"

/*
 * The variable which overwrites a number of lanes available at runtime.
 */
#define OBJ_NLANES_ENV_VARIABLE "PMEMOBJ_NLANES"

#define OBJ_X_VALID_FLAGS PMEMOBJ_F_RELAXED

static const struct pool_attr Obj_create_attr = {
		OBJ_HDR_SIG,
		OBJ_FORMAT_MAJOR,
		OBJ_FORMAT_FEAT_DEFAULT,
		{0}, {0}, {0}, {0}, {0}
};

static const struct pool_attr Obj_open_attr = {
		OBJ_HDR_SIG,
		OBJ_FORMAT_MAJOR,
		OBJ_FORMAT_FEAT_CHECK,
		{0}, {0}, {0}, {0}, {0}
};

static struct critnib *pools_ht; /* hash table used for searching by UUID */
static struct critnib *pools_tree; /* tree used for searching by address */

int _pobj_cache_invalidate;

#ifndef _WIN32

__thread struct _pobj_pcache _pobj_cached_pool;

/*
 * pmemobj_direct -- returns the direct pointer of an object
 */
void *
pmemobj_direct(PMEMoid oid)
{
	return pmemobj_direct_inline(oid);
}

#else /* _WIN32 */

/*
 * XXX - this is a temporary implementation
 *
 * Seems like we could still use TLS and simply substitute "__thread" with
 * "__declspec(thread)", however it's not clear if it would work correctly
 * with Windows DLL's.
 * Need to verify that once we have the multi-threaded tests ported.
 */

struct _pobj_pcache {
	PMEMobjpool *pop;
	uint64_t uuid_lo;
	int invalidate;
};

static os_once_t Cached_pool_key_once = OS_ONCE_INIT;
static os_tls_key_t Cached_pool_key;

/*
 * _Cached_pool_key_alloc -- (internal) allocate pool cache pthread key
 */
static void
_Cached_pool_key_alloc(void)
{
	int pth_ret = os_tls_key_create(&Cached_pool_key, free);
	if (pth_ret)
		FATAL("!os_tls_key_create");
}

/*
 * pmemobj_direct -- returns the direct pointer of an object
 */
void *
pmemobj_direct(PMEMoid oid)
{
	if (oid.off == 0 || oid.pool_uuid_lo == 0)
		return NULL;

	struct _pobj_pcache *pcache = os_tls_get(Cached_pool_key);
	if (pcache == NULL) {
		pcache = calloc(sizeof(struct _pobj_pcache), 1);
		if (pcache == NULL)
			FATAL("!pcache malloc");
		int ret = os_tls_set(Cached_pool_key, pcache);
		if (ret)
			FATAL("!os_tls_set");
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

#endif /* _WIN32 */

/*
 * obj_ctl_init_and_load -- (static) initializes CTL and loads configuration
 *	from env variable and file
 */
static int
obj_ctl_init_and_load(PMEMobjpool *pop)
{
	LOG(3, "pop %p", pop);

	if (pop != NULL && (pop->ctl = ctl_new()) == NULL) {
		LOG(2, "!ctl_new");
		return -1;
	}

	if (pop) {
		tx_ctl_register(pop);
		pmalloc_ctl_register(pop);
		stats_ctl_register(pop);
		debug_ctl_register(pop);
	}

	char *env_config = os_getenv(OBJ_CONFIG_ENV_VARIABLE);
	if (env_config != NULL) {
		if (ctl_load_config_from_string(pop ? pop->ctl : NULL,
				pop, env_config) != 0) {
			LOG(2, "unable to parse config stored in %s "
				"environment variable",
				OBJ_CONFIG_ENV_VARIABLE);
			goto err;
		}
	}

	char *env_config_file = os_getenv(OBJ_CONFIG_FILE_ENV_VARIABLE);
	if (env_config_file != NULL && env_config_file[0] != '\0') {
		if (ctl_load_config_from_file(pop ? pop->ctl : NULL,
				pop, env_config_file) != 0) {
			LOG(2, "unable to parse config stored in %s "
				"file (from %s environment variable)",
				env_config_file,
				OBJ_CONFIG_FILE_ENV_VARIABLE);
			goto err;
		}
	}

	return 0;
err:
	if (pop)
		ctl_delete(pop->ctl);
	return -1;
}

/*
 * obj_pool_init -- (internal) allocate global structs holding all opened pools
 *
 * This is invoked on a first call to pmemobj_open() or pmemobj_create().
 * Memory is released in library destructor.
 *
 * This function needs to be threadsafe.
 */
static void
obj_pool_init(void)
{
	LOG(3, NULL);

	struct critnib *c;

	if (pools_ht == NULL) {
		c = critnib_new();
		if (c == NULL)
			FATAL("!critnib_new for pools_ht");
		if (!util_bool_compare_and_swap64(&pools_ht, NULL, c))
			critnib_delete(c);
	}

	if (pools_tree == NULL) {
		c = critnib_new();
		if (c == NULL)
			FATAL("!critnib_new for pools_tree");
		if (!util_bool_compare_and_swap64(&pools_tree, NULL, c))
			critnib_delete(c);
	}
}

/*
 * pmemobj_oid -- return a PMEMoid based on the virtual address
 *
 * If the address does not belong to any pool OID_NULL is returned.
 */
PMEMoid
pmemobj_oid(const void *addr)
{
	PMEMobjpool *pop = pmemobj_pool_by_ptr(addr);
	if (pop == NULL)
		return OID_NULL;

	PMEMoid oid = {pop->uuid_lo, (uintptr_t)addr - (uintptr_t)pop};
	return oid;
}

/*
 * obj_init -- initialization of obj
 *
 * Called by constructor.
 */
void
obj_init(void)
{
	LOG(3, NULL);

	COMPILE_ERROR_ON(sizeof(struct pmemobjpool) !=
		POOL_HDR_SIZE + POOL_DESC_SIZE);

	COMPILE_ERROR_ON(PMEMOBJ_F_MEM_NODRAIN != PMEM_F_MEM_NODRAIN);

	COMPILE_ERROR_ON(PMEMOBJ_F_MEM_NONTEMPORAL != PMEM_F_MEM_NONTEMPORAL);
	COMPILE_ERROR_ON(PMEMOBJ_F_MEM_TEMPORAL != PMEM_F_MEM_TEMPORAL);

	COMPILE_ERROR_ON(PMEMOBJ_F_MEM_WC != PMEM_F_MEM_WC);
	COMPILE_ERROR_ON(PMEMOBJ_F_MEM_WB != PMEM_F_MEM_WB);

	COMPILE_ERROR_ON(PMEMOBJ_F_MEM_NOFLUSH != PMEM_F_MEM_NOFLUSH);


#ifdef _WIN32
	/* XXX - temporary implementation (see above) */
	os_once(&Cached_pool_key_once, _Cached_pool_key_alloc);
#endif
	/*
	 * Load global config, ignore any issues. They will be caught on the
	 * subsequent call to this function for individual pools.
	 */
	ctl_global_register();

	if (obj_ctl_init_and_load(NULL))
		FATAL("error: %s", pmemobj_errormsg());

	lane_info_boot();

	util_remote_init();
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

	if (pools_ht)
		critnib_delete(pools_ht);
	if (pools_tree)
		critnib_delete(pools_tree);
	lane_info_destroy();
	util_remote_fini();

#ifdef _WIN32
	(void) os_tls_key_delete(Cached_pool_key);
#endif
}

/*
 * obj_drain_empty -- (internal) empty function for drain on non-pmem memory
 */
static void
obj_drain_empty(void)
{
	/* do nothing */
}

/*
 * obj_nopmem_memcpy -- (internal) memcpy followed by an msync
 */
static void *
obj_nopmem_memcpy(void *dest, const void *src, size_t len, unsigned flags)
{
	LOG(15, "dest %p src %p len %zu flags 0x%x", dest, src, len, flags);

	/*
	 * Use pmem_memcpy instead of memcpy, because pmemobj_memcpy is supposed
	 * to guarantee that multiple of 8 byte stores to 8 byte aligned
	 * addresses are fail safe atomic. pmem_memcpy guarantees that, while
	 * libc memcpy does not.
	 */
	pmem_memcpy(dest, src, len, PMEM_F_MEM_NOFLUSH);
	pmem_msync(dest, len);
	return dest;
}

/*
 * obj_nopmem_memmove -- (internal) memmove followed by an msync
 */
static void *
obj_nopmem_memmove(void *dest, const void *src, size_t len, unsigned flags)
{
	LOG(15, "dest %p src %p len %zu flags 0x%x", dest, src, len, flags);

	/* see comment in obj_nopmem_memcpy */
	pmem_memmove(dest, src, len, PMEM_F_MEM_NOFLUSH);
	pmem_msync(dest, len);
	return dest;
}

/*
 * obj_nopmem_memset -- (internal) memset followed by an msync
 */
static void *
obj_nopmem_memset(void *dest, int c, size_t len, unsigned flags)
{
	LOG(15, "dest %p c 0x%02x len %zu flags 0x%x", dest, c, len, flags);

	/* see comment in obj_nopmem_memcpy */
	pmem_memset(dest, c, len, PMEM_F_MEM_NOFLUSH);
	pmem_msync(dest, len);
	return dest;
}

/*
 * obj_remote_persist -- (internal) remote persist function
 */
static int
obj_remote_persist(PMEMobjpool *pop, const void *addr, size_t len,
			unsigned lane, unsigned flags)
{
	LOG(15, "pop %p addr %p len %zu lane %u flags %u",
		pop, addr, len, lane, flags);

	ASSERTne(pop->rpp, NULL);

	uintptr_t offset = (uintptr_t)addr - pop->remote_base;

	unsigned rpmem_flags = 0;
	if (flags & PMEMOBJ_F_RELAXED)
		rpmem_flags |= RPMEM_PERSIST_RELAXED;

	int rv = Rpmem_persist(pop->rpp, offset, len, lane, rpmem_flags);
	if (rv) {
		ERR("!rpmem_persist(rpp %p offset %zu length %zu lane %u)"
			" FATAL ERROR (returned value %i)",
			pop->rpp, offset, len, lane, rv);
		return -1;
	}

	return 0;
}

/*
 * XXX - Consider removing obj_norep_*() wrappers to call *_local()
 * functions directly.  Alternatively, always use obj_rep_*(), even
 * if there are no replicas.  Verify the performance penalty.
 */

/*
 * obj_norep_memcpy -- (internal) memcpy w/o replication
 */
static void *
obj_norep_memcpy(void *ctx, void *dest, const void *src, size_t len,
		unsigned flags)
{
	PMEMobjpool *pop = ctx;
	LOG(15, "pop %p dest %p src %p len %zu flags 0x%x", pop, dest, src, len,
			flags);

	return pop->memcpy_local(dest, src, len,
					flags & PMEM_F_MEM_VALID_FLAGS);
}

/*
 * obj_norep_memmove -- (internal) memmove w/o replication
 */
static void *
obj_norep_memmove(void *ctx, void *dest, const void *src, size_t len,
		unsigned flags)
{
	PMEMobjpool *pop = ctx;
	LOG(15, "pop %p dest %p src %p len %zu flags 0x%x", pop, dest, src, len,
			flags);

	return pop->memmove_local(dest, src, len,
					flags & PMEM_F_MEM_VALID_FLAGS);
}

/*
 * obj_norep_memset -- (internal) memset w/o replication
 */
static void *
obj_norep_memset(void *ctx, void *dest, int c, size_t len, unsigned flags)
{
	PMEMobjpool *pop = ctx;
	LOG(15, "pop %p dest %p c 0x%02x len %zu flags 0x%x", pop, dest, c, len,
			flags);

	return pop->memset_local(dest, c, len, flags & PMEM_F_MEM_VALID_FLAGS);
}

/*
 * obj_norep_persist -- (internal) persist w/o replication
 */
static int
obj_norep_persist(void *ctx, const void *addr, size_t len, unsigned flags)
{
	PMEMobjpool *pop = ctx;
	LOG(15, "pop %p addr %p len %zu", pop, addr, len);

	pop->persist_local(addr, len);

	return 0;
}

/*
 * obj_norep_flush -- (internal) flush w/o replication
 */
static int
obj_norep_flush(void *ctx, const void *addr, size_t len, unsigned flags)
{
	PMEMobjpool *pop = ctx;
	LOG(15, "pop %p addr %p len %zu", pop, addr, len);

	pop->flush_local(addr, len);

	return 0;
}

/*
 * obj_norep_drain -- (internal) drain w/o replication
 */
static void
obj_norep_drain(void *ctx)
{
	PMEMobjpool *pop = ctx;
	LOG(15, "pop %p", pop);

	pop->drain_local();
}

static void obj_pool_cleanup(PMEMobjpool *pop);

/*
 * obj_handle_remote_persist_error -- (internal) handle remote persist
 *                                    fatal error
 */
static void
obj_handle_remote_persist_error(PMEMobjpool *pop)
{
	LOG(1, "pop %p", pop);

	ERR("error clean up...");
	obj_pool_cleanup(pop);

	FATAL("Fatal error of remote persist. Aborting...");
}

/*
 * obj_rep_memcpy -- (internal) memcpy with replication
 */
static void *
obj_rep_memcpy(void *ctx, void *dest, const void *src, size_t len,
		unsigned flags)
{
	PMEMobjpool *pop = ctx;
	LOG(15, "pop %p dest %p src %p len %zu flags 0x%x", pop, dest, src, len,
			flags);

	unsigned lane = UINT_MAX;

	if (pop->has_remote_replicas)
		lane = lane_hold(pop, NULL);

	void *ret = pop->memcpy_local(dest, src, len, flags);

	PMEMobjpool *rep = pop->replica;
	while (rep) {
		void *rdest = (char *)rep + (uintptr_t)dest - (uintptr_t)pop;
		if (rep->rpp == NULL) {
			rep->memcpy_local(rdest, src, len,
						flags & PMEM_F_MEM_VALID_FLAGS);
		} else {
			if (rep->persist_remote(rep, rdest, len, lane, flags))
				obj_handle_remote_persist_error(pop);
		}
		rep = rep->replica;
	}

	if (pop->has_remote_replicas)
		lane_release(pop);

	return ret;
}

/*
 * obj_rep_memmove -- (internal) memmove with replication
 */
static void *
obj_rep_memmove(void *ctx, void *dest, const void *src, size_t len,
		unsigned flags)
{
	PMEMobjpool *pop = ctx;
	LOG(15, "pop %p dest %p src %p len %zu flags 0x%x", pop, dest, src, len,
			flags);

	unsigned lane = UINT_MAX;

	if (pop->has_remote_replicas)
		lane = lane_hold(pop, NULL);

	void *ret = pop->memmove_local(dest, src, len, flags);

	PMEMobjpool *rep = pop->replica;
	while (rep) {
		void *rdest = (char *)rep + (uintptr_t)dest - (uintptr_t)pop;
		if (rep->rpp == NULL) {
			rep->memmove_local(rdest, src, len,
						flags & PMEM_F_MEM_VALID_FLAGS);
		} else {
			if (rep->persist_remote(rep, rdest, len, lane, flags))
				obj_handle_remote_persist_error(pop);
		}
		rep = rep->replica;
	}

	if (pop->has_remote_replicas)
		lane_release(pop);

	return ret;
}

/*
 * obj_rep_memset -- (internal) memset with replication
 */
static void *
obj_rep_memset(void *ctx, void *dest, int c, size_t len, unsigned flags)
{
	PMEMobjpool *pop = ctx;
	LOG(15, "pop %p dest %p c 0x%02x len %zu flags 0x%x", pop, dest, c, len,
			flags);

	unsigned lane = UINT_MAX;

	if (pop->has_remote_replicas)
		lane = lane_hold(pop, NULL);

	void *ret = pop->memset_local(dest, c, len, flags);

	PMEMobjpool *rep = pop->replica;
	while (rep) {
		void *rdest = (char *)rep + (uintptr_t)dest - (uintptr_t)pop;
		if (rep->rpp == NULL) {
			rep->memset_local(rdest, c, len,
						flags & PMEM_F_MEM_VALID_FLAGS);
		} else {
			if (rep->persist_remote(rep, rdest, len, lane, flags))
				obj_handle_remote_persist_error(pop);
		}
		rep = rep->replica;
	}

	if (pop->has_remote_replicas)
		lane_release(pop);

	return ret;
}

/*
 * obj_rep_persist -- (internal) persist with replication
 */
static int
obj_rep_persist(void *ctx, const void *addr, size_t len, unsigned flags)
{
	PMEMobjpool *pop = ctx;
	LOG(15, "pop %p addr %p len %zu", pop, addr, len);

	unsigned lane = UINT_MAX;

	if (pop->has_remote_replicas)
		lane = lane_hold(pop, NULL);

	pop->persist_local(addr, len);

	PMEMobjpool *rep = pop->replica;
	while (rep) {
		void *raddr = (char *)rep + (uintptr_t)addr - (uintptr_t)pop;
		if (rep->rpp == NULL) {
			rep->memcpy_local(raddr, addr, len, 0);
		} else {
			if (rep->persist_remote(rep, raddr, len, lane, flags))
				obj_handle_remote_persist_error(pop);
		}
		rep = rep->replica;
	}

	if (pop->has_remote_replicas)
		lane_release(pop);

	return 0;
}

/*
 * obj_rep_flush -- (internal) flush with replication
 */
static int
obj_rep_flush(void *ctx, const void *addr, size_t len, unsigned flags)
{
	PMEMobjpool *pop = ctx;
	LOG(15, "pop %p addr %p len %zu", pop, addr, len);

	unsigned lane = UINT_MAX;

	if (pop->has_remote_replicas)
		lane = lane_hold(pop, NULL);

	pop->flush_local(addr, len);

	PMEMobjpool *rep = pop->replica;
	while (rep) {
		void *raddr = (char *)rep + (uintptr_t)addr - (uintptr_t)pop;
		if (rep->rpp == NULL) {
			rep->memcpy_local(raddr, addr, len,
				PMEM_F_MEM_NODRAIN);
		} else {
			if (rep->persist_remote(rep, raddr, len, lane, flags))
				obj_handle_remote_persist_error(pop);
		}
		rep = rep->replica;
	}

	if (pop->has_remote_replicas)
		lane_release(pop);

	return 0;
}

/*
 * obj_rep_drain -- (internal) drain with replication
 */
static void
obj_rep_drain(void *ctx)
{
	PMEMobjpool *pop = ctx;
	LOG(15, "pop %p", pop);

	pop->drain_local();

	PMEMobjpool *rep = pop->replica;
	while (rep) {
		if (rep->rpp == NULL)
			rep->drain_local();
		rep = rep->replica;
	}
}

#if VG_MEMCHECK_ENABLED
/*
 * Arbitrary value. When there's more undefined regions than MAX_UNDEFS, it's
 * not worth reporting everything - developer should fix the code.
 */
#define MAX_UNDEFS 1000

/*
 * obj_vg_check_no_undef -- (internal) check whether there are any undefined
 *				regions
 */
static void
obj_vg_check_no_undef(struct pmemobjpool *pop)
{
	LOG(4, "pop %p", pop);

	struct {
		void *start, *end;
	} undefs[MAX_UNDEFS];
	int num_undefs = 0;

	VALGRIND_DO_DISABLE_ERROR_REPORTING;
	char *addr_start = pop->addr;
	char *addr_end = addr_start + pop->set->poolsize;

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
				" regions: [pool address: %p]\n", pop);
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
 * obj_vg_boot -- (internal) notify Valgrind about pool objects
 */
static void
obj_vg_boot(struct pmemobjpool *pop)
{
	if (!On_valgrind)
		return;

	LOG(4, "pop %p", pop);

	if (os_getenv("PMEMOBJ_VG_CHECK_UNDEF"))
		obj_vg_check_no_undef(pop);
}

#endif

/*
 * obj_runtime_init_common -- (internal) runtime initialization
 *
 * Common routine for create/open and check.
 */
static int
obj_runtime_init_common(PMEMobjpool *pop)
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

	pop->conversion_flags = 0;
	pmemops_persist(&pop->p_ops,
		&pop->conversion_flags, sizeof(pop->conversion_flags));

	return 0;
}

/*
 * obj_runtime_cleanup_common -- (internal) runtime cleanup
 *
 * Common routine for create/open and check
 */
static void
obj_runtime_cleanup_common(PMEMobjpool *pop)
{
	lane_section_cleanup(pop);
	lane_cleanup(pop);
}

/*
 * obj_descr_create -- (internal) create obj pool descriptor
 */
static int
obj_descr_create(PMEMobjpool *pop, const char *layout, size_t poolsize)
{
	LOG(3, "pop %p layout %s poolsize %zu", pop, layout, poolsize);

	ASSERTeq(poolsize % Pagesize, 0);

	/* opaque info lives at the beginning of mapped memory pool */
	void *dscp = (void *)((uintptr_t)pop + sizeof(struct pool_hdr));

	/* create the persistent part of pool's descriptor */
	memset(dscp, 0, OBJ_DSC_P_SIZE);
	if (layout)
		strncpy(pop->layout, layout, PMEMOBJ_MAX_LAYOUT - 1);
	struct pmem_ops *p_ops = &pop->p_ops;

	pop->lanes_offset = OBJ_LANES_OFFSET;
	pop->nlanes = OBJ_NLANES;

	/* zero all lanes */
	lane_init_data(pop);

	pop->heap_offset = pop->lanes_offset +
		pop->nlanes * sizeof(struct lane_layout);
	pop->heap_offset = (pop->heap_offset + Pagesize - 1) & ~(Pagesize - 1);

	size_t heap_size = pop->set->poolsize - pop->heap_offset;

	/* initialize heap prior to storing the checksum */
	errno = palloc_init((char *)pop + pop->heap_offset, heap_size,
		&pop->heap_size, p_ops);
	if (errno != 0) {
		ERR("!palloc_init");
		return -1;
	}

	util_checksum(dscp, OBJ_DSC_P_SIZE, &pop->checksum, 1, 0);

	/*
	 * store the persistent part of pool's descriptor (2kB)
	 *
	 * It's safe to use PMEMOBJ_F_RELAXED flag because the entire
	 * structure is protected by checksum.
	 */
	pmemops_xpersist(p_ops, dscp, OBJ_DSC_P_SIZE, PMEMOBJ_F_RELAXED);

	/* initialize run_id, it will be incremented later */
	pop->run_id = 0;
	pmemops_persist(p_ops, &pop->run_id, sizeof(pop->run_id));

	pop->root_offset = 0;
	pmemops_persist(p_ops, &pop->root_offset, sizeof(pop->root_offset));
	pop->root_size = 0;
	pmemops_persist(p_ops, &pop->root_size, sizeof(pop->root_size));

	pop->conversion_flags = 0;
	pmemops_persist(p_ops, &pop->conversion_flags,
		sizeof(pop->conversion_flags));

	/*
	 * It's safe to use PMEMOBJ_F_RELAXED flag because the reserved
	 * area must be entirely zeroed.
	 */
	pmemops_memset(p_ops, pop->pmem_reserved, 0,
		sizeof(pop->pmem_reserved), PMEMOBJ_F_RELAXED);

	return 0;
}

/*
 * obj_descr_check -- (internal) validate obj pool descriptor
 */
static int
obj_descr_check(PMEMobjpool *pop, const char *layout, size_t poolsize)
{
	LOG(3, "pop %p layout %s poolsize %zu", pop, layout, poolsize);

	void *dscp = (void *)((uintptr_t)pop + sizeof(struct pool_hdr));

	if (pop->rpp) {
		/* read remote descriptor */
		if (obj_read_remote(pop->rpp, pop->remote_base, dscp, dscp,
				OBJ_DSC_P_SIZE)) {
			ERR("!obj_read_remote");
			return -1;
		}
	}

	if (!util_checksum(dscp, OBJ_DSC_P_SIZE, &pop->checksum, 0, 0)) {
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

	if (pop->heap_offset % Pagesize) {
		ERR("unaligned heap: off %" PRIu64, pop->heap_offset);
		errno = EINVAL;
		return -1;
	}

	return 0;
}

/*
 * obj_msync_nofail -- (internal) pmem_msync wrapper that never fails from
 * caller's perspective
 */
static void
obj_msync_nofail(const void *addr, size_t size)
{
	if (pmem_msync(addr, size))
		FATAL("!pmem_msync");
}

/*
 * obj_replica_init_local -- (internal) initialize runtime part
 *                               of the local replicas
 */
static int
obj_replica_init_local(PMEMobjpool *rep, int is_pmem, size_t resvsize)
{
	LOG(3, "rep %p is_pmem %d resvsize %zu", rep, is_pmem, resvsize);

	/*
	 * Use some of the memory pool area for run-time info.  This
	 * run-time state is never loaded from the file, it is always
	 * created here, so no need to worry about byte-order.
	 */
	rep->is_pmem = is_pmem;

	/* init hooks */
	rep->persist_remote = NULL;

	/*
	 * All replicas, except for master, are ignored as far as valgrind is
	 * concerned. This is to save CPU time and lessen the complexity of
	 * instrumentation.
	 */
	if (!rep->is_master_replica)
		VALGRIND_ADD_TO_GLOBAL_TX_IGNORE(rep, resvsize);

	if (rep->is_pmem) {
		rep->persist_local = pmem_persist;
		rep->flush_local = pmem_flush;
		rep->drain_local = pmem_drain;
		rep->memcpy_local = pmem_memcpy;
		rep->memmove_local = pmem_memmove;
		rep->memset_local = pmem_memset;
	} else {
		rep->persist_local = obj_msync_nofail;
		rep->flush_local = obj_msync_nofail;
		rep->drain_local = obj_drain_empty;
		rep->memcpy_local = obj_nopmem_memcpy;
		rep->memmove_local = obj_nopmem_memmove;
		rep->memset_local = obj_nopmem_memset;
	}

	return 0;
}

/*
 * obj_replica_init_remote -- (internal) initialize runtime part
 *                                of a remote replica
 */
static int
obj_replica_init_remote(PMEMobjpool *rep, struct pool_set *set,
				unsigned repidx, int create)
{
	LOG(3, "rep %p set %p repidx %u", rep, set, repidx);

	struct pool_replica *repset = set->replica[repidx];

	ASSERTne(repset->remote->rpp, NULL);
	ASSERTne(repset->remote->node_addr, NULL);
	ASSERTne(repset->remote->pool_desc, NULL);

	rep->node_addr = Strdup(repset->remote->node_addr);
	if (rep->node_addr == NULL)
		return -1;
	rep->pool_desc = Strdup(repset->remote->pool_desc);
	if (rep->pool_desc == NULL) {
		Free(rep->node_addr);
		return -1;
	}

	rep->rpp = repset->remote->rpp;

	/* remote_base - beginning of the remote pool */
	rep->remote_base = (uintptr_t)rep->addr;

	/* init hooks */
	rep->persist_remote = obj_remote_persist;
	rep->persist_local = NULL;
	rep->flush_local = NULL;
	rep->drain_local = NULL;
	rep->memcpy_local = NULL;
	rep->memmove_local = NULL;
	rep->memset_local = NULL;

	rep->p_ops.remote.read = obj_read_remote;
	rep->p_ops.remote.ctx = rep->rpp;
	rep->p_ops.remote.base = rep->remote_base;

	return 0;
}

/*
 * obj_cleanup_remote -- (internal) clean up the remote pools data
 */
static void
obj_cleanup_remote(PMEMobjpool *pop)
{
	LOG(3, "pop %p", pop);

	for (; pop != NULL; pop = pop->replica) {
		if (pop->rpp != NULL) {
			Free(pop->node_addr);
			Free(pop->pool_desc);
			pop->rpp = NULL;
		}
	}
}

/*
 * obj_replica_init -- (internal) initialize runtime part of the replica
 */
static int
obj_replica_init(PMEMobjpool *rep, struct pool_set *set, unsigned repidx,
			int create)
{
	struct pool_replica *repset = set->replica[repidx];

	if (repidx == 0) {
		/* master replica */
		rep->is_master_replica = 1;
		rep->has_remote_replicas = set->remote;

		if (set->nreplicas > 1) {
			rep->p_ops.persist = obj_rep_persist;
			rep->p_ops.flush = obj_rep_flush;
			rep->p_ops.drain = obj_rep_drain;
			rep->p_ops.memcpy = obj_rep_memcpy;
			rep->p_ops.memmove = obj_rep_memmove;
			rep->p_ops.memset = obj_rep_memset;
		} else {
			rep->p_ops.persist = obj_norep_persist;
			rep->p_ops.flush = obj_norep_flush;
			rep->p_ops.drain = obj_norep_drain;
			rep->p_ops.memcpy = obj_norep_memcpy;
			rep->p_ops.memmove = obj_norep_memmove;
			rep->p_ops.memset = obj_norep_memset;
		}
		rep->p_ops.base = rep;
	} else {
		/* non-master replicas */
		rep->is_master_replica = 0;
		rep->has_remote_replicas = 0;

		rep->p_ops.persist = NULL;
		rep->p_ops.flush = NULL;
		rep->p_ops.drain = NULL;
		rep->p_ops.memcpy = NULL;
		rep->p_ops.memmove = NULL;
		rep->p_ops.memset = NULL;

		rep->p_ops.base = NULL;
	}

	rep->is_dev_dax = set->replica[repidx]->part[0].is_dev_dax;

	int ret;
	if (repset->remote)
		ret = obj_replica_init_remote(rep, set, repidx, create);
	else
		ret = obj_replica_init_local(rep, repset->is_pmem,
			set->resvsize);
	if (ret)
		return ret;

	return 0;
}

/*
 * obj_replica_fini -- (internal) deinitialize replica
 */
static void
obj_replica_fini(struct pool_replica *repset)
{
	PMEMobjpool *rep = repset->part[0].addr;

	if (repset->remote)
		obj_cleanup_remote(rep);
}

/*
 * obj_runtime_init -- (internal) initialize runtime part of the pool header
 */
static int
obj_runtime_init(PMEMobjpool *pop, int rdonly, int boot, unsigned nlanes)
{
	LOG(3, "pop %p rdonly %d boot %d", pop, rdonly, boot);
	struct pmem_ops *p_ops = &pop->p_ops;

	/* run_id is made unique by incrementing the previous value */
	pop->run_id += 2;
	if (pop->run_id == 0)
		pop->run_id += 2;
	pmemops_persist(p_ops, &pop->run_id, sizeof(pop->run_id));

	/*
	 * Use some of the memory pool area for run-time info.  This
	 * run-time state is never loaded from the file, it is always
	 * created here, so no need to worry about byte-order.
	 */
	pop->rdonly = rdonly;

	pop->uuid_lo = pmemobj_get_uuid_lo(pop);

	pop->lanes_desc.runtime_nlanes = nlanes;

	pop->tx_params = tx_params_new();
	if (pop->tx_params == NULL)
		goto err_tx_params;

	pop->stats = stats_new(pop);
	if (pop->stats == NULL)
		goto err_stat;

	VALGRIND_REMOVE_PMEM_MAPPING(&pop->mutex_head,
		sizeof(pop->mutex_head));
	VALGRIND_REMOVE_PMEM_MAPPING(&pop->rwlock_head,
		sizeof(pop->rwlock_head));
	VALGRIND_REMOVE_PMEM_MAPPING(&pop->cond_head,
		sizeof(pop->cond_head));
	pop->mutex_head = NULL;
	pop->rwlock_head = NULL;
	pop->cond_head = NULL;

	if (boot) {
		if ((errno = obj_runtime_init_common(pop)) != 0)
			goto err_boot;

#if VG_MEMCHECK_ENABLED
		if (On_valgrind) {
			/* mark unused part of the pool as not accessible */
			void *end = palloc_heap_end(&pop->heap);
			VALGRIND_DO_MAKE_MEM_NOACCESS(end,
				(char *)pop + pop->set->poolsize - (char *)end);
		}
#endif

		obj_pool_init();

		if ((errno = critnib_insert(pools_ht, pop->uuid_lo, pop))) {
			ERR("!critnib_insert to pools_ht");
			goto err_critnib_insert;
		}

		if ((errno = critnib_insert(pools_tree, (uint64_t)pop, pop))) {
			ERR("!critnib_insert to pools_tree");
			goto err_tree_insert;
		}
	}

	if (obj_ctl_init_and_load(pop) != 0) {
		errno = EINVAL;
		goto err_ctl;
	}

	util_mutex_init(&pop->ulog_user_buffers.lock);
	pop->ulog_user_buffers.map = ravl_new_sized(
		operation_user_buffer_range_cmp,
		sizeof(struct user_buffer_def));
	if (pop->ulog_user_buffers.map == NULL) {
		ERR("!ravl_new_sized");
		goto err_user_buffers_map;
	}
	pop->ulog_user_buffers.verify = 0;

	/*
	 * If possible, turn off all permissions on the pool header page.
	 *
	 * The prototype PMFS doesn't allow this when large pages are in
	 * use. It is not considered an error if this fails.
	 */
	RANGE_NONE(pop->addr, sizeof(struct pool_hdr), pop->is_dev_dax);

	return 0;

err_user_buffers_map:
	util_mutex_destroy(&pop->ulog_user_buffers.lock);
	ctl_delete(pop->ctl);
err_ctl:;
	void *n = critnib_remove(pools_tree, (uint64_t)pop);
	ASSERTne(n, NULL);
err_tree_insert:
	critnib_remove(pools_ht, pop->uuid_lo);
err_critnib_insert:
	obj_runtime_cleanup_common(pop);
err_boot:
	stats_delete(pop, pop->stats);
err_stat:
	tx_params_delete(pop->tx_params);
err_tx_params:

	return -1;
}

/*
 * obj_get_nlanes -- get a number of lanes available at runtime. If the value
 * provided with the PMEMOBJ_NLANES environment variable is greater than 0 and
 * smaller than OBJ_NLANES constant it returns PMEMOBJ_NLANES. Otherwise it
 * returns OBJ_NLANES.
 */
static unsigned
obj_get_nlanes(void)
{
	LOG(3, NULL);

	char *env_nlanes = os_getenv(OBJ_NLANES_ENV_VARIABLE);
	if (env_nlanes) {
		int nlanes = atoi(env_nlanes);
		if (nlanes <= 0) {
			ERR("%s variable must be a positive integer",
					OBJ_NLANES_ENV_VARIABLE);
			errno = EINVAL;
			goto no_valid_env;
		}

		return (unsigned)(OBJ_NLANES < nlanes ? OBJ_NLANES : nlanes);
	}

no_valid_env:
	return OBJ_NLANES;
}

/*
 * pmemobj_createU -- create a transactional memory pool (set)
 */
#ifndef _WIN32
static inline
#endif
PMEMobjpool *
pmemobj_createU(const char *path, const char *layout,
		size_t poolsize, mode_t mode)
{
	LOG(3, "path %s layout %s poolsize %zu mode %o",
			path, layout, poolsize, mode);

	PMEMobjpool *pop;
	struct pool_set *set;

	/* check length of layout */
	if (layout && (strlen(layout) >= PMEMOBJ_MAX_LAYOUT)) {
		ERR("Layout too long");
		errno = EINVAL;
		return NULL;
	}

	/*
	 * A number of lanes available at runtime equals the lowest value
	 * from all reported by remote replicas hosts. In the single host mode
	 * the runtime number of lanes is equal to the total number of lanes
	 * available in the pool or the value provided with PMEMOBJ_NLANES
	 * environment variable whichever is lower.
	 */
	unsigned runtime_nlanes = obj_get_nlanes();

	struct pool_attr adj_pool_attr = Obj_create_attr;

	/* force set SDS feature */
	if (SDS_at_create)
		adj_pool_attr.features.incompat |= POOL_FEAT_SDS;
	else
		adj_pool_attr.features.incompat &= ~POOL_FEAT_SDS;

	if (util_pool_create(&set, path, poolsize, PMEMOBJ_MIN_POOL,
			PMEMOBJ_MIN_PART, &adj_pool_attr, &runtime_nlanes,
			REPLICAS_ENABLED) != 0) {
		LOG(2, "cannot create pool or pool set");
		return NULL;
	}

	ASSERT(set->nreplicas > 0);

	/* pop is master replica from now on */
	pop = set->replica[0]->part[0].addr;

	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *repset = set->replica[r];
		PMEMobjpool *rep = repset->part[0].addr;

		size_t rt_size = (uintptr_t)(rep + 1) - (uintptr_t)&rep->addr;
		VALGRIND_REMOVE_PMEM_MAPPING(&rep->addr, rt_size);

		memset(&rep->addr, 0, rt_size);

		rep->addr = rep;
		rep->replica = NULL;
		rep->rpp = NULL;

		/* initialize replica runtime - is_pmem, funcs, ... */
		if (obj_replica_init(rep, set, r, 1 /* create */) != 0) {
			ERR("initialization of replica #%u failed", r);
			goto err;
		}

		/* link replicas */
		if (r < set->nreplicas - 1)
			rep->replica = set->replica[r + 1]->part[0].addr;
	}

	pop->set = set;

	/* create pool descriptor */
	if (obj_descr_create(pop, layout, set->poolsize) != 0) {
		LOG(2, "creation of pool descriptor failed");
		goto err;
	}

	/* initialize runtime parts - lanes, obj stores, ... */
	if (obj_runtime_init(pop, 0, 1 /* boot */,
					runtime_nlanes) != 0) {
		ERR("pool initialization failed");
		goto err;
	}

	if (util_poolset_chmod(set, mode))
		goto err;

	util_poolset_fdclose(set);

	LOG(3, "pop %p", pop);

	return pop;

err:
	LOG(4, "error clean up");
	int oerrno = errno;
	if (set->remote)
		obj_cleanup_remote(pop);
	util_poolset_close(set, DELETE_CREATED_PARTS);
	errno = oerrno;
	return NULL;
}

#ifndef _WIN32
/*
 * pmemobj_create -- create a transactional memory pool (set)
 */
PMEMobjpool *
pmemobj_create(const char *path, const char *layout,
		size_t poolsize, mode_t mode)
{
	PMEMOBJ_API_START();

	PMEMobjpool *pop = pmemobj_createU(path, layout, poolsize, mode);

	PMEMOBJ_API_END();
	return pop;
}
#else
/*
 * pmemobj_createW -- create a transactional memory pool (set)
 */
PMEMobjpool *
pmemobj_createW(const wchar_t *path, const wchar_t *layout, size_t poolsize,
	mode_t mode)
{
	char *upath = util_toUTF8(path);
	if (upath == NULL)
		return NULL;
	char *ulayout = NULL;
	if (layout != NULL) {
		ulayout = util_toUTF8(layout);
		if (ulayout == NULL) {
			util_free_UTF8(upath);
			return NULL;
		}
	}
	PMEMobjpool *ret = pmemobj_createU(upath, ulayout, poolsize, mode);

	util_free_UTF8(upath);
	util_free_UTF8(ulayout);

	return ret;
}
#endif

/*
 * obj_check_basic_local -- (internal) basic pool consistency check
 *                              of a local replica
 */
static int
obj_check_basic_local(PMEMobjpool *pop, size_t mapped_size)
{
	LOG(3, "pop %p mapped_size %zu", pop, mapped_size);

	ASSERTeq(pop->rpp, NULL);

	int consistent = 1;

	if (pop->run_id % 2) {
		ERR("invalid run_id %" PRIu64, pop->run_id);
		consistent = 0;
	}

	if ((errno = lane_check(pop)) != 0) {
		LOG(2, "!lane_check");
		consistent = 0;
	}

	/* pop->heap_size can still be 0 at this point */
	size_t heap_size = mapped_size - pop->heap_offset;
	errno = palloc_heap_check((char *)pop + pop->heap_offset,
		heap_size);
	if (errno != 0) {
		LOG(2, "!heap_check");
		consistent = 0;
	}

	return consistent;
}

/*
 * obj_read_remote -- read data from remote replica
 *
 * It reads data of size 'length' from the remote replica 'pop'
 * from address 'addr' and saves it at address 'dest'.
 */
int
obj_read_remote(void *ctx, uintptr_t base, void *dest, void *addr,
		size_t length)
{
	LOG(3, "ctx %p base 0x%lx dest %p addr %p length %zu", ctx, base, dest,
			addr, length);

	ASSERTne(ctx, NULL);
	ASSERT((uintptr_t)addr >= base);

	uintptr_t offset = (uintptr_t)addr - base;
	if (Rpmem_read(ctx, dest, offset, length, RLANE_DEFAULT)) {
		ERR("!rpmem_read");
		return -1;
	}

	return 0;
}

/*
 * obj_check_basic_remote -- (internal) basic pool consistency check
 *                               of a remote replica
 */
static int
obj_check_basic_remote(PMEMobjpool *pop, size_t mapped_size)
{
	LOG(3, "pop %p mapped_size %zu", pop, mapped_size);

	ASSERTne(pop->rpp, NULL);

	int consistent = 1;

	/* read pop->run_id */
	if (obj_read_remote(pop->rpp, pop->remote_base, &pop->run_id,
			&pop->run_id, sizeof(pop->run_id))) {
		ERR("!obj_read_remote");
		return -1;
	}

	if (pop->run_id % 2) {
		ERR("invalid run_id %" PRIu64, pop->run_id);
		consistent = 0;
	}

	/* XXX add lane_check_remote */

	/* pop->heap_size can still be 0 at this point */
	size_t heap_size = mapped_size - pop->heap_offset;
	if (palloc_heap_check_remote((char *)pop + pop->heap_offset,
			heap_size, &pop->p_ops.remote)) {
		LOG(2, "!heap_check_remote");
		consistent = 0;
	}

	return consistent;
}

/*
 * obj_check_basic -- (internal) basic pool consistency check
 *
 * Used to check if all the replicas are consistent prior to pool recovery.
 */
static int
obj_check_basic(PMEMobjpool *pop, size_t mapped_size)
{
	LOG(3, "pop %p mapped_size %zu", pop, mapped_size);

	if (pop->rpp == NULL)
		return obj_check_basic_local(pop, mapped_size);
	else
		return obj_check_basic_remote(pop, mapped_size);
}

/*
 * obj_pool_close -- (internal) close the pool set
 */
static void
obj_pool_close(struct pool_set *set)
{
	int oerrno = errno;
	util_poolset_close(set, DO_NOT_DELETE_PARTS);
	errno = oerrno;
}

/*
 * obj_pool_open -- (internal) open the given pool
 */
static int
obj_pool_open(struct pool_set **set, const char *path, unsigned flags,
	unsigned *nlanes)
{
	if (util_pool_open(set, path, PMEMOBJ_MIN_PART, &Obj_open_attr,
				nlanes, NULL, flags) != 0) {
		LOG(2, "cannot open pool or pool set");
		return -1;
	}

	ASSERT((*set)->nreplicas > 0);

	/* read-only mode is not supported in libpmemobj */
	if ((*set)->rdonly) {
		ERR("read-only mode is not supported");
		errno = EINVAL;
		goto err_rdonly;
	}

	return 0;
err_rdonly:
	obj_pool_close(*set);
	return -1;
}

/*
 * obj_replicas_init -- (internal) initialize all replicas
 */
static int
obj_replicas_init(struct pool_set *set)
{
	unsigned r;
	for (r = 0; r < set->nreplicas; r++) {
		struct pool_replica *repset = set->replica[r];
		PMEMobjpool *rep = repset->part[0].addr;

		size_t rt_size = (uintptr_t)(rep + 1) - (uintptr_t)&rep->addr;

		VALGRIND_REMOVE_PMEM_MAPPING(&rep->addr, rt_size);

		memset(&rep->addr, 0, rt_size);

		rep->addr = rep;
		rep->replica = NULL;
		rep->rpp = NULL;

		/* initialize replica runtime - is_pmem, funcs, ... */
		if (obj_replica_init(rep, set, r, 0 /* open */) != 0) {
			ERR("initialization of replica #%u failed", r);
			goto err;
		}

		/* link replicas */
		if (r < set->nreplicas - 1)
			rep->replica = set->replica[r + 1]->part[0].addr;
	}

	return 0;
err:
	for (unsigned p = 0; p < r; p++)
		obj_replica_fini(set->replica[p]);

	return -1;
}

/*
 * obj_replicas_fini -- (internal) deinitialize all replicas
 */
static void
obj_replicas_fini(struct pool_set *set)
{
	int oerrno = errno;
	for (unsigned r = 0; r < set->nreplicas; r++)
		obj_replica_fini(set->replica[r]);
	errno = oerrno;
}

/*
 * obj_replicas_check_basic -- (internal) perform basic consistency check
 * for all replicas
 */
static int
obj_replicas_check_basic(PMEMobjpool *pop)
{
	PMEMobjpool *rep;
	for (unsigned r = 0; r < pop->set->nreplicas; r++) {
		rep = pop->set->replica[r]->part[0].addr;
		if (obj_check_basic(rep, pop->set->poolsize) == 0) {
			ERR("inconsistent replica #%u", r);
			return -1;
		}
	}

	/* copy lanes */
	void *src = (void *)((uintptr_t)pop + pop->lanes_offset);
	size_t len = pop->nlanes * sizeof(struct lane_layout);

	for (unsigned r = 1; r < pop->set->nreplicas; r++) {
		rep = pop->set->replica[r]->part[0].addr;
		void *dst = (void *)((uintptr_t)rep + pop->lanes_offset);
		if (rep->rpp == NULL) {
			rep->memcpy_local(dst, src, len, 0);
		} else {
			if (rep->persist_remote(rep, dst, len,
					RLANE_DEFAULT, 0))
				obj_handle_remote_persist_error(pop);
		}
	}

	return 0;
}

/*
 * obj_open_common -- open a transactional memory pool (set)
 *
 * This routine takes flags and does all the work
 * (flag POOL_OPEN_COW - internal calls can map a read-only pool if required).
 */
static PMEMobjpool *
obj_open_common(const char *path, const char *layout, unsigned flags, int boot)
{
	LOG(3, "path %s layout %s flags 0x%x", path, layout, flags);

	PMEMobjpool *pop = NULL;
	struct pool_set *set;

	/*
	 * A number of lanes available at runtime equals the lowest value
	 * from all reported by remote replicas hosts. In the single host mode
	 * the runtime number of lanes is equal to the total number of lanes
	 * available in the pool or the value provided with PMEMOBJ_NLANES
	 * environment variable whichever is lower.
	 */
	unsigned runtime_nlanes = obj_get_nlanes();
	if (obj_pool_open(&set, path, flags, &runtime_nlanes))
		return NULL;

	/* pop is master replica from now on */
	pop = set->replica[0]->part[0].addr;

	if (obj_replicas_init(set))
		goto replicas_init;

	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *repset = set->replica[r];
		PMEMobjpool *rep = repset->part[0].addr;
		/* check descriptor */
		if (obj_descr_check(rep, layout, set->poolsize) != 0) {
			LOG(2, "descriptor check of replica #%u failed", r);
			goto err_descr_check;
		}
	}

	pop->set = set;

	if (boot) {
		/* check consistency of 'master' replica */
		if (obj_check_basic(pop, pop->set->poolsize) == 0) {
			goto err_check_basic;
		}
	}

	if (set->nreplicas > 1) {
		if (obj_replicas_check_basic(pop))
			goto err_replicas_check_basic;
	}

	/*
	 * before runtime initialization lanes are unavailable, remote persists
	 * should use RLANE_DEFAULT
	 */
	pop->lanes_desc.runtime_nlanes = 0;

#if VG_MEMCHECK_ENABLED
	pop->vg_boot = boot;
#endif
	/* initialize runtime parts - lanes, obj stores, ... */
	if (obj_runtime_init(pop, 0, boot, runtime_nlanes) != 0) {
		ERR("pool initialization failed");
		goto err_runtime_init;
	}

#if VG_MEMCHECK_ENABLED
	if (boot)
		obj_vg_boot(pop);
#endif

	util_poolset_fdclose(set);

	LOG(3, "pop %p", pop);

	return pop;

err_runtime_init:
err_replicas_check_basic:
err_check_basic:
err_descr_check:
	obj_replicas_fini(set);
replicas_init:
	obj_pool_close(set);
	return NULL;
}

/*
 * pmemobj_openU -- open a transactional memory pool
 */
#ifndef _WIN32
static inline
#endif
PMEMobjpool *
pmemobj_openU(const char *path, const char *layout)
{
	LOG(3, "path %s layout %s", path, layout);

	return obj_open_common(path, layout,
			COW_at_open ? POOL_OPEN_COW : 0, 1);
}

#ifndef _WIN32
/*
 * pmemobj_open -- open a transactional memory pool
 */
PMEMobjpool *
pmemobj_open(const char *path, const char *layout)
{
	PMEMOBJ_API_START();

	PMEMobjpool *pop = pmemobj_openU(path, layout);

	PMEMOBJ_API_END();
	return pop;
}
#else
/*
 * pmemobj_openW -- open a transactional memory pool
 */
PMEMobjpool *
pmemobj_openW(const wchar_t *path, const wchar_t *layout)
{
	char *upath = util_toUTF8(path);
	if (upath == NULL)
		return NULL;

	char *ulayout = NULL;
	if (layout != NULL) {
		ulayout = util_toUTF8(layout);
		if (ulayout == NULL) {
			util_free_UTF8(upath);
			return NULL;
		}
	}

	PMEMobjpool *ret = pmemobj_openU(upath, ulayout);
	util_free_UTF8(upath);
	util_free_UTF8(ulayout);
	return ret;
}
#endif

/*
 * obj_replicas_cleanup -- (internal) free resources allocated for replicas
 */
static void
obj_replicas_cleanup(struct pool_set *set)
{
	LOG(3, "set %p", set);

	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *rep = set->replica[r];

		PMEMobjpool *pop = rep->part[0].addr;

		if (pop->rpp != NULL) {
			/*
			 * remote replica will be closed in util_poolset_close
			 */
			pop->rpp = NULL;

			Free(pop->node_addr);
			Free(pop->pool_desc);
		}
	}
}

/*
 * obj_pool_lock_cleanup -- (internal) Destroy any locks or condition
 *	variables that were allocated at run time
 */
static void
obj_pool_lock_cleanup(PMEMobjpool *pop)
{
	LOG(3, "pop %p", pop);

	PMEMmutex_internal *nextm;
	for (PMEMmutex_internal *m = pop->mutex_head; m != NULL; m = nextm) {
		nextm = m->PMEMmutex_next;
		LOG(4, "mutex %p *mutex %p", &m->PMEMmutex_lock,
			m->PMEMmutex_bsd_mutex_p);
		os_mutex_destroy(&m->PMEMmutex_lock);
		m->PMEMmutex_next = NULL;
		m->PMEMmutex_bsd_mutex_p = NULL;
	}
	pop->mutex_head = NULL;

	PMEMrwlock_internal *nextr;
	for (PMEMrwlock_internal *r = pop->rwlock_head; r != NULL; r = nextr) {
		nextr = r->PMEMrwlock_next;
		LOG(4, "rwlock %p *rwlock %p", &r->PMEMrwlock_lock,
			r->PMEMrwlock_bsd_rwlock_p);
		os_rwlock_destroy(&r->PMEMrwlock_lock);
		r->PMEMrwlock_next = NULL;
		r->PMEMrwlock_bsd_rwlock_p = NULL;
	}
	pop->rwlock_head = NULL;

	PMEMcond_internal *nextc;
	for (PMEMcond_internal *c = pop->cond_head; c != NULL; c = nextc) {
		nextc = c->PMEMcond_next;
		LOG(4, "cond %p *cond %p", &c->PMEMcond_cond,
			c->PMEMcond_bsd_cond_p);
		os_cond_destroy(&c->PMEMcond_cond);
		c->PMEMcond_next = NULL;
		c->PMEMcond_bsd_cond_p = NULL;
	}
	pop->cond_head = NULL;
}
/*
 * obj_pool_cleanup -- (internal) cleanup the pool and unmap
 */
static void
obj_pool_cleanup(PMEMobjpool *pop)
{
	LOG(3, "pop %p", pop);

	ravl_delete(pop->ulog_user_buffers.map);
	util_mutex_destroy(&pop->ulog_user_buffers.lock);

	stats_delete(pop, pop->stats);
	tx_params_delete(pop->tx_params);
	ctl_delete(pop->ctl);

	obj_pool_lock_cleanup(pop);

	lane_section_cleanup(pop);
	lane_cleanup(pop);

	/* unmap all the replicas */
	obj_replicas_cleanup(pop->set);
	util_poolset_close(pop->set, DO_NOT_DELETE_PARTS);
}

/*
 * pmemobj_close -- close a transactional memory pool
 */
void
pmemobj_close(PMEMobjpool *pop)
{
	LOG(3, "pop %p", pop);
	PMEMOBJ_API_START();

	_pobj_cache_invalidate++;

	if (critnib_remove(pools_ht, pop->uuid_lo) != pop) {
		ERR("critnib_remove for pools_ht");
	}

	if (critnib_remove(pools_tree, (uint64_t)pop) != pop)
		ERR("critnib_remove for pools_tree");

#ifndef _WIN32

	if (_pobj_cached_pool.pop == pop) {
		_pobj_cached_pool.pop = NULL;
		_pobj_cached_pool.uuid_lo = 0;
	}

#else /* _WIN32 */

	struct _pobj_pcache *pcache = os_tls_get(Cached_pool_key);
	if (pcache != NULL) {
		if (pcache->pop == pop) {
			pcache->pop = NULL;
			pcache->uuid_lo = 0;
		}
	}

#endif /* _WIN32 */

	obj_pool_cleanup(pop);
	PMEMOBJ_API_END();
}

/*
 * pmemobj_checkU -- transactional memory pool consistency check
 */
#ifndef _WIN32
static inline
#endif
int
pmemobj_checkU(const char *path, const char *layout)
{
	LOG(3, "path %s layout %s", path, layout);

	PMEMobjpool *pop = obj_open_common(path, layout, POOL_OPEN_COW, 0);
	if (pop == NULL)
		return -1;	/* errno set by obj_open_common() */

	int consistent = 1;

	/*
	 * For replicated pools, basic consistency check is performed
	 * in obj_open_common().
	 */
	if (pop->replica == NULL)
		consistent = obj_check_basic(pop, pop->set->poolsize);

	if (consistent && (errno = obj_runtime_init_common(pop)) != 0) {
		LOG(3, "!obj_boot");
		consistent = 0;
	}

	if (consistent) {
		obj_pool_cleanup(pop);
	} else {
		stats_delete(pop, pop->stats);
		tx_params_delete(pop->tx_params);
		ctl_delete(pop->ctl);

		/* unmap all the replicas */
		obj_replicas_cleanup(pop->set);
		util_poolset_close(pop->set, DO_NOT_DELETE_PARTS);
	}

	if (consistent)
		LOG(4, "pool consistency check OK");

	return consistent;
}

#ifndef _WIN32
/*
 * pmemobj_check -- transactional memory pool consistency check
 */
int
pmemobj_check(const char *path, const char *layout)
{
	PMEMOBJ_API_START();

	int ret = pmemobj_checkU(path, layout);

	PMEMOBJ_API_END();
	return ret;
}
#else
/*
 * pmemobj_checkW -- transactional memory pool consistency check
 */
int
pmemobj_checkW(const wchar_t *path, const wchar_t *layout)
{
	char *upath = util_toUTF8(path);
	if (upath == NULL)
		return -1;

	char *ulayout = NULL;
	if (layout != NULL) {
		ulayout = util_toUTF8(layout);
		if (ulayout == NULL) {
			util_free_UTF8(upath);
			return -1;
		}
	}

	int ret = pmemobj_checkU(upath, ulayout);

	util_free_UTF8(upath);
	util_free_UTF8(ulayout);

	return ret;
}
#endif

/*
 * pmemobj_pool_by_oid -- returns the pool handle associated with the oid
 */
PMEMobjpool *
pmemobj_pool_by_oid(PMEMoid oid)
{
	LOG(3, "oid.off 0x%016" PRIx64, oid.off);

	/* XXX this is a temporary fix, to be fixed properly later */
	if (pools_ht == NULL)
		return NULL;

	return critnib_get(pools_ht, oid.pool_uuid_lo);
}

/*
 * pmemobj_pool_by_ptr -- returns the pool handle associated with the address
 */
PMEMobjpool *
pmemobj_pool_by_ptr(const void *addr)
{
	LOG(3, "addr %p", addr);

	/* fast path for transactions */
	PMEMobjpool *pop = tx_get_pop();

	if ((pop != NULL) && OBJ_PTR_FROM_POOL(pop, addr))
		return pop;

	/* XXX this is a temporary fix, to be fixed properly later */
	if (pools_tree == NULL)
		return NULL;

	pop = critnib_find_le(pools_tree, (uint64_t)addr);
	if (pop == NULL)
		return NULL;

	size_t pool_size = pop->heap_offset + pop->heap_size;
	if ((char *)addr >= (char *)pop + pool_size)
		return NULL;

	return pop;
}

/* arguments for constructor_alloc */
struct constr_args {
	int zero_init;
	pmemobj_constr constructor;
	void *arg;
};

/*
 * constructor_alloc -- (internal) constructor for obj_alloc_construct
 */
static int
constructor_alloc(void *ctx, void *ptr, size_t usable_size, void *arg)
{
	PMEMobjpool *pop = ctx;
	LOG(3, "pop %p ptr %p arg %p", pop, ptr, arg);
	struct pmem_ops *p_ops = &pop->p_ops;

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct constr_args *carg = arg;

	if (carg->zero_init)
		pmemops_memset(p_ops, ptr, 0, usable_size, 0);

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
	type_num_t type_num, uint64_t flags,
	pmemobj_constr constructor, void *arg)
{
	if (size > PMEMOBJ_MAX_ALLOC_SIZE) {
		ERR("requested size too large");
		errno = ENOMEM;
		return -1;
	}

	struct constr_args carg;

	carg.zero_init = flags & POBJ_FLAG_ZERO;
	carg.constructor = constructor;
	carg.arg = arg;

	struct operation_context *ctx = pmalloc_operation_hold(pop);

	if (oidp)
		operation_add_entry(ctx, &oidp->pool_uuid_lo, pop->uuid_lo,
				ULOG_OPERATION_SET);

	int ret = palloc_operation(&pop->heap, 0,
			oidp != NULL ? &oidp->off : NULL, size,
			constructor_alloc, &carg, type_num, 0,
			CLASS_ID_FROM_FLAG(flags), ARENA_ID_FROM_FLAG(flags),
			ctx);

	pmalloc_operation_release(pop);

	return ret;
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

	PMEMOBJ_API_START();
	int ret = obj_alloc_construct(pop, oidp, size, type_num,
			0, constructor, arg);

	PMEMOBJ_API_END();
	return ret;
}

/*
 * pmemobj_xalloc -- allocates with flags
 */
int
pmemobj_xalloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
	uint64_t type_num, uint64_t flags,
	pmemobj_constr constructor, void *arg)
{
	LOG(3, "pop %p oidp %p size %zu type_num %llx flags %llx "
		"constructor %p arg %p",
		pop, oidp, size, (unsigned long long)type_num,
		(unsigned long long)flags,
		constructor, arg);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();

	if (size == 0) {
		ERR("allocation with size 0");
		errno = EINVAL;
		return -1;
	}

	if (flags & ~POBJ_TX_XALLOC_VALID_FLAGS) {
		ERR("unknown flags 0x%" PRIx64,
				flags & ~POBJ_TX_XALLOC_VALID_FLAGS);
		errno = EINVAL;
		return -1;
	}

	PMEMOBJ_API_START();
	int ret = obj_alloc_construct(pop, oidp, size, type_num,
			flags, constructor, arg);

	PMEMOBJ_API_END();
	return ret;
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

	PMEMOBJ_API_START();
	int ret = obj_alloc_construct(pop, oidp, size, type_num, POBJ_FLAG_ZERO,
		NULL, NULL);

	PMEMOBJ_API_END();
	return ret;
}

/*
 * obj_free -- (internal) free an object
 */
static void
obj_free(PMEMobjpool *pop, PMEMoid *oidp)
{
	ASSERTne(oidp, NULL);

	struct operation_context *ctx = pmalloc_operation_hold(pop);

	operation_add_entry(ctx, &oidp->pool_uuid_lo, 0, ULOG_OPERATION_SET);

	palloc_operation(&pop->heap, oidp->off, &oidp->off, 0, NULL, NULL,
			0, 0, 0, 0, ctx);

	pmalloc_operation_release(pop);
}

/*
 * constructor_realloc -- (internal) constructor for pmemobj_realloc
 */
static int
constructor_realloc(void *ctx, void *ptr, size_t usable_size, void *arg)
{
	PMEMobjpool *pop = ctx;
	LOG(3, "pop %p ptr %p arg %p", pop, ptr, arg);
	struct pmem_ops *p_ops = &pop->p_ops;

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct carg_realloc *carg = arg;

	if (!carg->zero_init)
		return 0;

	if (usable_size > carg->old_size) {
		size_t grow_len = usable_size - carg->old_size;
		void *new_data_ptr = (void *)((uintptr_t)ptr + carg->old_size);

		pmemops_memset(p_ops, new_data_ptr, 0, grow_len, 0);
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
				POBJ_FLAG_ZERO, NULL, NULL);
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

	struct carg_realloc carg;
	carg.ptr = OBJ_OFF_TO_PTR(pop, oidp->off);
	carg.new_size = size;
	carg.old_size = pmemobj_alloc_usable_size(*oidp);
	carg.user_type = type_num;
	carg.constructor = NULL;
	carg.arg = NULL;
	carg.zero_init = zero_init;

	struct operation_context *ctx = pmalloc_operation_hold(pop);

	int ret = palloc_operation(&pop->heap, oidp->off, &oidp->off,
			size, constructor_realloc, &carg, type_num,
			0, 0, 0, ctx);

	pmalloc_operation_release(pop);

	return ret;
}

/*
 * constructor_zrealloc_root -- (internal) constructor for pmemobj_root
 */
static int
constructor_zrealloc_root(void *ctx, void *ptr, size_t usable_size, void *arg)
{
	PMEMobjpool *pop = ctx;
	LOG(3, "pop %p ptr %p arg %p", pop, ptr, arg);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	VALGRIND_ADD_TO_TX(ptr, usable_size);

	struct carg_realloc *carg = arg;

	constructor_realloc(pop, ptr, usable_size, arg);
	int ret = 0;
	if (carg->constructor)
		ret = carg->constructor(pop, ptr, carg->arg);

	VALGRIND_REMOVE_FROM_TX(ptr, usable_size);

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

	LOG(3, "pop %p oid.off 0x%016" PRIx64 " size %zu type_num %" PRIu64,
		pop, oidp->off, size, type_num);

	PMEMOBJ_API_START();
	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();
	ASSERT(OBJ_OID_IS_VALID(pop, *oidp));

	int ret = obj_realloc_common(pop, oidp, size, (type_num_t)type_num, 0);

	PMEMOBJ_API_END();
	return ret;
}

/*
 * pmemobj_zrealloc -- resizes an existing object, any new space is zeroed.
 */
int
pmemobj_zrealloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size,
		uint64_t type_num)
{
	ASSERTne(oidp, NULL);

	LOG(3, "pop %p oid.off 0x%016" PRIx64 " size %zu type_num %" PRIu64,
		pop, oidp->off, size, type_num);

	PMEMOBJ_API_START();

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();
	ASSERT(OBJ_OID_IS_VALID(pop, *oidp));

	int ret = obj_realloc_common(pop, oidp, size, (type_num_t)type_num, 1);

	PMEMOBJ_API_END();
	return ret;
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
	pmemops_memcpy(&pop->p_ops, ptr, carg->s, carg->size, 0);

	return 0;
}

/*
 * pmemobj_strdup -- allocates a new object with duplicate of the string s.
 */
int
pmemobj_strdup(PMEMobjpool *pop, PMEMoid *oidp, const char *s,
		uint64_t type_num)
{
	LOG(3, "pop %p oidp %p string %s type_num %" PRIu64,
	    pop, oidp, s, type_num);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();

	if (NULL == s) {
		errno = EINVAL;
		return -1;
	}

	PMEMOBJ_API_START();
	struct carg_strdup carg;
	carg.size = (strlen(s) + 1) * sizeof(char);
	carg.s = s;

	int ret = obj_alloc_construct(pop, oidp, carg.size,
		(type_num_t)type_num, 0, constructor_strdup, &carg);

	PMEMOBJ_API_END();
	return ret;
}

/* arguments for constructor_wcsdup */
struct carg_wcsdup {
	size_t size;
	const wchar_t *s;
};

/*
 * constructor_wcsdup -- (internal) constructor of pmemobj_wcsdup
 */
static int
constructor_wcsdup(PMEMobjpool *pop, void *ptr, void *arg)
{
	LOG(3, "pop %p ptr %p arg %p", pop, ptr, arg);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct carg_wcsdup *carg = arg;

	/* copy string */
	pmemops_memcpy(&pop->p_ops, ptr, carg->s, carg->size, 0);

	return 0;
}

/*
 * pmemobj_wcsdup -- allocates a new object with duplicate of the wide character
 * string s.
 */
int
pmemobj_wcsdup(PMEMobjpool *pop, PMEMoid *oidp, const wchar_t *s,
	uint64_t type_num)
{
	LOG(3, "pop %p oidp %p string %S type_num %" PRIu64,
		    pop, oidp, s, type_num);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();

	if (NULL == s) {
		errno = EINVAL;
		return -1;
	}

	PMEMOBJ_API_START();
	struct carg_wcsdup carg;
	carg.size = (wcslen(s) + 1) * sizeof(wchar_t);
	carg.s = s;

	int ret = obj_alloc_construct(pop, oidp, carg.size,
		(type_num_t)type_num, 0, constructor_wcsdup, &carg);

	PMEMOBJ_API_END();
	return ret;
}

/*
 * pmemobj_free -- frees an existing object
 */
void
pmemobj_free(PMEMoid *oidp)
{
	ASSERTne(oidp, NULL);

	LOG(3, "oid.off 0x%016" PRIx64, oidp->off);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();

	if (oidp->off == 0)
		return;

	PMEMOBJ_API_START();
	PMEMobjpool *pop = pmemobj_pool_by_oid(*oidp);

	ASSERTne(pop, NULL);
	ASSERT(OBJ_OID_IS_VALID(pop, *oidp));

	obj_free(pop, oidp);
	PMEMOBJ_API_END();
}

/*
 * pmemobj_alloc_usable_size -- returns usable size of object
 */
size_t
pmemobj_alloc_usable_size(PMEMoid oid)
{
	LOG(3, "oid.off 0x%016" PRIx64, oid.off);

	if (oid.off == 0)
		return 0;

	PMEMobjpool *pop = pmemobj_pool_by_oid(oid);

	ASSERTne(pop, NULL);
	ASSERT(OBJ_OID_IS_VALID(pop, oid));

	return (palloc_usable_size(&pop->heap, oid.off));
}

/*
 * pmemobj_memcpy_persist -- pmemobj version of memcpy
 */
void *
pmemobj_memcpy_persist(PMEMobjpool *pop, void *dest, const void *src,
	size_t len)
{
	LOG(15, "pop %p dest %p src %p len %zu", pop, dest, src, len);
	PMEMOBJ_API_START();

	void *ptr = pmemops_memcpy(&pop->p_ops, dest, src, len, 0);

	PMEMOBJ_API_END();
	return ptr;
}

/*
 * pmemobj_memset_persist -- pmemobj version of memset
 */
void *
pmemobj_memset_persist(PMEMobjpool *pop, void *dest, int c, size_t len)
{
	LOG(15, "pop %p dest %p c 0x%02x len %zu", pop, dest, c, len);
	PMEMOBJ_API_START();

	void *ptr = pmemops_memset(&pop->p_ops, dest, c, len, 0);

	PMEMOBJ_API_END();
	return ptr;
}

/*
 * pmemobj_memcpy -- pmemobj version of memcpy
 */
void *
pmemobj_memcpy(PMEMobjpool *pop, void *dest, const void *src, size_t len,
		unsigned flags)
{
	LOG(15, "pop %p dest %p src %p len %zu flags 0x%x", pop, dest, src, len,
			flags);

	PMEMOBJ_API_START();

	void *ptr = pmemops_memcpy(&pop->p_ops, dest, src, len, flags);

	PMEMOBJ_API_END();
	return ptr;
}

/*
 * pmemobj_memmove -- pmemobj version of memmove
 */
void *
pmemobj_memmove(PMEMobjpool *pop, void *dest, const void *src, size_t len,
		unsigned flags)
{
	LOG(15, "pop %p dest %p src %p len %zu flags 0x%x", pop, dest, src, len,
			flags);

	PMEMOBJ_API_START();

	void *ptr = pmemops_memmove(&pop->p_ops, dest, src, len, flags);

	PMEMOBJ_API_END();
	return ptr;
}

/*
 * pmemobj_memset -- pmemobj version of memset
 */
void *
pmemobj_memset(PMEMobjpool *pop, void *dest, int c, size_t len, unsigned flags)
{
	LOG(15, "pop %p dest %p c 0x%02x len %zu flags 0x%x", pop, dest, c, len,
			flags);

	PMEMOBJ_API_START();

	void *ptr = pmemops_memset(&pop->p_ops, dest, c, len, flags);

	PMEMOBJ_API_END();
	return ptr;
}

/*
 * pmemobj_persist -- pmemobj version of pmem_persist
 */
void
pmemobj_persist(PMEMobjpool *pop, const void *addr, size_t len)
{
	LOG(15, "pop %p addr %p len %zu", pop, addr, len);

	pmemops_persist(&pop->p_ops, addr, len);
}

/*
 * pmemobj_flush -- pmemobj version of pmem_flush
 */
void
pmemobj_flush(PMEMobjpool *pop, const void *addr, size_t len)
{
	LOG(15, "pop %p addr %p len %zu", pop, addr, len);

	pmemops_flush(&pop->p_ops, addr, len);
}

/*
 * pmemobj_xpersist -- pmemobj version of pmem_persist with additional flags
 * argument
 */
int
pmemobj_xpersist(PMEMobjpool *pop, const void *addr, size_t len, unsigned flags)
{
	LOG(15, "pop %p addr %p len %zu", pop, addr, len);

	if (flags & ~OBJ_X_VALID_FLAGS) {
		errno = EINVAL;
		ERR("invalid flags 0x%x", flags);
		return -1;
	}

	return pmemops_xpersist(&pop->p_ops, addr, len, flags);
}

/*
 * pmemobj_xflush -- pmemobj version of pmem_flush with additional flags
 * argument
 */
int
pmemobj_xflush(PMEMobjpool *pop, const void *addr, size_t len, unsigned flags)
{
	LOG(15, "pop %p addr %p len %zu", pop, addr, len);

	if (flags & ~OBJ_X_VALID_FLAGS) {
		errno = EINVAL;
		ERR("invalid flags 0x%x", flags);
		return -1;
	}

	return pmemops_xflush(&pop->p_ops, addr, len, flags);
}

/*
 * pmemobj_drain -- pmemobj version of pmem_drain
 */
void
pmemobj_drain(PMEMobjpool *pop)
{
	LOG(15, "pop %p", pop);

	pmemops_drain(&pop->p_ops);
}

/*
 * pmemobj_type_num -- returns type number of object
 */
uint64_t
pmemobj_type_num(PMEMoid oid)
{
	LOG(3, "oid.off 0x%016" PRIx64, oid.off);

	ASSERT(!OID_IS_NULL(oid));

	PMEMobjpool *pop = pmemobj_pool_by_oid(oid);

	ASSERTne(pop, NULL);
	ASSERT(OBJ_OID_IS_VALID(pop, oid));

	return palloc_extra(&pop->heap, oid.off);
}

/* arguments for constructor_alloc_root */
struct carg_root {
	size_t size;
	pmemobj_constr constructor;
	void *arg;
};

/*
 * obj_realloc_root -- (internal) reallocate root object
 */
static int
obj_alloc_root(PMEMobjpool *pop, size_t size,
	pmemobj_constr constructor, void *arg)
{
	LOG(3, "pop %p size %zu", pop, size);

	struct carg_realloc carg;

	carg.ptr = OBJ_OFF_TO_PTR(pop, pop->root_offset);
	carg.old_size = pop->root_size;
	carg.new_size = size;
	carg.user_type = POBJ_ROOT_TYPE_NUM;
	carg.constructor = constructor;
	carg.zero_init = 1;
	carg.arg = arg;

	struct operation_context *ctx = pmalloc_operation_hold(pop);

	operation_add_entry(ctx, &pop->root_size, size, ULOG_OPERATION_SET);

	int ret = palloc_operation(&pop->heap, pop->root_offset,
			&pop->root_offset, size,
			constructor_zrealloc_root, &carg,
			POBJ_ROOT_TYPE_NUM, OBJ_INTERNAL_OBJECT_MASK,
			0, 0, ctx);

	pmalloc_operation_release(pop);

	return ret;
}

/*
 * pmemobj_root_size -- returns size of the root object
 */
size_t
pmemobj_root_size(PMEMobjpool *pop)
{
	LOG(3, "pop %p", pop);

	if (pop->root_offset && pop->root_size) {
		return pop->root_size;
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

	if (size == 0 && pop->root_offset == 0) {
		ERR("requested size cannot equals zero");
		errno = EINVAL;
		return OID_NULL;
	}

	PMEMOBJ_API_START();

	PMEMoid root;

	pmemobj_mutex_lock_nofail(pop, &pop->rootlock);

	if (size > pop->root_size &&
		obj_alloc_root(pop, size, constructor, arg)) {
		pmemobj_mutex_unlock_nofail(pop, &pop->rootlock);
		LOG(2, "obj_realloc_root failed");
		PMEMOBJ_API_END();
		return OID_NULL;
	}

	root.pool_uuid_lo = pop->uuid_lo;
	root.off = pop->root_offset;

	pmemobj_mutex_unlock_nofail(pop, &pop->rootlock);

	PMEMOBJ_API_END();
	return root;
}

/*
 * pmemobj_root -- returns root object
 */
PMEMoid
pmemobj_root(PMEMobjpool *pop, size_t size)
{
	LOG(3, "pop %p size %zu", pop, size);

	PMEMOBJ_API_START();
	PMEMoid oid = pmemobj_root_construct(pop, size, NULL, NULL);
	PMEMOBJ_API_END();
	return oid;
}

/*
 * pmemobj_first - returns first object of specified type
 */
PMEMoid
pmemobj_first(PMEMobjpool *pop)
{
	LOG(3, "pop %p", pop);

	PMEMoid ret = {0, 0};

	uint64_t off = palloc_first(&pop->heap);
	if (off != 0) {
		ret.off = off;
		ret.pool_uuid_lo = pop->uuid_lo;

		if (palloc_flags(&pop->heap, off) & OBJ_INTERNAL_OBJECT_MASK) {
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
	LOG(3, "oid.off 0x%016" PRIx64, oid.off);

	PMEMoid curr = oid;
	if (curr.off == 0)
		return OID_NULL;

	PMEMobjpool *pop = pmemobj_pool_by_oid(curr);
	ASSERTne(pop, NULL);

	do {
		ASSERT(OBJ_OID_IS_VALID(pop, curr));
		uint64_t next_off = palloc_next(&pop->heap, curr.off);

		if (next_off == 0)
			return OID_NULL;

		/* next object exists */
		curr.off = next_off;

	} while (palloc_flags(&pop->heap, curr.off) & OBJ_INTERNAL_OBJECT_MASK);

	return curr;
}

/*
 * pmemobj_reserve -- reserves a single object
 */
PMEMoid
pmemobj_reserve(PMEMobjpool *pop, struct pobj_action *act,
	size_t size, uint64_t type_num)
{
	LOG(3, "pop %p act %p size %zu type_num %llx",
		pop, act, size,
		(unsigned long long)type_num);

	PMEMOBJ_API_START();
	PMEMoid oid = OID_NULL;

	if (palloc_reserve(&pop->heap, size, NULL, NULL, type_num,
		0, 0, 0, act) != 0) {
		PMEMOBJ_API_END();
		return oid;
	}

	oid.off = act->heap.offset;
	oid.pool_uuid_lo = pop->uuid_lo;

	PMEMOBJ_API_END();
	return oid;
}

/*
 * pmemobj_xreserve -- reserves a single object
 */
PMEMoid
pmemobj_xreserve(PMEMobjpool *pop, struct pobj_action *act,
	size_t size, uint64_t type_num, uint64_t flags)
{
	LOG(3, "pop %p act %p size %zu type_num %llx flags %llx",
		pop, act, size,
		(unsigned long long)type_num, (unsigned long long)flags);

	PMEMoid oid = OID_NULL;

	if (flags & ~POBJ_ACTION_XRESERVE_VALID_FLAGS) {
		ERR("unknown flags 0x%" PRIx64,
				flags & ~POBJ_ACTION_XRESERVE_VALID_FLAGS);
		errno = EINVAL;
		return oid;
	}

	PMEMOBJ_API_START();
	struct constr_args carg;

	carg.zero_init = flags & POBJ_FLAG_ZERO;
	carg.constructor = NULL;
	carg.arg = NULL;

	if (palloc_reserve(&pop->heap, size, constructor_alloc, &carg,
		type_num, 0, CLASS_ID_FROM_FLAG(flags),
		ARENA_ID_FROM_FLAG(flags), act) != 0) {
		PMEMOBJ_API_END();
		return oid;
	}

	oid.off = act->heap.offset;
	oid.pool_uuid_lo = pop->uuid_lo;

	PMEMOBJ_API_END();
	return oid;
}

/*
 * pmemobj_set_value -- creates an action to set a value
 */
void
pmemobj_set_value(PMEMobjpool *pop, struct pobj_action *act,
	uint64_t *ptr, uint64_t value)
{
	palloc_set_value(&pop->heap, act, ptr, value);
}

/*
 * pmemobj_defer_free -- creates a deferred free action
 */
void
pmemobj_defer_free(PMEMobjpool *pop, PMEMoid oid, struct pobj_action *act)
{
	ASSERT(!OID_IS_NULL(oid));
	palloc_defer_free(&pop->heap, oid.off, act);
}

/*
 * pmemobj_publish -- publishes a collection of actions
 */
int
pmemobj_publish(PMEMobjpool *pop, struct pobj_action *actv, size_t actvcnt)
{
	PMEMOBJ_API_START();
	struct operation_context *ctx = pmalloc_operation_hold(pop);

	size_t entries_size = actvcnt * sizeof(struct ulog_entry_val);

	if (operation_reserve(ctx, entries_size) != 0) {
		PMEMOBJ_API_END();
		return -1;
	}

	palloc_publish(&pop->heap, actv, actvcnt, ctx);

	pmalloc_operation_release(pop);

	PMEMOBJ_API_END();
	return 0;
}

/*
 * pmemobj_cancel -- cancels collection of actions
 */
void
pmemobj_cancel(PMEMobjpool *pop, struct pobj_action *actv, size_t actvcnt)
{
	PMEMOBJ_API_START();
	palloc_cancel(&pop->heap, actv, actvcnt);
	PMEMOBJ_API_END();
}

/*
 * pmemobj_list_insert -- adds object to a list
 */
int
pmemobj_list_insert(PMEMobjpool *pop, size_t pe_offset, void *head,
		    PMEMoid dest, int before, PMEMoid oid)
{
	LOG(3, "pop %p pe_offset %zu head %p dest.off 0x%016" PRIx64
	    " before %d oid.off 0x%016" PRIx64,
	    pop, pe_offset, head, dest.off, before, oid.off);
	PMEMOBJ_API_START();

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();
	ASSERT(OBJ_OID_IS_VALID(pop, oid));
	ASSERT(OBJ_OID_IS_VALID(pop, dest));

	ASSERT(pe_offset <= pmemobj_alloc_usable_size(dest)
			- sizeof(struct list_entry));
	ASSERT(pe_offset <= pmemobj_alloc_usable_size(oid)
			- sizeof(struct list_entry));

	int ret = list_insert(pop, (ssize_t)pe_offset, head, dest, before, oid);

	PMEMOBJ_API_END();
	return ret;
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
	LOG(3, "pop %p pe_offset %zu head %p dest.off 0x%016" PRIx64
	    " before %d size %zu type_num %" PRIu64,
	    pop, pe_offset, head, dest.off, before, size, type_num);

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();
	ASSERT(OBJ_OID_IS_VALID(pop, dest));

	ASSERT(pe_offset <= pmemobj_alloc_usable_size(dest)
			- sizeof(struct list_entry));
	ASSERT(pe_offset <= size - sizeof(struct list_entry));

	if (size > PMEMOBJ_MAX_ALLOC_SIZE) {
		ERR("requested size too large");
		errno = ENOMEM;
		return OID_NULL;
	}

	PMEMOBJ_API_START();
	struct constr_args carg;

	carg.constructor = constructor;
	carg.arg = arg;
	carg.zero_init = 0;

	PMEMoid retoid = OID_NULL;
	list_insert_new_user(pop, pe_offset, head, dest, before, size, type_num,
			constructor_alloc, &carg, &retoid);

	PMEMOBJ_API_END();
	return retoid;
}

/*
 * pmemobj_list_remove -- removes object from a list
 */
int
pmemobj_list_remove(PMEMobjpool *pop, size_t pe_offset, void *head,
		    PMEMoid oid, int free)
{
	LOG(3, "pop %p pe_offset %zu head %p oid.off 0x%016" PRIx64 " free %d",
	    pop, pe_offset, head, oid.off, free);
	PMEMOBJ_API_START();

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();
	ASSERT(OBJ_OID_IS_VALID(pop, oid));

	ASSERT(pe_offset <= pmemobj_alloc_usable_size(oid)
			- sizeof(struct list_entry));

	int ret;
	if (free)
		ret = list_remove_free_user(pop, pe_offset, head, &oid);
	else
		ret = list_remove(pop, (ssize_t)pe_offset, head, oid);

	PMEMOBJ_API_END();
	return ret;
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
	    " head_old %p head_new %p dest.off 0x%016" PRIx64
	    " before %d oid.off 0x%016" PRIx64 "",
	    pop, pe_old_offset, pe_new_offset,
	    head_old, head_new, dest.off, before, oid.off);
	PMEMOBJ_API_START();

	/* log notice message if used inside a transaction */
	_POBJ_DEBUG_NOTICE_IN_TX();

	ASSERT(OBJ_OID_IS_VALID(pop, oid));
	ASSERT(OBJ_OID_IS_VALID(pop, dest));

	ASSERT(pe_old_offset <= pmemobj_alloc_usable_size(oid)
			- sizeof(struct list_entry));
	ASSERT(pe_new_offset <= pmemobj_alloc_usable_size(oid)
			- sizeof(struct list_entry));
	ASSERT(pe_old_offset <= pmemobj_alloc_usable_size(dest)
			- sizeof(struct list_entry));
	ASSERT(pe_new_offset <= pmemobj_alloc_usable_size(dest)
			- sizeof(struct list_entry));

	int ret = list_move(pop, pe_old_offset, head_old,
				pe_new_offset, head_new,
				dest, before, oid);

	PMEMOBJ_API_END();
	return ret;
}

/*
 * pmemobj_ctl_getU -- programmatically executes a read ctl query
 */
#ifndef _WIN32
static inline
#endif
int
pmemobj_ctl_getU(PMEMobjpool *pop, const char *name, void *arg)
{
	LOG(3, "pop %p name %s arg %p", pop, name, arg);
	return ctl_query(pop == NULL ? NULL : pop->ctl, pop,
			CTL_QUERY_PROGRAMMATIC, name, CTL_QUERY_READ, arg);
}

/*
 * pmemobj_ctl_setU -- programmatically executes a write ctl query
 */
#ifndef _WIN32
static inline
#endif
int
pmemobj_ctl_setU(PMEMobjpool *pop, const char *name, void *arg)
{
	LOG(3, "pop %p name %s arg %p", pop, name, arg);
	return ctl_query(pop == NULL ? NULL : pop->ctl, pop,
		CTL_QUERY_PROGRAMMATIC, name, CTL_QUERY_WRITE, arg);
}

/*
 * pmemobj_ctl_execU -- programmatically executes a runnable ctl query
 */
#ifndef _WIN32
static inline
#endif
int
pmemobj_ctl_execU(PMEMobjpool *pop, const char *name, void *arg)
{
	LOG(3, "pop %p name %s arg %p", pop, name, arg);
	return ctl_query(pop == NULL ? NULL : pop->ctl, pop,
		CTL_QUERY_PROGRAMMATIC, name, CTL_QUERY_RUNNABLE, arg);
}

#ifndef _WIN32
/*
 * pmemobj_ctl_get -- programmatically executes a read ctl query
 */
int
pmemobj_ctl_get(PMEMobjpool *pop, const char *name, void *arg)
{
	return pmemobj_ctl_getU(pop, name, arg);
}

/*
 * pmemobj_ctl_set -- programmatically executes a write ctl query
 */
int
pmemobj_ctl_set(PMEMobjpool *pop, const char *name, void *arg)
{
	PMEMOBJ_API_START();

	int ret = pmemobj_ctl_setU(pop, name, arg);

	PMEMOBJ_API_END();
	return ret;
}

/*
 * pmemobj_ctl_exec -- programmatically executes a runnable ctl query
 */
int
pmemobj_ctl_exec(PMEMobjpool *pop, const char *name, void *arg)
{
	PMEMOBJ_API_START();

	int ret =  pmemobj_ctl_execU(pop, name, arg);

	PMEMOBJ_API_END();
	return ret;
}
#else
/*
 * pmemobj_ctl_getW -- programmatically executes a read ctl query
 */
int
pmemobj_ctl_getW(PMEMobjpool *pop, const wchar_t *name, void *arg)
{
	char *uname = util_toUTF8(name);
	if (uname == NULL)
		return -1;

	int ret = pmemobj_ctl_getU(pop, uname, arg);
	util_free_UTF8(uname);

	return ret;
}

/*
 * pmemobj_ctl_setW -- programmatically executes a write ctl query
 */
int
pmemobj_ctl_setW(PMEMobjpool *pop, const wchar_t *name, void *arg)
{
	char *uname = util_toUTF8(name);
	if (uname == NULL)
		return -1;

	int ret = pmemobj_ctl_setU(pop, uname, arg);
	util_free_UTF8(uname);

	return ret;
}

/*
 * pmemobj_ctl_execW -- programmatically executes a runnable ctl query
 */
int
pmemobj_ctl_execW(PMEMobjpool *pop, const wchar_t *name, void *arg)
{
	char *uname = util_toUTF8(name);
	if (uname == NULL)
		return -1;

	int ret = pmemobj_ctl_execU(pop, uname, arg);
	util_free_UTF8(uname);

	return ret;
}
#endif

/*
 * _pobj_debug_notice -- logs notice message if used inside a transaction
 */
void
_pobj_debug_notice(const char *api_name, const char *file, int line)
{
#ifdef DEBUG
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

#if VG_PMEMCHECK_ENABLED
/*
 * pobj_emit_log -- logs library and function names to pmemcheck store log
 */
void
pobj_emit_log(const char *func, int order)
{
	util_emit_log("libpmemobj", func, order);
}
#endif

#if FAULT_INJECTION
void
pmemobj_inject_fault_at(enum pmem_allocation_type type, int nth,
							const char *at)
{
	common_inject_fault_at(type, nth, at);
}

int
pmemobj_fault_injection_enabled(void)
{
	return common_fault_injection_enabled();
}
#endif
