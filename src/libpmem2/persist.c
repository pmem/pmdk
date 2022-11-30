// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2022, Intel Corporation */

/*
 * persist.c -- pmem2_get_[persist|flush|drain]_fn
 */

#include <errno.h>
#include <stdlib.h>

#include "libpmem2.h"
#include "libpmem2/base.h"
#include "map.h"
#include "out.h"
#include "os.h"
#include "persist.h"
#include "deep_flush.h"
#include "pmem2_arch.h"
#include "pmem2_utils.h"
#include "valgrind_internal.h"

static struct pmem2_arch_info Info;

/*
 * memmove_nodrain_libc -- (internal) memmove to pmem using libc
 */
static void *
memmove_nodrain_libc(void *pmemdest, const void *src, size_t len,
		unsigned flags, flush_func flush,
		const struct memmove_nodrain *memmove_funcs)
{
#ifdef DEBUG
	if (flags & ~PMEM2_F_MEM_VALID_FLAGS)
		ERR("invalid flags 0x%x", flags);
#endif
	LOG(15, "pmemdest %p src %p len %zu flags 0x%x", pmemdest, src, len,
			flags);

	SUPPRESS_UNUSED(memmove_funcs);

	memmove(pmemdest, src, len);

	if (!(flags & PMEM2_F_MEM_NOFLUSH))
		flush(pmemdest, len);

	return pmemdest;
}

/*
 * memset_nodrain_libc -- (internal) memset to pmem using libc
 */
static void *
memset_nodrain_libc(void *pmemdest, int c, size_t len, unsigned flags,
		flush_func flush, const struct memset_nodrain *memset_funcs)
{
#ifdef DEBUG
	if (flags & ~PMEM2_F_MEM_VALID_FLAGS)
		ERR("invalid flags 0x%x", flags);
#endif
	LOG(15, "pmemdest %p c 0x%x len %zu flags 0x%x", pmemdest, c, len,
			flags);

	SUPPRESS_UNUSED(memset_funcs);

	memset(pmemdest, c, len);

	if (!(flags & PMEM2_F_MEM_NOFLUSH))
		flush(pmemdest, len);

	return pmemdest;
}

/*
 * pmem2_persist_init -- initialize persist module
 */
void
pmem2_persist_init(void)
{
	Info.memmove_nodrain = NULL;
	Info.memset_nodrain = NULL;
	Info.memmove_nodrain_eadr = NULL;
	Info.memset_nodrain_eadr = NULL;
	Info.flush = NULL;
	Info.fence = NULL;
	Info.flush_has_builtin_fence = 0;

	pmem2_arch_init(&Info);

	char *ptr = os_getenv("PMEM_NO_GENERIC_MEMCPY");
	long long no_generic = 0;
	if (ptr)
		no_generic = atoll(ptr);

	if (Info.memmove_nodrain == NULL) {
		if (no_generic) {
			Info.memmove_nodrain = memmove_nodrain_libc;
			Info.memmove_nodrain_eadr = memmove_nodrain_libc;
			LOG(3, "using libc memmove");
		} else {
			Info.memmove_nodrain = memmove_nodrain_generic;
			Info.memmove_nodrain_eadr = memmove_nodrain_generic;
			LOG(3, "using generic memmove");
		}
	}

	if (Info.memset_nodrain == NULL) {
		if (no_generic) {
			Info.memset_nodrain = memset_nodrain_libc;
			Info.memset_nodrain_eadr = memset_nodrain_libc;
			LOG(3, "using libc memset");
		} else {
			Info.memset_nodrain = memset_nodrain_generic;
			Info.memset_nodrain_eadr = memset_nodrain_generic;
			LOG(3, "using generic memset");
		}
	}
}

/*
 * pmem2_drain -- wait for any PM stores to drain from HW buffers
 */
static void
pmem2_drain(void)
{
	LOG(15, NULL);

	Info.fence();
}

/*
 * pmem2_log_flush -- log the flush attempt for the given range
 */
static inline void
pmem2_log_flush(const void *addr, size_t len)
{
	LOG(15, "addr %p len %zu", addr, len);

	VALGRIND_DO_CHECK_MEM_IS_ADDRESSABLE(addr, len);
}

/*
 * pmem2_flush_nop -- NOP version of the flush routine, used in cases where
 * memory behind the mapping is already in persistence domain
 */
static void
pmem2_flush_nop(const void *addr, size_t len)
{
	pmem2_log_flush(addr, len);

	/* nothing more to do, other than telling pmemcheck about it */
	VALGRIND_DO_FLUSH(addr, len);
}

/*
 * pmem2_flush_cpu_cache -- flush processor cache for the given range
 */
static void
pmem2_flush_cpu_cache(const void *addr, size_t len)
{
	pmem2_log_flush(addr, len);

	Info.flush(addr, len);
}

