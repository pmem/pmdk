// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * map_rbtree.h -- common interface for maps
 */

#ifndef MAP_RBTREE_H
#define MAP_RBTREE_H

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct map_ops rbtree_map_ops;

#define MAP_RBTREE (&rbtree_map_ops)

#ifdef __cplusplus
}
#endif

#endif /* MAP_RBTREE_H */
