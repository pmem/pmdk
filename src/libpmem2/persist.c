// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * persist.c -- pmem2_get_[persist|flush|drain]_fn
 */

#include <errno.h>
#include <stdlib.h>

#include "libpmem2.h"
#include "map.h"
#include "out.h"
#include "persist.h"
#include "pmem2_arch.h"
#include "valgrind_internal.h"

static struct pmem2_arch_info Info;

/*
 * pmem2_persist_init -- initialize persist module
 */
void
pmem2_persist_init(void)
{
	Info.memmove_nodrain = NULL;
	Info.memset_nodrain = NULL;
	Info.flush = NULL;
	Info.fence = NULL;
	Info.flush_has_builtin_fence = 0;

	pmem2_arch_init(&Info);
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
			break;
		case PMEM2_GRANULARITY_CACHE_LINE:
			map->persist_fn = pmem2_persist_cpu_cache;
			map->flush_fn = pmem2_flush_cpu_cache;
			map->drain_fn = pmem2_drain;
			break;
		case PMEM2_GRANULARITY_BYTE:
			map->persist_fn = pmem2_persist_noflush;
			map->flush_fn = pmem2_flush_nop;
			map->drain_fn = pmem2_drain;
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
	return map->persist_fn;
}

/*
 * pmem2_get_flush_fn - return a pointer to a function responsible for
 * flushing data in range owned by pmem2_map
 */
pmem2_flush_fn
pmem2_get_flush_fn(struct pmem2_map *map)
{
	return map->flush_fn;
}

/*
 * pmem2_get_drain_fn - return a pointer to a function responsible for
 * draining flushes in range owned by pmem2_map
 */
pmem2_drain_fn
pmem2_get_drain_fn(struct pmem2_map *map)
{
	return map->drain_fn;
}
