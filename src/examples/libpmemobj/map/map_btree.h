/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2020, Intel Corporation */

/*
 * map_ctree.h -- common interface for maps
 */

#ifndef MAP_BTREE_H
#define MAP_BTREE_H

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct map_ops btree_map_ops;

#define MAP_BTREE (&btree_map_ops)

#ifdef __cplusplus
}
#endif

#endif /* MAP_BTREE_H */