/*
 * pmem2_persist_noflush -- make all changes to a range of pmem persistent
 */
static void
pmem2_persist_noflush(const void *addr, size_t len)
{
	pmem2_flush_nop(addr, len);
	pmem2_drain();
}

/*
 * pmem2_persist_cpu_cache -- make all changes to a range of pmem persistent
 */
static void
pmem2_persist_cpu_cache(const void *addr, size_t len)
{
	pmem2_flush_cpu_cache(addr, len);
	pmem2_drain();
}

/*
 * pmem2_flush_file_buffers -- flush CPU and OS caches for the given range
 */
static int
pmem2_flush_file_buffers(const void *addr, size_t len, int autorestart)
{
	int olderrno = errno;

	pmem2_log_flush(addr, len);

	/*
	 * Flushing using OS-provided mechanisms requires that the address
	 * be a multiple of the page size.
	 * Align address down and change len so that [addr, addr + len) still
	 * contains the initial range.
	 */

	/* round address down to page boundary */
	uintptr_t new_addr = ALIGN_DOWN((uintptr_t)addr, Pagesize);

	/* increase len by the amount we gain when we round addr down */
	len += (uintptr_t)addr - new_addr;

	addr = (const void *)new_addr;

	int ret = 0;

	/*
	 * Find all the mappings overlapping with the [addr, addr + len) range
	 * and flush them, one by one.
	 */
	do {
		struct pmem2_map *map = pmem2_map_find(addr, len);
		if (!map)
			break;

		size_t flush;
		size_t remaining = map->reserved_length;
		if (map->addr < addr) {
			/*
			 * Addr is inside of the mapping, so we have to decrease
			 * the remaining length by an offset from the start
			 * of our mapping.
			 */
			remaining -= (uintptr_t)addr - (uintptr_t)map->addr;
		} else if (map->addr == addr) {
			/* perfect match, there's nothing to do in this case */
		} else {
			/*
			 * map->addr > addr, so we have to skip the hole
			 * between addr and map->addr.
			 */
			len -= (uintptr_t)map->addr - (uintptr_t)addr;
			addr = map->addr;
		}

		if (len > remaining)
			flush = remaining;
		else
			flush = len;

		int ret1 = pmem2_flush_file_buffers_os(map, addr, flush,
				autorestart);
		if (ret1 != 0)
			ret = ret1;

		addr = ((const char *)addr) + flush;
		len -= flush;
	} while (len > 0);

	errno = olderrno;

	return ret;
}

/*
 * pmem2_persist_pages -- flush processor cache for the given range
 */
static void
pmem2_persist_pages(const void *addr, size_t len)
{
	/*
	 * Restarting on EINTR in general is a bad idea, but we don't have
	 * any way to communicate the failure outside.
	 */
	const int autorestart = 1;

	int ret = pmem2_flush_file_buffers(addr, len, autorestart);
	if (ret) {
		/*
		 * 1) There's no way to propagate this error. Silently ignoring
		 *    it would lead to data corruption.
		 * 2) non-pmem code path shouldn't be used in production.
		 *
		 * The only sane thing to do is to crash the application. Sorry.
		 */
		abort();
	}
}

/*
 * pmem2_drain_nop -- variant of pmem2_drain for page granularity;
 * it is a NOP because the flush part has built-in drain
 */
static void
pmem2_drain_nop(void)
{
	LOG(15, NULL);
}

/*
 * pmem2_deep_flush_page -- do nothing - pmem2_persist_fn already did msync
 */
int
pmem2_deep_flush_page(struct pmem2_map *map, void *ptr, size_t size)
{
	LOG(3, "map %p ptr %p size %zu", map, ptr, size);
	return 0;
}

/*
 * pmem2_deep_flush_cache -- flush buffers for fsdax or write
 * to deep_flush for DevDax
 */
int
pmem2_deep_flush_cache(struct pmem2_map *map, void *ptr, size_t size)
{
	LOG(3, "map %p ptr %p size %zu", map, ptr, size);

	enum pmem2_file_type type = map->source.value.ftype;

	/*
	 * XXX: this should be moved to pmem2_deep_flush_dax
	 * while refactoring abstraction
	 */
	if (type == PMEM2_FTYPE_DEVDAX)
		pmem2_persist_cpu_cache(ptr, size);

	int ret = pmem2_deep_flush_dax(map, ptr, size);
	if (ret < 0) {
		LOG(1, "cannot perform deep flush cache for map %p", map);
		return ret;
	}

	return 0;
}

/*
 * pmem2_deep_flush_byte -- flush cpu cache and perform deep flush for dax
 */
