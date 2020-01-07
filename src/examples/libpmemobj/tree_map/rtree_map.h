// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016, Intel Corporation */

/*
 * rtree_map.h -- Radix TreeMap collection implementation
 */

#ifndef RTREE_MAP_H
#define RTREE_MAP_H

#include <libpmemobj.h>

#ifndef RTREE_MAP_TYPE_OFFSET
#define RTREE_MAP_TYPE_OFFSET 1020
#endif

struct rtree_map;
TOID_DECLARE(struct rtree_map, RTREE_MAP_TYPE_OFFSET + 0);

int rtree_map_check(PMEMobjpool *pop, TOID(struct rtree_map) map);
int rtree_map_create(PMEMobjpool *pop, TOID(struct rtree_map) *map, void *arg);
int rtree_map_destroy(PMEMobjpool *pop, TOID(struct rtree_map) *map);
int rtree_map_insert(PMEMobjpool *pop, TOID(struct rtree_map) map,
	const unsigned char *key, uint64_t key_size, PMEMoid value);
int rtree_map_insert_new(PMEMobjpool *pop, TOID(struct rtree_map) map,
		const unsigned char *key, uint64_t key_size,
		size_t size, unsigned type_num,
		void (*constructor)(PMEMobjpool *pop, void *ptr, void *arg),
		void *arg);
PMEMoid rtree_map_remove(PMEMobjpool *pop, TOID(struct rtree_map) map,
		const unsigned char *key, uint64_t key_size);
int rtree_map_remove_free(PMEMobjpool *pop, TOID(struct rtree_map) map,
		const unsigned char *key, uint64_t key_size);
int rtree_map_clear(PMEMobjpool *pop, TOID(struct rtree_map) map);
PMEMoid rtree_map_get(PMEMobjpool *pop, TOID(struct rtree_map) map,
		const unsigned char *key, uint64_t key_size);
int rtree_map_lookup(PMEMobjpool *pop, TOID(struct rtree_map) map,
		const unsigned char *key, uint64_t key_size);
int rtree_map_foreach(PMEMobjpool *pop, TOID(struct rtree_map) map,
	int (*cb)(const unsigned char *key, uint64_t key_size,
		PMEMoid value, void *arg),
	void *arg);
int rtree_map_is_empty(PMEMobjpool *pop, TOID(struct rtree_map) map);

#endif /* RTREE_MAP_H */
