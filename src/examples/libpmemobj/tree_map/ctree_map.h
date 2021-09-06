/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2020, Intel Corporation */

/*
 * ctree_map.h -- TreeMap sorted collection implementation
 */

#ifndef CTREE_MAP_H
#define CTREE_MAP_H

#include <libpmemobj.h>

#ifndef CTREE_MAP_TYPE_OFFSET
#define CTREE_MAP_TYPE_OFFSET 1008
#endif

struct ctree_map;
TOID_DECLARE(struct ctree_map, CTREE_MAP_TYPE_OFFSET + 0);

int ctree_map_check(PMEMobjpool *pop, TOID(struct ctree_map) map);
int ctree_map_create(PMEMobjpool *pop, TOID(struct ctree_map) *map, void *arg);
int ctree_map_destroy(PMEMobjpool *pop, TOID(struct ctree_map) *map);
int ctree_map_insert(PMEMobjpool *pop, TOID(struct ctree_map) map,
	uint64_t key, PMEMoid value);
int ctree_map_insert_new(PMEMobjpool *pop, TOID(struct ctree_map) map,
		uint64_t key, size_t size, unsigned type_num,
		void (*constructor)(PMEMobjpool *pop, void *ptr, void *arg),
		void *arg);
PMEMoid ctree_map_remove(PMEMobjpool *pop, TOID(struct ctree_map) map,
		uint64_t key);
int ctree_map_remove_free(PMEMobjpool *pop, TOID(struct ctree_map) map,
		uint64_t key);
int ctree_map_clear(PMEMobjpool *pop, TOID(struct ctree_map) map);
PMEMoid ctree_map_get(PMEMobjpool *pop, TOID(struct ctree_map) map,
		uint64_t key);
int ctree_map_lookup(PMEMobjpool *pop, TOID(struct ctree_map) map,
		uint64_t key);
int ctree_map_foreach(PMEMobjpool *pop, TOID(struct ctree_map) map,
	int (*cb)(uint64_t key, PMEMoid value, void *arg), void *arg);
int ctree_map_is_empty(PMEMobjpool *pop, TOID(struct ctree_map) map);

#endif /* CTREE_MAP_H */
