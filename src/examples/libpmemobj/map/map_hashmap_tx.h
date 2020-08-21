/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2020, Intel Corporation */

/*
 * map_hashmap_tx.h -- common interface for maps
 */

#ifndef MAP_HASHMAP_TX_H
#define MAP_HASHMAP_TX_H

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct map_ops hashmap_tx_ops;

#define MAP_HASHMAP_TX (&hashmap_tx_ops)

#ifdef __cplusplus
}
#endif

#endif /* MAP_HASHMAP_TX_H */
