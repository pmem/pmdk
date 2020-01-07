// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

/*
 * rbtree_map.h -- TreeMap sorted collection implementation
 */

#ifndef RBTREE_MAP_H
#define RBTREE_MAP_H

#include <libpmemobj.h>

#ifndef RBTREE_MAP_TYPE_OFFSET
#define RBTREE_MAP_TYPE_OFFSET 1016
#endif

struct rbtree_map;
TOID_DECLARE(struct rbtree_map, RBTREE_MAP_TYPE_OFFSET + 0);

int rbtree_map_check(PMEMobjpool *pop, TOID(struct rbtree_map) map);
int rbtree_map_create(PMEMobjpool *pop, TOID(struct rbtree_map) *map,
	void *arg);
int rbtree_map_destroy(PMEMobjpool *pop, TOID(struct rbtree_map) *map);
int rbtree_map_insert(PMEMobjpool *pop, TOID(struct rbtree_map) map,
	uint64_t key, PMEMoid value);
int rbtree_map_insert_new(PMEMobjpool *pop, TOID(struct rbtree_map) map,
		uint64_t key, size_t size, unsigned type_num,
		void (*constructor)(PMEMobjpool *pop, void *ptr, void *arg),
		void *arg);
PMEMoid rbtree_map_remove(PMEMobjpool *pop, TOID(struct rbtree_map) map,
		uint64_t key);
int rbtree_map_remove_free(PMEMobjpool *pop, TOID(struct rbtree_map) map,
		uint64_t key);
int rbtree_map_clear(PMEMobjpool *pop, TOID(struct rbtree_map) map);
PMEMoid rbtree_map_get(PMEMobjpool *pop, TOID(struct rbtree_map) map,
		uint64_t key);
int rbtree_map_lookup(PMEMobjpool *pop, TOID(struct rbtree_map) map,
		uint64_t key);
int rbtree_map_foreach(PMEMobjpool *pop, TOID(struct rbtree_map) map,
	int (*cb)(uint64_t key, PMEMoid value, void *arg), void *arg);
int rbtree_map_is_empty(PMEMobjpool *pop, TOID(struct rbtree_map) map);

#endif /* RBTREE_MAP_H */
