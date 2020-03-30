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

int pmem2_deep_sync_write(int region_id);
int pmem2_sync_cacheline(struct pmem2_map *map, void *ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif
