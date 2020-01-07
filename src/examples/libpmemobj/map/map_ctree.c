// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

/*
 * map_ctree.c -- common interface for maps
 */

#include <map.h>
#include <ctree_map.h>

#include "map_ctree.h"

/*
 * map_ctree_check -- wrapper for ctree_map_check
 */
static int
map_ctree_check(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct ctree_map) ctree_map;
	TOID_ASSIGN(ctree_map, map.oid);

	return ctree_map_check(pop, ctree_map);
}

/*
 * map_ctree_create -- wrapper for ctree_map_create
 */
static int
map_ctree_create(PMEMobjpool *pop, TOID(struct map) *map, void *arg)
{
	TOID(struct ctree_map) *ctree_map =
		(TOID(struct ctree_map) *)map;

	return ctree_map_create(pop, ctree_map, arg);
}

/*
 * map_ctree_destroy -- wrapper for ctree_map_destroy
 */
static int
map_ctree_destroy(PMEMobjpool *pop, TOID(struct map) *map)
{
	TOID(struct ctree_map) *ctree_map =
		(TOID(struct ctree_map) *)map;

	return ctree_map_destroy(pop, ctree_map);
}

/*
 * map_ctree_insert -- wrapper for ctree_map_insert
 */
static int
map_ctree_insert(PMEMobjpool *pop, TOID(struct map) map,
		uint64_t key, PMEMoid value)
{
	TOID(struct ctree_map) ctree_map;
	TOID_ASSIGN(ctree_map, map.oid);

	return ctree_map_insert(pop, ctree_map, key, value);
}

/*
 * map_ctree_insert_new -- wrapper for ctree_map_insert_new
 */
static int
map_ctree_insert_new(PMEMobjpool *pop, TOID(struct map) map,
		uint64_t key, size_t size,
		unsigned type_num,
		void (*constructor)(PMEMobjpool *pop, void *ptr, void *arg),
		void *arg)
{
	TOID(struct ctree_map) ctree_map;
	TOID_ASSIGN(ctree_map, map.oid);

	return ctree_map_insert_new(pop, ctree_map, key, size,
			type_num, constructor, arg);
}

/*
 * map_ctree_remove -- wrapper for ctree_map_remove
 */
static PMEMoid
map_ctree_remove(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct ctree_map) ctree_map;
	TOID_ASSIGN(ctree_map, map.oid);

	return ctree_map_remove(pop, ctree_map, key);
}

/*
 * map_ctree_remove_free -- wrapper for ctree_map_remove_free
 */
static int
map_ctree_remove_free(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct ctree_map) ctree_map;
	TOID_ASSIGN(ctree_map, map.oid);

	return ctree_map_remove_free(pop, ctree_map, key);
}

/*
 * map_ctree_clear -- wrapper for ctree_map_clear
 */
static int
map_ctree_clear(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct ctree_map) ctree_map;
	TOID_ASSIGN(ctree_map, map.oid);

	return ctree_map_clear(pop, ctree_map);
}

/*
 * map_ctree_get -- wrapper for ctree_map_get
 */
static PMEMoid
map_ctree_get(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct ctree_map) ctree_map;
	TOID_ASSIGN(ctree_map, map.oid);

	return ctree_map_get(pop, ctree_map, key);
}

/*
 * map_ctree_lookup -- wrapper for ctree_map_lookup
 */
static int
map_ctree_lookup(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct ctree_map) ctree_map;
	TOID_ASSIGN(ctree_map, map.oid);

	return ctree_map_lookup(pop, ctree_map, key);
}

/*
 * map_ctree_foreach -- wrapper for ctree_map_foreach
 */
static int
map_ctree_foreach(PMEMobjpool *pop, TOID(struct map) map,
		int (*cb)(uint64_t key, PMEMoid value, void *arg),
		void *arg)
{
	TOID(struct ctree_map) ctree_map;
	TOID_ASSIGN(ctree_map, map.oid);

	return ctree_map_foreach(pop, ctree_map, cb, arg);
}

/*
 * map_ctree_is_empty -- wrapper for ctree_map_is_empty
 */
static int
map_ctree_is_empty(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct ctree_map) ctree_map;
	TOID_ASSIGN(ctree_map, map.oid);

	return ctree_map_is_empty(pop, ctree_map);
}

struct map_ops ctree_map_ops = {
	/* .check	= */ map_ctree_check,
	/* .create	= */ map_ctree_create,
	/* .destroy	= */ map_ctree_destroy,
	/* .init	= */ NULL,
	/* .insert	= */ map_ctree_insert,
	/* .insert_new	= */ map_ctree_insert_new,
	/* .remove	= */ map_ctree_remove,
	/* .remove_free	= */ map_ctree_remove_free,
	/* .clear	= */ map_ctree_clear,
	/* .get		= */ map_ctree_get,
	/* .lookup	= */ map_ctree_lookup,
	/* .foreach	= */ map_ctree_foreach,
	/* .is_empty	= */ map_ctree_is_empty,
	/* .count	= */ NULL,
	/* .cmd		= */ NULL,
};
