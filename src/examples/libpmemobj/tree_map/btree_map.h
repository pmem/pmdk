/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2020, Intel Corporation */

/*
 * btree_map.h -- TreeMap sorted collection implementation
 */

#ifndef BTREE_MAP_H
#define BTREE_MAP_H

#include <libpmemobj.h>

#ifndef BTREE_MAP_TYPE_OFFSET
#define BTREE_MAP_TYPE_OFFSET 1012
#endif

struct btree_map;
TOID_DECLARE(struct btree_map, BTREE_MAP_TYPE_OFFSET + 0);

int btree_map_check(PMEMobjpool *pop, TOID(struct btree_map) map);
int btree_map_create(PMEMobjpool *pop, TOID(struct btree_map) *map, void *arg);
int btree_map_destroy(PMEMobjpool *pop, TOID(struct btree_map) *map);
int btree_map_insert(PMEMobjpool *pop, TOID(struct btree_map) map,
	uint64_t key, PMEMoid value);
int btree_map_insert_new(PMEMobjpool *pop, TOID(struct btree_map) map,
		uint64_t key, size_t size, unsigned type_num,
		void (*constructor)(PMEMobjpool *pop, void *ptr, void *arg),
		void *arg);
PMEMoid btree_map_remove(PMEMobjpool *pop, TOID(struct btree_map) map,
		uint64_t key);
int btree_map_remove_free(PMEMobjpool *pop, TOID(struct btree_map) map,
		uint64_t key);
int btree_map_clear(PMEMobjpool *pop, TOID(struct btree_map) map);
PMEMoid btree_map_get(PMEMobjpool *pop, TOID(struct btree_map) map,
		uint64_t key);
int btree_map_lookup(PMEMobjpool *pop, TOID(struct btree_map) map,
		uint64_t key);
int btree_map_foreach(PMEMobjpool *pop, TOID(struct btree_map) map,
	int (*cb)(uint64_t key, PMEMoid value, void *arg), void *arg);
int btree_map_is_empty(PMEMobjpool *pop, TOID(struct btree_map) map);

#endif /* BTREE_MAP_H */
