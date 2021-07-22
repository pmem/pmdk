// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2021, Intel Corporation */

/*
 * map_skiplist.c -- common interface for maps
 */

#include <map.h>
#include <skiplist_map.h>

#include "map_skiplist.h"

/*
 * map_skiplist_check -- wrapper for skiplist_map_check
 */
static int
map_skiplist_check(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct skiplist_map_node) skiplist_map;
	TOID_ASSIGN(skiplist_map, map.oid);

	return skiplist_map_check(pop, skiplist_map);
}

/*
 * map_skiplist_create -- wrapper for skiplist_map_new
 */
static int
map_skiplist_create(PMEMobjpool *pop, TOID(struct map) *map, void *arg)
{
	TOID(struct skiplist_map_node) *skiplist_map =
		(TOID(struct skiplist_map_node) *)map;

	return skiplist_map_create(pop, skiplist_map, arg);
}

/*
 * map_skiplist_destroy -- wrapper for skiplist_map_delete
 */
static int
map_skiplist_destroy(PMEMobjpool *pop, TOID(struct map) *map)
{
	TOID(struct skiplist_map_node) *skiplist_map =
		(TOID(struct skiplist_map_node) *)map;

	return skiplist_map_destroy(pop, skiplist_map);
}

/*
 * map_skiplist_insert -- wrapper for skiplist_map_insert
 */
static int
map_skiplist_insert(PMEMobjpool *pop, TOID(struct map) map,
		uint64_t key, PMEMoid value)
{
	TOID(struct skiplist_map_node) skiplist_map;
	TOID_ASSIGN(skiplist_map, map.oid);

	return skiplist_map_insert(pop, skiplist_map, key, value);
}

/*
 * map_skiplist_insert_new -- wrapper for skiplist_map_insert_new
 */
static int
map_skiplist_insert_new(PMEMobjpool *pop, TOID(struct map) map,
		uint64_t key, size_t size,
		unsigned type_num,
		void (*constructor)(PMEMobjpool *pop, void *ptr, void *arg),
		void *arg)
{
	TOID(struct skiplist_map_node) skiplist_map;
	TOID_ASSIGN(skiplist_map, map.oid);

	return skiplist_map_insert_new(pop, skiplist_map, key, size,
			type_num, constructor, arg);
}

/*
 * map_skiplist_remove -- wrapper for skiplist_map_remove
 */
static PMEMoid
map_skiplist_remove(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct skiplist_map_node) skiplist_map;
	TOID_ASSIGN(skiplist_map, map.oid);

	return skiplist_map_remove(pop, skiplist_map, key);
}

/*
 * map_skiplist_remove_free -- wrapper for skiplist_map_remove_free
 */
static int
map_skiplist_remove_free(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct skiplist_map_node) skiplist_map;
	TOID_ASSIGN(skiplist_map, map.oid);

	return skiplist_map_remove_free(pop, skiplist_map, key);
}

/*
 * map_skiplist_clear -- wrapper for skiplist_map_clear
 */
static int
map_skiplist_clear(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct skiplist_map_node) skiplist_map;
	TOID_ASSIGN(skiplist_map, map.oid);

	return skiplist_map_clear(pop, skiplist_map);
}

/*
 * map_skiplist_get -- wrapper for skiplist_map_get
 */
static PMEMoid
map_skiplist_get(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct skiplist_map_node) skiplist_map;
	TOID_ASSIGN(skiplist_map, map.oid);

	return skiplist_map_get(pop, skiplist_map, key);
}

/*
 * map_skiplist_lookup -- wrapper for skiplist_map_lookup
 */
static int
map_skiplist_lookup(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct skiplist_map_node) skiplist_map;
	TOID_ASSIGN(skiplist_map, map.oid);

	return skiplist_map_lookup(pop, skiplist_map, key);
}

/*
 * map_skiplist_foreach -- wrapper for skiplist_map_foreach
 */
static int
map_skiplist_foreach(PMEMobjpool *pop, TOID(struct map) map,
		int (*cb)(uint64_t key, PMEMoid value, void *arg),
		void *arg)
{
	TOID(struct skiplist_map_node) skiplist_map;
	TOID_ASSIGN(skiplist_map, map.oid);

	return skiplist_map_foreach(pop, skiplist_map, cb, arg);
}

/*
 * map_skiplist_is_empty -- wrapper for skiplist_map_is_empty
 */
static int
map_skiplist_is_empty(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct skiplist_map_node) skiplist_map;
	TOID_ASSIGN(skiplist_map, map.oid);

	return skiplist_map_is_empty(pop, skiplist_map);
}

/*
 * map_skiplist_init -- recovers map state
 * Since there is no need for recovery for skiplist, function is dummy.
 */
static int
map_skiplist_init(PMEMobjpool *pop, TOID(struct map) map)
{
	return 0;
}

struct map_ops skiplist_map_ops = {
	/* .check	= */ map_skiplist_check,
	/* .create	= */ map_skiplist_create,
	/* .destroy	= */ map_skiplist_destroy,
	/* .init	= */ map_skiplist_init,
	/* .insert	= */ map_skiplist_insert,
	/* .insert_new	= */ map_skiplist_insert_new,
	/* .remove	= */ map_skiplist_remove,
	/* .remove_free	= */ map_skiplist_remove_free,
	/* .clear	= */ map_skiplist_clear,
	/* .get		= */ map_skiplist_get,
	/* .lookup	= */ map_skiplist_lookup,
	/* .foreach	= */ map_skiplist_foreach,
	/* .is_empty	= */ map_skiplist_is_empty,
	/* .count	= */ NULL,
	/* .cmd		= */ NULL,
};
