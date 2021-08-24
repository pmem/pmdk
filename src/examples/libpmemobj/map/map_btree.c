// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2021, Intel Corporation */

/*
 * map_btree.c -- common interface for maps
 */

#include <map.h>
#include <btree_map.h>

#include "map_btree.h"

/*
 * map_btree_check -- wrapper for btree_map_check
 */
static int
map_btree_check(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct btree_map) btree_map;
	TOID_ASSIGN(btree_map, map.oid);

	return btree_map_check(pop, btree_map);
}

/*
 * map_btree_create -- wrapper for btree_map_create
 */
static int
map_btree_create(PMEMobjpool *pop, TOID(struct map) *map, void *arg)
{
	TOID(struct btree_map) *btree_map =
		(TOID(struct btree_map) *)map;

	return btree_map_create(pop, btree_map, arg);
}

/*
 * map_btree_destroy -- wrapper for btree_map_destroy
 */
static int
map_btree_destroy(PMEMobjpool *pop, TOID(struct map) *map)
{
	TOID(struct btree_map) *btree_map =
		(TOID(struct btree_map) *)map;

	return btree_map_destroy(pop, btree_map);
}

/*
 * map_btree_insert -- wrapper for btree_map_insert
 */
static int
map_btree_insert(PMEMobjpool *pop, TOID(struct map) map,
		uint64_t key, PMEMoid value)
{
	TOID(struct btree_map) btree_map;
	TOID_ASSIGN(btree_map, map.oid);

	return btree_map_insert(pop, btree_map, key, value);
}

/*
 * map_btree_insert_new -- wrapper for btree_map_insert_new
 */
static int
map_btree_insert_new(PMEMobjpool *pop, TOID(struct map) map,
		uint64_t key, size_t size,
		unsigned type_num,
		void (*constructor)(PMEMobjpool *pop, void *ptr, void *arg),
		void *arg)
{
	TOID(struct btree_map) btree_map;
	TOID_ASSIGN(btree_map, map.oid);

	return btree_map_insert_new(pop, btree_map, key, size,
			type_num, constructor, arg);
}

/*
 * map_btree_remove -- wrapper for btree_map_remove
 */
static PMEMoid
map_btree_remove(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct btree_map) btree_map;
	TOID_ASSIGN(btree_map, map.oid);

	return btree_map_remove(pop, btree_map, key);
}

/*
 * map_btree_remove_free -- wrapper for btree_map_remove_free
 */
static int
map_btree_remove_free(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct btree_map) btree_map;
	TOID_ASSIGN(btree_map, map.oid);

	return btree_map_remove_free(pop, btree_map, key);
}

/*
 * map_btree_clear -- wrapper for btree_map_clear
 */
static int
map_btree_clear(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct btree_map) btree_map;
	TOID_ASSIGN(btree_map, map.oid);

	return btree_map_clear(pop, btree_map);
}

/*
 * map_btree_get -- wrapper for btree_map_get
 */
static PMEMoid
map_btree_get(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct btree_map) btree_map;
	TOID_ASSIGN(btree_map, map.oid);

	return btree_map_get(pop, btree_map, key);
}

/*
 * map_btree_lookup -- wrapper for btree_map_lookup
 */
static int
map_btree_lookup(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct btree_map) btree_map;
	TOID_ASSIGN(btree_map, map.oid);

	return btree_map_lookup(pop, btree_map, key);
}

/*
 * map_btree_foreach -- wrapper for btree_map_foreach
 */
static int
map_btree_foreach(PMEMobjpool *pop, TOID(struct map) map,
		int (*cb)(uint64_t key, PMEMoid value, void *arg),
		void *arg)
{
	TOID(struct btree_map) btree_map;
	TOID_ASSIGN(btree_map, map.oid);

	return btree_map_foreach(pop, btree_map, cb, arg);
}

/*
 * map_btree_is_empty -- wrapper for btree_map_is_empty
 */
static int
map_btree_is_empty(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct btree_map) btree_map;
	TOID_ASSIGN(btree_map, map.oid);

	return btree_map_is_empty(pop, btree_map);
}

/*
 * map_btree_init -- recovers map state
 * Since there is no need for recovery for btree, function is dummy.
 */
static int
map_btree_init(PMEMobjpool *pop, TOID(struct map) map)
{
	return 0;
}

struct map_ops btree_map_ops = {
	/* .check	= */ map_btree_check,
	/* .create	= */ map_btree_create,
	/* .destroy	= */ map_btree_destroy,
	/* .init	= */ map_btree_init,
	/* .insert	= */ map_btree_insert,
	/* .insert_new	= */ map_btree_insert_new,
	/* .remove	= */ map_btree_remove,
	/* .remove_free	= */ map_btree_remove_free,
	/* .clear	= */ map_btree_clear,
	/* .get		= */ map_btree_get,
	/* .lookup	= */ map_btree_lookup,
	/* .foreach	= */ map_btree_foreach,
	/* .is_empty	= */ map_btree_is_empty,
	/* .count	= */ NULL,
	/* .cmd		= */ NULL,
};
