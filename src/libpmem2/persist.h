// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * persist.h -- internal definitions for libpmem2 persist module
 */
#ifndef PMEM2_PERSIST_H
#define PMEM2_PERSIST_H

#include <stddef.h>

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

void pmem2_persist_init(void);

int pmem2_flush_file_buffers_os(struct pmem2_map *map, const void *addr,
		size_t len, int autorestart);
void pmem2_set_flush_fns(struct pmem2_map *map);
void pmem2_set_mem_fns(struct pmem2_map *map);

#ifdef __cplusplus
}
#endif

#endif
