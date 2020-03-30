// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * deep_sync.c -- pmem2_deep_sync implementation
 */

#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "libpmem2.h"
#include "deep_sync.h"
#include "os.h"
#include "out.h"
#include "fs.h"
#include "persist.h"
#include "pmem2_utils.h"

/*
 * pmem2_deep_sync -- performs deep sync operation
 */
int
pmem2_deep_sync(struct pmem2_map *map, void *ptr, size_t size)
{
	LOG(3, "map %p ptr %p size %zu", map, ptr, size);

	if (map == NULL)
		return PMEM2_E_NOSUPP;

	uintptr_t map_addr = (uintptr_t)map->addr;
	uintptr_t map_end = map_addr + map->content_length;
	uintptr_t sync_addr = (uintptr_t)ptr;
	uintptr_t sync_end = sync_addr + size;

	if (map_addr > sync_end || map_end < sync_addr) {
		ERR("requested sync rage ptr %p size %zu"
			"does not overlap map %p", ptr, size, map);
		return PMEM2_E_SYNC_RANGE;
	}

	enum pmem2_granularity g = map->effective_granularity;
	switch (g) {
		case PMEM2_GRANULARITY_PAGE:
			/* do nothing - pmem2_persist_fn already did msync */
			break;
		case PMEM2_GRANULARITY_CACHE_LINE:
		{
			if (pmem2_sync_cacheline(map, ptr, size)) {
				LOG(1, "cannot perform deep sync for map %p",
					map);
				return PMEM2_E_SYNC_CACHELINE;
			}
			break;
		}
		case PMEM2_GRANULARITY_BYTE:
		{
			size_t common_part = MIN(size, map->content_length);
			pmem2_persist_fn persist_fn = pmem2_get_persist_fn(map);
			persist_fn(ptr, common_part);
			if (pmem2_sync_cacheline(map, ptr, common_part)) {
				LOG(1, "cannot perform deep sync for map %p",
					map);
				return PMEM2_E_SYNC_CACHELINE;
			}
			break;
		}
		default:
			ASSERT(0); /* unreachable  - map should fail before */
	}

	return 0;
}
