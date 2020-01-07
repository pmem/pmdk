// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * map_ctree.h -- common interface for maps
 */

#ifndef MAP_CTREE_H
#define MAP_CTREE_H

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct map_ops ctree_map_ops;

#define MAP_CTREE (&ctree_map_ops)

#ifdef __cplusplus
}
#endif

#endif /* MAP_CTREE_H */
