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
#include "pmem2_utils.h"

/*
 * pmem2_deep_sync -- performs deep sync operation
 */
int
pmem2_deep_sync(struct pmem2_map *map, void *ptr, size_t size)
{
	LOG(3, "map %p ptr %p size %zu", map, ptr, size);

	uintptr_t map_addr = (uintptr_t)map->addr;
	uintptr_t map_end = map_addr + map->content_length;
	uintptr_t sync_addr = (uintptr_t)ptr;
	uintptr_t sync_end = sync_addr + size;

	if (sync_addr < map_addr || sync_end > map_end) {
		ERR("requested sync rage ptr %p size %zu"
			"exceeds map range %p", ptr, size, map);
		return PMEM2_E_SYNC_RANGE;
	}

	int ret = map->deep_sync_fn(map, ptr, size);
	if (ret) {
		LOG(1, "cannot perform deep sync operation for map %p", map);
		return ret;
	}

	return 0;
}