int
pmem2_deep_flush_byte(struct pmem2_map *map, void *ptr, size_t size)
{
	LOG(3, "map %p ptr %p size %zu", map, ptr, size);

	if (map->source.type == PMEM2_SOURCE_ANON) {
		ERR("Anonymous source does not support deep flush");
		return PMEM2_E_NOSUPP;
	}

	ASSERT(map->source.type == PMEM2_SOURCE_FD ||
		map->source.type == PMEM2_SOURCE_HANDLE);

	enum pmem2_file_type type = map->source.value.ftype;

	/*
	 * XXX: this should be moved to pmem2_deep_flush_dax
	 * while refactoring abstraction
	 */
	if (type == PMEM2_FTYPE_DEVDAX)
		pmem2_persist_cpu_cache(ptr, size);

	int ret = pmem2_deep_flush_dax(map, ptr, size);
	if (ret < 0) {
		LOG(1, "cannot perform deep flush byte for map %p", map);
		return ret;
	}

	return 0;
}

/*
 * pmem2_set_flush_fns -- set function pointers related to flushing
 */
void
pmem2_set_flush_fns(struct pmem2_map *map)
{
	switch (map->effective_granularity) {
		case PMEM2_GRANULARITY_PAGE:
			map->persist_fn = pmem2_persist_pages;
			map->flush_fn = pmem2_persist_pages;
			map->drain_fn = pmem2_drain_nop;
			map->deep_flush_fn = pmem2_deep_flush_page;
			break;
		case PMEM2_GRANULARITY_CACHE_LINE:
			map->persist_fn = pmem2_persist_cpu_cache;
			map->flush_fn = pmem2_flush_cpu_cache;
			map->drain_fn = pmem2_drain;
			map->deep_flush_fn = pmem2_deep_flush_cache;
			break;
		case PMEM2_GRANULARITY_BYTE:
			map->persist_fn = pmem2_persist_noflush;
			map->flush_fn = pmem2_flush_nop;
			map->drain_fn = pmem2_drain;
			map->deep_flush_fn = pmem2_deep_flush_byte;
			break;
		default:
			abort();
	}

}

/*
 * pmem2_get_persist_fn - return a pointer to a function responsible for
 * persisting data in range owned by pmem2_map
 */
pmem2_persist_fn
pmem2_get_persist_fn(struct pmem2_map *map)
{
	/* we do not need to clear err because this function cannot fail */
	return map->persist_fn;
}

/*
 * pmem2_get_flush_fn - return a pointer to a function responsible for
 * flushing data in range owned by pmem2_map
 */
pmem2_flush_fn
pmem2_get_flush_fn(struct pmem2_map *map)
{
	/* we do not need to clear err because this function cannot fail */
	return map->flush_fn;
}

/*
 * pmem2_get_drain_fn - return a pointer to a function responsible for
 * draining flushes in range owned by pmem2_map
 */
pmem2_drain_fn
pmem2_get_drain_fn(struct pmem2_map *map)
{
	/* we do not need to clear err because this function cannot fail */
	return map->drain_fn;
}

/*
 * pmem2_memmove_nonpmem -- mem[move|cpy] followed by an msync
 */
static void *
pmem2_memmove_nonpmem(void *pmemdest, const void *src, size_t len,
		unsigned flags)
{
#ifdef DEBUG
	if (flags & ~PMEM2_F_MEM_VALID_FLAGS)
		ERR("invalid flags 0x%x", flags);
#endif
	PMEM2_API_START("pmem2_memmove");
	Info.memmove_nodrain(pmemdest, src, len,
		flags & ~PMEM2_F_MEM_NODRAIN,
		Info.flush, &Info.memmove_funcs);

	if (!(flags & PMEM2_F_MEM_NOFLUSH))
		pmem2_persist_pages(pmemdest, len);

	PMEM2_API_END("pmem2_memmove");
	return pmemdest;
}

/*
 * pmem2_memset_nonpmem -- memset followed by an msync
 */
static void *
pmem2_memset_nonpmem(void *pmemdest, int c, size_t len, unsigned flags)
{
#ifdef DEBUG
	if (flags & ~PMEM2_F_MEM_VALID_FLAGS)
		ERR("invalid flags 0x%x", flags);
#endif
	PMEM2_API_START("pmem2_memset");
	Info.memset_nodrain(pmemdest, c, len,
		flags & ~PMEM2_F_MEM_NODRAIN,
		Info.flush, &Info.memset_funcs);

	if (!(flags & PMEM2_F_MEM_NOFLUSH))
		pmem2_persist_pages(pmemdest, len);

	PMEM2_API_END("pmem2_memset");
	return pmemdest;
}

/*
 * pmem2_memmove -- mem[move|cpy] to pmem
 */
