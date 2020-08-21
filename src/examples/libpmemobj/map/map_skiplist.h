/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

/*
 * map_skiplist.h -- common interface for maps
 */

#ifndef MAP_SKIPLIST_H
#define MAP_SKIPLIST_H

#include "map.h"

extern struct map_ops skiplist_map_ops;

#define MAP_SKIPLIST (&skiplist_map_ops)

#endif /* MAP_SKIPLIST_H */
