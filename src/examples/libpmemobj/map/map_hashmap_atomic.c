// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

/*
 * map_hashmap_atomic.c -- common interface for maps
 */

#include <map.h>
#include <hashmap_atomic.h>

#include "map_hashmap_atomic.h"

/*
 * map_hm_atomic_check -- wrapper for hm_atomic_check
 */
static int
map_hm_atomic_check(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct hashmap_atomic) hashmap_atomic;
	TOID_ASSIGN(hashmap_atomic, map.oid);

	return hm_atomic_check(pop, hashmap_atomic);
}

/*
 * map_hm_atomic_count -- wrapper for hm_atomic_count
 */
static size_t
map_hm_atomic_count(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct hashmap_atomic) hashmap_atomic;
	TOID_ASSIGN(hashmap_atomic, map.oid);

	return hm_atomic_count(pop, hashmap_atomic);
}

/*
 * map_hm_atomic_init -- wrapper for hm_atomic_init
 */
static int
map_hm_atomic_init(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct hashmap_atomic) hashmap_atomic;
	TOID_ASSIGN(hashmap_atomic, map.oid);

	return hm_atomic_init(pop, hashmap_atomic);
}

/*
 * map_hm_atomic_new -- wrapper for hm_atomic_create
 */
static int
map_hm_atomic_create(PMEMobjpool *pop, TOID(struct map) *map, void *arg)
{
	TOID(struct hashmap_atomic) *hashmap_atomic =
		(TOID(struct hashmap_atomic) *)map;

	return hm_atomic_create(pop, hashmap_atomic, arg);
}

/*
 * map_hm_atomic_insert -- wrapper for hm_atomic_insert
 */
static int
map_hm_atomic_insert(PMEMobjpool *pop, TOID(struct map) map,
		uint64_t key, PMEMoid value)
{
	TOID(struct hashmap_atomic) hashmap_atomic;
	TOID_ASSIGN(hashmap_atomic, map.oid);

	return hm_atomic_insert(pop, hashmap_atomic, key, value);
}

/*
 * map_hm_atomic_remove -- wrapper for hm_atomic_remove
 */
static PMEMoid
map_hm_atomic_remove(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct hashmap_atomic) hashmap_atomic;
	TOID_ASSIGN(hashmap_atomic, map.oid);

	return hm_atomic_remove(pop, hashmap_atomic, key);
}

/*
 * map_hm_atomic_get -- wrapper for hm_atomic_get
 */
static PMEMoid
map_hm_atomic_get(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct hashmap_atomic) hashmap_atomic;
	TOID_ASSIGN(hashmap_atomic, map.oid);

	return hm_atomic_get(pop, hashmap_atomic, key);
}

/*
 * map_hm_atomic_lookup -- wrapper for hm_atomic_lookup
 */
static int
map_hm_atomic_lookup(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct hashmap_atomic) hashmap_atomic;
	TOID_ASSIGN(hashmap_atomic, map.oid);

	return hm_atomic_lookup(pop, hashmap_atomic, key);
}

/*
 * map_hm_atomic_foreach -- wrapper for hm_atomic_foreach
 */
static int
map_hm_atomic_foreach(PMEMobjpool *pop, TOID(struct map) map,
		int (*cb)(uint64_t key, PMEMoid value, void *arg),
		void *arg)
{
	TOID(struct hashmap_atomic) hashmap_atomic;
	TOID_ASSIGN(hashmap_atomic, map.oid);

	return hm_atomic_foreach(pop, hashmap_atomic, cb, arg);
}

/*
 * map_hm_atomic_cmd -- wrapper for hm_atomic_cmd
 */
static int
map_hm_atomic_cmd(PMEMobjpool *pop, TOID(struct map) map,
		unsigned cmd, uint64_t arg)
{
	TOID(struct hashmap_atomic) hashmap_atomic;
	TOID_ASSIGN(hashmap_atomic, map.oid);

	return hm_atomic_cmd(pop, hashmap_atomic, cmd, arg);
}

struct map_ops hashmap_atomic_ops = {
	/* .check	= */ map_hm_atomic_check,
	/* .create	= */ map_hm_atomic_create,
	/* .destroy	= */ NULL,
	/* .init	= */ map_hm_atomic_init,
	/* .insert	= */ map_hm_atomic_insert,
	/* .insert_new	= */ NULL,
	/* .remove	= */ map_hm_atomic_remove,
	/* .remove_free	= */ NULL,
	/* .clear	= */ NULL,
	/* .get		= */ map_hm_atomic_get,
	/* .lookup	= */ map_hm_atomic_lookup,
	/* .foreach	= */ map_hm_atomic_foreach,
	/* .is_empty	= */ NULL,
	/* .count	= */ map_hm_atomic_count,
	/* .cmd		= */ map_hm_atomic_cmd,
};
