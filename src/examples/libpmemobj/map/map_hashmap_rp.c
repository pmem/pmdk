/*
 * Copyright 2018, Intel Corporation
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
 * map_hashmap_rp.c -- common interface for maps
 */

#include <map.h>
#include <hashmap_rp.h>

#include "map_hashmap_rp.h"

/*
 * map_hm_rp_check -- wrapper for hm_rp_check
 */
static int
map_hm_rp_check(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct hashmap_rp) hashmap_rp;
	TOID_ASSIGN(hashmap_rp, map.oid);

	return hm_rp_check(pop, hashmap_rp);
}

/*
 * map_hm_rp_count -- wrapper for hm_rp_count
 */
static size_t
map_hm_rp_count(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct hashmap_rp) hashmap_rp;
	TOID_ASSIGN(hashmap_rp, map.oid);

	return hm_rp_count(pop, hashmap_rp);
}

/*
 * map_hm_rp_init -- wrapper for hm_rp_init
 */
static int
map_hm_rp_init(PMEMobjpool *pop, TOID(struct map) map)
{
	TOID(struct hashmap_rp) hashmap_rp;
	TOID_ASSIGN(hashmap_rp, map.oid);

	return hm_rp_init(pop, hashmap_rp);
}

/*
 * map_hm_rp_create -- wrapper for hm_rp_create
 */
static int
map_hm_rp_create(PMEMobjpool *pop, TOID(struct map) *map, void *arg)
{
	TOID(struct hashmap_rp) *hashmap_rp =
		(TOID(struct hashmap_rp) *)map;

	return hm_rp_create(pop, hashmap_rp, arg);
}

/*
 * map_hm_rp_insert -- wrapper for hm_rp_insert
 */
static int
map_hm_rp_insert(PMEMobjpool *pop, TOID(struct map) map,
		uint64_t key, PMEMoid value)
{
	TOID(struct hashmap_rp) hashmap_rp;
	TOID_ASSIGN(hashmap_rp, map.oid);

	return hm_rp_insert(pop, hashmap_rp, key, value);
}

/*
 * map_hm_rp_remove -- wrapper for hm_rp_remove
 */
static PMEMoid
map_hm_rp_remove(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct hashmap_rp) hashmap_rp;
	TOID_ASSIGN(hashmap_rp, map.oid);

	return hm_rp_remove(pop, hashmap_rp, key);
}

/*
 * map_hm_rp_get -- wrapper for hm_rp_get
 */
static PMEMoid
map_hm_rp_get(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct hashmap_rp) hashmap_rp;
	TOID_ASSIGN(hashmap_rp, map.oid);

	return hm_rp_get(pop, hashmap_rp, key);
}

/*
 * map_hm_rp_lookup -- wrapper for hm_rp_lookup
 */
static int
map_hm_rp_lookup(PMEMobjpool *pop, TOID(struct map) map, uint64_t key)
{
	TOID(struct hashmap_rp) hashmap_rp;
	TOID_ASSIGN(hashmap_rp, map.oid);

	return hm_rp_lookup(pop, hashmap_rp, key);
}

/*
 * map_hm_rp_foreach -- wrapper for hm_rp_foreach
 */
static int
map_hm_rp_foreach(PMEMobjpool *pop, TOID(struct map) map,
		int (*cb)(uint64_t key, PMEMoid value, void *arg),
		void *arg)
{
	TOID(struct hashmap_rp) hashmap_rp;
	TOID_ASSIGN(hashmap_rp, map.oid);

	return hm_rp_foreach(pop, hashmap_rp, cb, arg);
}

/*
 * map_hm_rp_cmd -- wrapper for hm_rp_cmd
 */
static int
map_hm_rp_cmd(PMEMobjpool *pop, TOID(struct map) map,
		unsigned cmd, uint64_t arg)
{
	TOID(struct hashmap_rp) hashmap_rp;
	TOID_ASSIGN(hashmap_rp, map.oid);

	return hm_rp_cmd(pop, hashmap_rp, cmd, arg);
}

struct map_ops hashmap_rp_ops = {
	/* .check	= */ map_hm_rp_check,
	/* .create	= */ map_hm_rp_create,
	/* .destroy	= */ NULL,
	/* .init	= */ map_hm_rp_init,
	/* .insert	= */ map_hm_rp_insert,
	/* .insert_new	= */ NULL,
	/* .remove	= */ map_hm_rp_remove,
	/* .remove_free	= */ NULL,
	/* .clear	= */ NULL,
	/* .get		= */ map_hm_rp_get,
	/* .lookup	= */ map_hm_rp_lookup,
	/* .foreach	= */ map_hm_rp_foreach,
	/* .is_empty	= */ NULL,
	/* .count	= */ map_hm_rp_count,
	/* .cmd		= */ map_hm_rp_cmd,
};
