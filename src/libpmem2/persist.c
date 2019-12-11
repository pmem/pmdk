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
void
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
