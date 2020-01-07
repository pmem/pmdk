// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

/*
 * map_hashmap_tx.c -- common interface for maps
 */

#include <map.h>
#include <hashmap_tx.h>

#include "map_hashmap_tx.h"

/*
 * map_hm_tx_check -- wrapper for hm_tx_check
 */
static int
map_hm_tx_check(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct hashmap_tx) hashmap_tx;
	TOID_ASSIGN(hashmap_tx, map.oid);

	return hm_tx_check(pop, hashmap_tx);
}

/*
 * map_hm_tx_count -- wrapper for hm_tx_count
 */
static size_t
map_hm_tx_count(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct hashmap_tx) hashmap_tx;
	TOID_ASSIGN(hashmap_tx, map.oid);

	return hm_tx_count(pop, hashmap_tx);
}

/*
 * map_hm_tx_init -- wrapper for hm_tx_init
 */
static int
map_hm_tx_init(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct hashmap_tx) hashmap_tx;
	TOID_ASSIGN(hashmap_tx, map.oid);

	return hm_tx_init(pop, hashmap_tx);
}

/*
 * map_hm_tx_create -- wrapper for hm_tx_create
 */
static int
map_hm_tx_create(PMEMobjpool *pop, TOID(struct map) *map, void *arg)
{
	TOID(struct hashmap_tx) *hashmap_tx =
		(TOID(struct hashmap_tx) *)map;

	return hm_tx_create(pop, hashmap_tx, arg);
}

/*
 * map_hm_tx_insert -- wrapper for hm_tx_insert
 */
static int
map_hm_tx_insert(PMEMobjpool *pop, TOID(struct map) map,
		uint64_t key, PMEMoid value)
{
	TOID(struct hashmap_tx) hashmap_tx;
	TOID_ASSIGN(hashmap_tx, map.oid);

	return hm_tx_insert(pop, hashmap_tx, key, value);
}

/*
 * map_hm_tx_remove -- wrapper for hm_tx_remove
 */
static PMEMoid
map_hm_tx_remove(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct hashmap_tx) hashmap_tx;
	TOID_ASSIGN(hashmap_tx, map.oid);

	return hm_tx_remove(pop, hashmap_tx, key);
}

/*
 * map_hm_tx_get -- wrapper for hm_tx_get
 */
static PMEMoid
map_hm_tx_get(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct hashmap_tx) hashmap_tx;
	TOID_ASSIGN(hashmap_tx, map.oid);

	return hm_tx_get(pop, hashmap_tx, key);
}

/*
 * map_hm_tx_lookup -- wrapper for hm_tx_lookup
 */
static int
map_hm_tx_lookup(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct hashmap_tx) hashmap_tx;
	TOID_ASSIGN(hashmap_tx, map.oid);

	return hm_tx_lookup(pop, hashmap_tx, key);
}

/*
 * map_hm_tx_foreach -- wrapper for hm_tx_foreach
 */
static int
map_hm_tx_foreach(PMEMobjpool *pop, TOID(struct map) map,
		int (*cb)(uint64_t key, PMEMoid value, void *arg),
		void *arg)
{
	TOID(struct hashmap_tx) hashmap_tx;
	TOID_ASSIGN(hashmap_tx, map.oid);

	return hm_tx_foreach(pop, hashmap_tx, cb, arg);
}

/*
 * map_hm_tx_cmd -- wrapper for hm_tx_cmd
 */
static int
map_hm_tx_cmd(PMEMobjpool *pop, TOID(struct map) map,
		unsigned cmd, uint64_t arg)
{
	TOID(struct hashmap_tx) hashmap_tx;
	TOID_ASSIGN(hashmap_tx, map.oid);

	return hm_tx_cmd(pop, hashmap_tx, cmd, arg);
}

struct map_ops hashmap_tx_ops = {
	/* .check	= */ map_hm_tx_check,
	/* .create	= */ map_hm_tx_create,
	/* .delete	= */ NULL,
	/* .init	= */ map_hm_tx_init,
	/* .insert	= */ map_hm_tx_insert,
	/* .insert_new	= */ NULL,
	/* .remove	= */ map_hm_tx_remove,
	/* .remove_free	= */ NULL,
	/* .clear	= */ NULL,
	/* .get		= */ map_hm_tx_get,
	/* .lookup	= */ map_hm_tx_lookup,
	/* .foreach	= */ map_hm_tx_foreach,
	/* .is_empty	= */ NULL,
	/* .count	= */ map_hm_tx_count,
	/* .cmd		= */ map_hm_tx_cmd,
};
