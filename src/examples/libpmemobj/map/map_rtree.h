// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * map_rtree.h -- common interface for maps
 */

#ifndef MAP_RTREE_H
#define MAP_RTREE_H

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct map_ops rtree_map_ops;

#define MAP_RTREE (&rtree_map_ops)

#ifdef __cplusplus
}
#endif

#endif /* MAP_RTREE_H */
