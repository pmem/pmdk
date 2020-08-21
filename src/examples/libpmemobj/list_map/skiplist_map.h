/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

/*
 * skiplist_map.h -- sorted list collection implementation
 */

#ifndef SKIPLIST_MAP_H
#define SKIPLIST_MAP_H

#include <libpmemobj.h>

#ifndef SKIPLIST_MAP_TYPE_OFFSET
#define SKIPLIST_MAP_TYPE_OFFSET 2020
#endif

struct skiplist_map_node;
TOID_DECLARE(struct skiplist_map_node, SKIPLIST_MAP_TYPE_OFFSET + 0);

int skiplist_map_check(PMEMobjpool *pop, TOID(struct skiplist_map_node) map);
int skiplist_map_create(PMEMobjpool *pop, TOID(struct skiplist_map_node) *map,
	void *arg);
int skiplist_map_destroy(PMEMobjpool *pop, TOID(struct skiplist_map_node) *map);
int skiplist_map_insert(PMEMobjpool *pop, TOID(struct skiplist_map_node) map,
		uint64_t key, PMEMoid value);
int skiplist_map_insert_new(PMEMobjpool *pop,
		TOID(struct skiplist_map_node) map, uint64_t key, size_t size,
		unsigned type_num,
		void (*constructor)(PMEMobjpool *pop, void *ptr, void *arg),
		void *arg);
PMEMoid skiplist_map_remove(PMEMobjpool *pop,
		TOID(struct skiplist_map_node) map, uint64_t key);
int skiplist_map_remove_free(PMEMobjpool *pop,
		TOID(struct skiplist_map_node) map, uint64_t key);
int skiplist_map_clear(PMEMobjpool *pop, TOID(struct skiplist_map_node) map);
PMEMoid skiplist_map_get(PMEMobjpool *pop, TOID(struct skiplist_map_node) map,
		uint64_t key);
int skiplist_map_lookup(PMEMobjpool *pop, TOID(struct skiplist_map_node) map,
		uint64_t key);
int skiplist_map_foreach(PMEMobjpool *pop, TOID(struct skiplist_map_node) map,
	int (*cb)(uint64_t key, PMEMoid value, void *arg), void *arg);
int skiplist_map_is_empty(PMEMobjpool *pop, TOID(struct skiplist_map_node) map);

#endif /* SKIPLIST_MAP_H */
