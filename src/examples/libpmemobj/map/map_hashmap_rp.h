// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * map_hashmap_rp.h -- common interface for maps
 */

#ifndef MAP_HASHMAP_RP_H
#define MAP_HASHMAP_RP_H

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct map_ops hashmap_rp_ops;

#define MAP_HASHMAP_RP (&hashmap_rp_ops)

#ifdef __cplusplus
}
#endif

#endif /* MAP_HASHMAP_RP_H */
