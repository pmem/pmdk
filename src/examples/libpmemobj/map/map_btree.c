/*
 * Copyright 2015-2017, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

struct map_ops btree_map_ops = {
	/* .check	= */ map_btree_check,
	/* .create	= */ map_btree_create,
	/* .destroy	= */ map_btree_destroy,
	/* .init	= */ NULL,
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
