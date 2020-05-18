// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * map_ravl.h -- internal definitions for map_ravl
 */

#ifndef MAP_RAVL_H
#define MAP_RAVL_H

#include "libpmem2.h"

struct map_ravl;

int map_ravl_new(struct map_ravl **mr);
void  map_ravl_delete(struct map_ravl **mr);
struct pmem2_map *map_ravl_find(struct map_ravl **mr, const void *addr,
		size_t size);
int map_ravl_add(struct map_ravl **mr, struct pmem2_map *map);
int map_ravl_remove(struct map_ravl **mr, struct pmem2_map *map);

#endif
