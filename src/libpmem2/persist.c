/*
 * Copyright 2019, Intel Corporation
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
 * pmem2_log_flush -- log flush attempt for the given range
 */
static inline void
pmem2_log_flush(const void *addr, size_t len)
{
	LOG(15, "addr %p len %zu", addr, len);

	VALGRIND_DO_CHECK_MEM_IS_ADDRESSABLE(addr, len);
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
 * pmem2_persist_noflush -- make any changes to a range of pmem persistent
 */
static void
pmem2_persist_noflush(const void *addr, size_t len)
{
	pmem2_log_flush(addr, len);
	pmem2_drain();
}

/*
 * pmem2_persist_cpu_cache -- make any changes to a range of pmem persistent
 */
static void
pmem2_persist_cpu_cache(const void *addr, size_t len)
{
	pmem2_flush_cpu_cache(addr, len);
	pmem2_drain();
}

/*
 * pmem2_persist_pages_internal -- flush CPU and OS caches for the given range
 */
static int
pmem2_flush_file_buffers(const void *addr, size_t len, int autorestart)
{
	int olderrno = errno;

	pmem2_log_flush(addr, len);

	/*
	 * Flushing using OS-provided mechanisms requires that the address to
	 * be a multiple of page size.
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
		struct pmem2_map *map = pmem2_get_mapping(addr, len);
		if (!map)
			break;

		size_t flush;
		if (map->addr < addr) {
			size_t remaining_in_map = (uintptr_t)map->addr
					+ map->reserved_length
					- (uintptr_t)addr;
			if (remaining_in_map > len)
				flush = remaining_in_map;
			else
				flush = len;
		} else {
			size_t off = (uintptr_t)map->addr - (uintptr_t)addr;
			addr = (const void *)(((uintptr_t)addr) + off);
			len -= off;

			if (map->reserved_length < len)
				flush = map->reserved_length;
			else
				flush = len;
		}

		int ret1 = os_flush_file_buffers(map, addr, flush,
				autorestart);
		if (ret1)
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
	 * Restarting on EINTR in general is a bad idea, but we currently
	 * don't have any way to communicate the failure outside.
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
 * pmem2_drain_stub -- pages variant of pmem2_drain
 */
static void
pmem2_drain_stub(void)
{
	LOG(15, NULL);
}

pmem2_persist_fn
pmem2_get_persist_fn(struct pmem2_map *map)
{
	switch (map->effective_granularity) {
		case PMEM2_GRANULARITY_PAGE:
			return pmem2_persist_pages;
		case PMEM2_GRANULARITY_CACHE_LINE:
			return pmem2_persist_cpu_cache;
		case PMEM2_GRANULARITY_BYTE:
			return pmem2_persist_noflush;
		default:
			abort();
	}
}

pmem2_flush_fn
pmem2_get_flush_fn(struct pmem2_map *map)
{
	switch (map->effective_granularity) {
		case PMEM2_GRANULARITY_PAGE:
			return pmem2_persist_pages;
		case PMEM2_GRANULARITY_CACHE_LINE:
			return pmem2_flush_cpu_cache;
		case PMEM2_GRANULARITY_BYTE:
			return pmem2_log_flush;
		default:
			abort();
	}
}

pmem2_drain_fn
pmem2_get_drain_fn(struct pmem2_map *map)
{
	switch (map->effective_granularity) {
		case PMEM2_GRANULARITY_PAGE:
			return pmem2_drain_stub;
		case PMEM2_GRANULARITY_CACHE_LINE:
			return pmem2_drain;
		case PMEM2_GRANULARITY_BYTE:
			return pmem2_drain;
		default:
			abort();
	}
}
