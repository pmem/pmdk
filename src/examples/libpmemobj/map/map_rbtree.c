// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2021, Intel Corporation */

/*
 * map_rbtree.c -- common interface for maps
 */

#include <map.h>
#include <rbtree_map.h>

#include "map_rbtree.h"

/*
 * map_rbtree_check -- wrapper for rbtree_map_check
 */
static int
map_rbtree_check(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct rbtree_map) rbtree_map;
	TOID_ASSIGN(rbtree_map, map.oid);

	return rbtree_map_check(pop, rbtree_map);
}

/*
 * map_rbtree_create -- wrapper for rbtree_map_new
 */
static int
map_rbtree_create(PMEMobjpool *pop, TOID(struct map) *map, void *arg)
{
	TOID(struct rbtree_map) *rbtree_map =
		(TOID(struct rbtree_map) *)map;

	return rbtree_map_create(pop, rbtree_map, arg);
}

/*
 * map_rbtree_destroy -- wrapper for rbtree_map_delete
 */
static int
map_rbtree_destroy(PMEMobjpool *pop, TOID(struct map) *map)
{
	TOID(struct rbtree_map) *rbtree_map =
		(TOID(struct rbtree_map) *)map;

	return rbtree_map_destroy(pop, rbtree_map);
}

/*
 * map_rbtree_insert -- wrapper for rbtree_map_insert
 */
static int
map_rbtree_insert(PMEMobjpool *pop, TOID(struct map) map,
		uint64_t key, PMEMoid value)
{
	TOID(struct rbtree_map) rbtree_map;
	TOID_ASSIGN(rbtree_map, map.oid);

	return rbtree_map_insert(pop, rbtree_map, key, value);
}

/*
 * map_rbtree_insert_new -- wrapper for rbtree_map_insert_new
 */
static int
map_rbtree_insert_new(PMEMobjpool *pop, TOID(struct map) map,
		uint64_t key, size_t size,
		unsigned type_num,
		void (*constructor)(PMEMobjpool *pop, void *ptr, void *arg),
		void *arg)
{
	TOID(struct rbtree_map) rbtree_map;
	TOID_ASSIGN(rbtree_map, map.oid);

	return rbtree_map_insert_new(pop, rbtree_map, key, size,
			type_num, constructor, arg);
}

/*
 * map_rbtree_remove -- wrapper for rbtree_map_remove
 */
static PMEMoid
map_rbtree_remove(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct rbtree_map) rbtree_map;
	TOID_ASSIGN(rbtree_map, map.oid);

	return rbtree_map_remove(pop, rbtree_map, key);
}

/*
 * map_rbtree_remove_free -- wrapper for rbtree_map_remove_free
 */
static int
map_rbtree_remove_free(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct rbtree_map) rbtree_map;
	TOID_ASSIGN(rbtree_map, map.oid);

	return rbtree_map_remove_free(pop, rbtree_map, key);
}

/*
 * map_rbtree_clear -- wrapper for rbtree_map_clear
 */
static int
map_rbtree_clear(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct rbtree_map) rbtree_map;
	TOID_ASSIGN(rbtree_map, map.oid);

	return rbtree_map_clear(pop, rbtree_map);
}

/*
 * map_rbtree_get -- wrapper for rbtree_map_get
 */
static PMEMoid
map_rbtree_get(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct rbtree_map) rbtree_map;
	TOID_ASSIGN(rbtree_map, map.oid);

	return rbtree_map_get(pop, rbtree_map, key);
}

/*
 * map_rbtree_lookup -- wrapper for rbtree_map_lookup
 */
static int
map_rbtree_lookup(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct rbtree_map) rbtree_map;
	TOID_ASSIGN(rbtree_map, map.oid);

	return rbtree_map_lookup(pop, rbtree_map, key);
}

/*
 * map_rbtree_foreach -- wrapper for rbtree_map_foreach
 */
static int
map_rbtree_foreach(PMEMobjpool *pop, TOID(struct map) map,
		int (*cb)(uint64_t key, PMEMoid value, void *arg),
		void *arg)
{
	TOID(struct rbtree_map) rbtree_map;
	TOID_ASSIGN(rbtree_map, map.oid);

	return rbtree_map_foreach(pop, rbtree_map, cb, arg);
}

/*
 * map_rbtree_is_empty -- wrapper for rbtree_map_is_empty
 */
static int
map_rbtree_is_empty(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct rbtree_map) rbtree_map;
	TOID_ASSIGN(rbtree_map, map.oid);

	return rbtree_map_is_empty(pop, rbtree_map);
}

/*
 * map_rbtree_init -- recovers map state
 * Since there is no need for recovery for rbtree, function is dummy.
 */
static int
map_rbtree_init(PMEMobjpool *pop, TOID(struct map) map)
{
	return 0;
}

struct map_ops rbtree_map_ops = {
	/* .check	= */ map_rbtree_check,
	/* .create	= */ map_rbtree_create,
	/* .destroy	= */ map_rbtree_destroy,
	/* .init	= */ map_rbtree_init,
	/* .insert	= */ map_rbtree_insert,
	/* .insert_new	= */ map_rbtree_insert_new,
	/* .remove	= */ map_rbtree_remove,
	/* .remove_free	= */ map_rbtree_remove_free,
	/* .clear	= */ map_rbtree_clear,
	/* .get		= */ map_rbtree_get,
	/* .lookup	= */ map_rbtree_lookup,
	/* .foreach	= */ map_rbtree_foreach,
	/* .is_empty	= */ map_rbtree_is_empty,
	/* .count	= */ NULL,
	/* .cmd		= */ NULL,
};