static void *
pmem2_memmove(void *pmemdest, const void *src, size_t len,
		unsigned flags)
{
#ifdef DEBUG
	if (flags & ~PMEM2_F_MEM_VALID_FLAGS)
		ERR("invalid flags 0x%x", flags);
#endif
	PMEM2_API_START("pmem2_memmove");
	Info.memmove_nodrain(pmemdest, src, len, flags, Info.flush,
			&Info.memmove_funcs);
	if ((flags & (PMEM2_F_MEM_NODRAIN | PMEM2_F_MEM_NOFLUSH)) == 0)
		pmem2_drain();

	PMEM2_API_END("pmem2_memmove");
	return pmemdest;
}

/*
 * pmem2_memset -- memset to pmem
 */
static void *
pmem2_memset(void *pmemdest, int c, size_t len, unsigned flags)
{
#ifdef DEBUG
	if (flags & ~PMEM2_F_MEM_VALID_FLAGS)
		ERR("invalid flags 0x%x", flags);
#endif
	PMEM2_API_START("pmem2_memset");
	Info.memset_nodrain(pmemdest, c, len, flags, Info.flush,
			&Info.memset_funcs);
	if ((flags & (PMEM2_F_MEM_NODRAIN | PMEM2_F_MEM_NOFLUSH)) == 0)
		pmem2_drain();

	PMEM2_API_END("pmem2_memset");
	return pmemdest;
}

/*
 * pmem2_memmove_eadr -- mem[move|cpy] to pmem, platform supports eADR
 */
static void *
pmem2_memmove_eadr(void *pmemdest, const void *src, size_t len,
		unsigned flags)
{
#ifdef DEBUG
	if (flags & ~PMEM2_F_MEM_VALID_FLAGS)
		ERR("invalid flags 0x%x", flags);
#endif
	PMEM2_API_START("pmem2_memmove");
	Info.memmove_nodrain_eadr(pmemdest, src, len, flags, Info.flush,
			&Info.memmove_funcs);
	if ((flags & (PMEM2_F_MEM_NODRAIN | PMEM2_F_MEM_NOFLUSH)) == 0)
		pmem2_drain();

	PMEM2_API_END("pmem2_memmove");
	return pmemdest;
}

/*
 * pmem2_memset_eadr -- memset to pmem, platform supports eADR
 */
static void *
pmem2_memset_eadr(void *pmemdest, int c, size_t len, unsigned flags)
{
#ifdef DEBUG
	if (flags & ~PMEM2_F_MEM_VALID_FLAGS)
		ERR("invalid flags 0x%x", flags);
#endif
	PMEM2_API_START("pmem2_memset");
	Info.memset_nodrain_eadr(pmemdest, c, len, flags, Info.flush,
			&Info.memset_funcs);
	if ((flags & (PMEM2_F_MEM_NODRAIN | PMEM2_F_MEM_NOFLUSH)) == 0)
		pmem2_drain();

	PMEM2_API_END("pmem2_memset");
	return pmemdest;
}

/*
 * pmem2_set_mem_fns -- set function pointers related to mem[move|cpy|set]
 */
void
pmem2_set_mem_fns(struct pmem2_map *map)
{
	switch (map->effective_granularity) {
		case PMEM2_GRANULARITY_PAGE:
			map->memmove_fn = pmem2_memmove_nonpmem;
			map->memcpy_fn = pmem2_memmove_nonpmem;
			map->memset_fn = pmem2_memset_nonpmem;
			break;
		case PMEM2_GRANULARITY_CACHE_LINE:
			map->memmove_fn = pmem2_memmove;
			map->memcpy_fn = pmem2_memmove;
			map->memset_fn = pmem2_memset;
			break;
		case PMEM2_GRANULARITY_BYTE:
			map->memmove_fn = pmem2_memmove_eadr;
			map->memcpy_fn = pmem2_memmove_eadr;
			map->memset_fn = pmem2_memset_eadr;
			break;
		default:
			abort();
	}

}

/*
 * pmem2_get_memmove_fn - return a pointer to a function
 */
pmem2_memmove_fn
pmem2_get_memmove_fn(struct pmem2_map *map)
{
	/* we do not need to clear err because this function cannot fail */
	return map->memmove_fn;
}

/*
 * pmem2_get_memcpy_fn - return a pointer to a function
 */
pmem2_memcpy_fn
pmem2_get_memcpy_fn(struct pmem2_map *map)
{
	/* we do not need to clear err because this function cannot fail */
	return map->memcpy_fn;
}

/*
 * pmem2_get_memset_fn - return a pointer to a function
 */
pmem2_memset_fn
pmem2_get_memset_fn(struct pmem2_map *map)
{
	/* we do not need to clear err because this function cannot fail */
	return map->memset_fn;
}

#if VG_PMEMCHECK_ENABLED
/*
 * pmem2_emit_log -- logs library and function names to pmemcheck store log
 */
void
pmem2_emit_log(const char *func, int order)
{
	util_emit_log("libpmem2", func, order);
}
#endif
