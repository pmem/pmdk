// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * deep_sync.h -- functions for deep sync functionality
 */

#ifndef PMEM2_DEEP_SYNC_H
#define PMEM2_DEEP_SYNC_H 1

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

int pmem2_deep_sync_write(unsigned region_id);
int pmem2_deep_sync_dax(struct pmem2_map *map);
int pmem2_deep_sync_page(struct pmem2_map *map, void *ptr, size_t size);
int pmem2_deep_sync_cache(struct pmem2_map *map, void *ptr, size_t size);
int pmem2_deep_sync_byte(struct pmem2_map *map, void *ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif
