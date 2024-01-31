// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * deep_flush.c -- pmem2_deep_flush implementation
 */

#include <stdlib.h>

#include "libpmem2.h"
#include "deep_flush.h"
#include "out.h"
#include "pmem2_utils.h"

/*
 * pmem2_deep_flush -- performs deep flush operation
 */
int
pmem2_deep_flush(struct pmem2_map *map, void *ptr, size_t size)
{
	LOG(3, "map %p ptr %p size %zu", map, ptr, size);
	PMEM2_ERR_CLR();

	uintptr_t map_addr = (uintptr_t)map->addr;
	uintptr_t map_end = map_addr + map->content_length;
	uintptr_t flush_addr = (uintptr_t)ptr;
	uintptr_t flush_end = flush_addr + size;

	if (flush_addr < map_addr || flush_end > map_end) {
		ERR_WO_ERRNO(
			"requested deep flush rage ptr %p size %zu exceeds map range %p",
			ptr, size, map);
		return PMEM2_E_DEEP_FLUSH_RANGE;
	}

	int ret = map->deep_flush_fn(map, ptr, size);
	if (ret) {
		CORE_LOG_ERROR(
			"cannot perform deep flush operation for map %p", map);
		return ret;
	}

	return 0;
}
