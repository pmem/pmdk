// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * map_hashmap_atomic.h -- common interface for maps
 */

#ifndef MAP_HASHMAP_ATOMIC_H
#define MAP_HASHMAP_ATOMIC_H

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct map_ops hashmap_atomic_ops;

#define MAP_HASHMAP_ATOMIC (&hashmap_atomic_ops)

#ifdef __cplusplus
}
#endif

#endif /* MAP_HASHMAP_ATOMIC_H */
