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
 * pmem2_sync_cacheline -- reads file type for map and check
 * if it is device dax or reg file, depend on file type
 * performs proper sync operation
 */
static int
pmem2_sync_cacheline(struct pmem2_map *map, void *ptr, size_t size)
{
	enum pmem2_file_type type;
	int ret =  pmem2_get_type_from_stat(map->map_st, &type);
	if (ret)
		return ret;

	if (type == PMEM2_FTYPE_REG) {
		size_t len = MIN(Pagesize, size);
		ret = pmem2_flush_file_buffers_os(map, ptr, len, 1);
		if (ret) {
			LOG(1, "cannot flush buffers addr %p len %zu",
				ptr, len);
			return PMEM2_E_ERRNO;
		}
	}
	if (type == PMEM2_FTYPE_DEVDAX) {
		int region_id = pmem2_device_dax_region_find(map->map_st);
		if (region_id < 0) {
			LOG(1, "cannot find region id for stat %p",
				map->map_st);
			return PMEM2_E_ERRNO;
		}
		if (pmem2_deep_sync_write(region_id)) {
			LOG(1, "cannot write to deep_flush file for region %d",
				region_id);
			return PMEM2_E_ERRNO;
		}
	}

	return 0;
}

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
			ERR("unknown graunlarity value %d", g);
			return PMEM2_E_GRANULARITY_NOT_SUPPORTED;
	}

	return 0;
}
