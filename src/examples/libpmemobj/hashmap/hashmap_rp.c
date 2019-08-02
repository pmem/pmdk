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
 * Integer hash set implementation with open addressing Robin Hood collision
 * resolution which uses action.h reserve/publish API.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <inttypes.h>

#include <libpmemobj.h>
#include "hashmap_rp.h"

#define TOMBSTONE_MASK (1ULL << 63)

#ifdef DEBUG
#define HM_ASSERT(cnd) assert(cnd)
#else
#define HM_ASSERT(cnd)
#endif

/* layout definition */
TOID_DECLARE(struct entry, HASHMAP_RP_TYPE_OFFSET + 1);

struct entry {
	uint64_t key;
	PMEMoid value;

	uint64_t hash;
};

struct add_entry {
	struct entry data;

	/* position index in hashmap, where data should be inserted/updated */
	size_t pos;

	/* Action array to perform addition in set of actions */
	struct pobj_action *actv;
	/* Action array index counter */
	size_t actv_cnt;

#ifdef DEBUG
	/* Swaps counter for current insertion. Enabled in debug mode */
	int swaps;
#endif
};

struct hashmap_rp {
	/* number of values inserted */
	uint64_t count;

	/* container capacity */
	uint64_t capacity;

	/* resize threshold */
	uint64_t resize_threshold;

	/* entries */
	TOID(struct entry) entries;
};

int *swaps_array = NULL;

#ifdef DEBUG
static inline int
is_power_of_2(uint64_t v)
{
	return v && !(v & (v - 1));
}
#endif

/*
 * entry_is_deleted -- checks 'tombstone' bit if hash is deleted
 */
static inline int
entry_is_deleted(uint64_t hash)
{
	return (hash & TOMBSTONE_MASK) > 0;
}

/*
 * entry_is_empty -- checks if entry is empty
 */
static inline int
entry_is_empty(uint64_t hash)
{
	return hash == 0 || entry_is_deleted(hash);
}

/*
 * increment_pos -- increment position index, skip 0
 */
static uint64_t
increment_pos(const struct hashmap_rp *hashmap, uint64_t pos)
{
	HM_ASSERT(is_power_of_2(hashmap->capacity));
	pos = (pos + 1) & (hashmap->capacity - 1);
	return pos == 0 ? 1 : pos;
}

/*
 * probe_distance -- returns probe number, an indicator how far from
 * desired position given hash is stored in hashmap
 */
static int
probe_distance(const struct hashmap_rp *hashmap, uint64_t hash_key,
	uint64_t slot_index)
{
	uint64_t capacity = hashmap->capacity;

	HM_ASSERT(is_power_of_2(hashmap->capacity));
	return (int)(slot_index + capacity - hash_key) & (capacity - 1);
}

/*
 * hash -- hash function based on Austin Appleby MurmurHash3 64-bit finalizer.
 * Returned value is modified to work with special values for unused and
 * and deleted hashes.
 */
static uint64_t
hash(const struct hashmap_rp *hashmap, uint64_t key)
{
	key ^= key >> 33;
	key *= 0xff51afd7ed558ccd;
	key ^= key >> 33;
	key *= 0xc4ceb9fe1a85ec53;
	key ^= key >> 33;
	HM_ASSERT(is_power_of_2(hashmap->capacity));
	key &= hashmap->capacity - 1;

	/* first, 'tombstone' bit is used to indicate deleted item */
	key &= ~TOMBSTONE_MASK;

	/*
	 * Ensure that we never return 0 as a hash, since we use 0 to
	 * indicate that element has never been used at all.
	 */
	return key == 0 ? 1 : key;
}

/*
 * hashmap_create -- hashmap initializer
 */
static void
hashmap_create(PMEMobjpool *pop, TOID(struct hashmap_rp) *hashmap_p,
		uint32_t seed)
{
	struct pobj_action actv[4];
	size_t actv_cnt = 0;

	TOID(struct hashmap_rp) hashmap =
		POBJ_RESERVE_NEW(pop, struct hashmap_rp, &actv[actv_cnt]);
	if (TOID_IS_NULL(hashmap))
		goto reserve_err;
	actv_cnt++;

	D_RW(hashmap)->count = 0;
	D_RW(hashmap)->capacity = INIT_ENTRIES_NUM_RP;
	D_RW(hashmap)->resize_threshold = (uint64_t)(INIT_ENTRIES_NUM_RP *
		HASHMAP_RP_LOAD_FACTOR);

	size_t sz = sizeof(struct entry) * D_RO(hashmap)->capacity;
	/* init entries with zero in order to track unused hashes */
	D_RW(hashmap)->entries = POBJ_XRESERVE_ALLOC(pop, struct entry, sz,
			&actv[actv_cnt], POBJ_XALLOC_ZERO);
	if (TOID_IS_NULL(D_RO(hashmap)->entries))
		goto reserve_err;
	actv_cnt++;

	pmemobj_persist(pop, D_RW(hashmap), sizeof(struct hashmap_rp));

	pmemobj_set_value(pop, &actv[actv_cnt++], &hashmap_p->oid.pool_uuid_lo,
		hashmap.oid.pool_uuid_lo);
	pmemobj_set_value(pop, &actv[actv_cnt++], &hashmap_p->oid.off,
		hashmap.oid.off);

	pmemobj_publish(pop, actv, actv_cnt);

#ifdef DEBUG
	swaps_array = (int *)calloc(INIT_ENTRIES_NUM_RP, sizeof(int));
	if (!swaps_array)
		abort();
#endif

	return;

reserve_err:
	fprintf(stderr, "hashmap alloc failed: %s\n", pmemobj_errormsg());
	pmemobj_cancel(pop, actv, actv_cnt);
	abort();
}

/*
 * entry_update -- updates entry in given hashmap with given arguments
 */
static void
entry_update(PMEMobjpool *pop, struct hashmap_rp *hashmap,
	struct add_entry *args, int rebuild)
{
	HM_ASSERT(HASHMAP_RP_MAX_ACTIONS > args->actv_cnt + 4);

	struct entry *entry_p = D_RW(hashmap->entries);
	entry_p += args->pos;

	if (rebuild == HASHMAP_RP_REBUILD) {
		entry_p->key = args->data.key;
		entry_p->value = args->data.value;
		entry_p->hash = args->data.hash;
	} else {
		pmemobj_set_value(pop, args->actv + args->actv_cnt++,
			&entry_p->key, args->data.key);
		pmemobj_set_value(pop, args->actv + args->actv_cnt++,
			&entry_p->value.pool_uuid_lo,
			args->data.value.pool_uuid_lo);
		pmemobj_set_value(pop, args->actv + args->actv_cnt++,
			&entry_p->value.off, args->data.value.off);
		pmemobj_set_value(pop, args->actv + args->actv_cnt++,
			&entry_p->hash, args->data.hash);
	}
#ifdef DEBUG
	assert(sizeof(swaps_array) / sizeof(swaps_array[0])
		> args->pos);
	swaps_array[args->pos] = args->swaps;
#endif
}

/*
 * entry_add -- increments given hashmap's elements counter and calls
 * entry_update
 */
static void
entry_add(PMEMobjpool *pop, struct hashmap_rp *hashmap, struct add_entry *args,
	int rebuild)
{
	HM_ASSERT(HASHMAP_RP_MAX_ACTIONS > args->actv_cnt + 1);

	if (rebuild == HASHMAP_RP_REBUILD)
		hashmap->count++;
	else {
		pmemobj_set_value(pop, args->actv + args->actv_cnt++,
			&hashmap->count, hashmap->count + 1);
	}

	entry_update(pop, hashmap, args, rebuild);
}

/*
 * insert_helper -- inserts specified value into the hashmap
 * If function was called during rebuild process, no redo logs will be used.
 * returns:
 * - 0 if successful,
 * - 1 if value already existed
 * - -1 on error
 */
static int
insert_helper(PMEMobjpool *pop, struct hashmap_rp *hashmap, uint64_t key,
	PMEMoid value, int rebuild)
{
	HM_ASSERT(hashmap->count + 1 < hashmap->resize_threshold);

	struct pobj_action actv[HASHMAP_RP_MAX_ACTIONS];

	struct add_entry args;
	args.data.key = key;
	args.data.value = value;
	args.data.hash = hash(hashmap, key);
	args.pos = args.data.hash;
	if (rebuild != HASHMAP_RP_REBUILD) {
		args.actv = actv;
		args.actv_cnt = 0;
	}

	int dist = 0;
	struct entry *entry_p = NULL;
#ifdef DEBUG
	int swaps = 0;
#endif

	for (int n = 0; n < HASHMAP_RP_MAX_SWAPS; ++n) {
		entry_p = D_RW(hashmap->entries);
		entry_p += args.pos;

#ifdef DEBUG
		args.swaps = swaps;
#endif
		/* Case 1: key already exists, override value */
		if (!entry_is_empty(entry_p->hash) &&
				entry_p->key == args.data.key) {
			entry_update(pop, hashmap, &args, rebuild);
			if (rebuild != HASHMAP_RP_REBUILD)
				pmemobj_publish(pop, args.actv, args.actv_cnt);

			return 1;
		}

		/* Case 2: slot is empty from the beginning */
		if (entry_p->hash == 0) {
			entry_add(pop, hashmap, &args, rebuild);
			if (rebuild != HASHMAP_RP_REBUILD)
				pmemobj_publish(pop, args.actv, args.actv_cnt);

			return 0;
		}

		/*
		 * Case 3: existing element (or tombstone) has probed less than
		 * current element. Swap them (or put into tombstone slot) and
		 * keep going to find another slot for that element.
		 */
		int existing_dist = probe_distance(hashmap, entry_p->hash,
							args.pos);
		if (existing_dist < dist) {
			if (entry_is_deleted(entry_p->hash)) {
				entry_add(pop, hashmap, &args, rebuild);
				if (rebuild != HASHMAP_RP_REBUILD)
					pmemobj_publish(pop, args.actv,
						args.actv_cnt);

				return 0;
			}

			struct entry temp = *entry_p;
			entry_update(pop, hashmap, &args, rebuild);
			args.data = temp;

#ifdef DEBUG
			swaps++;
#endif
			dist = existing_dist;
		}

		/*
		 * Case 4: increment slot number and probe counter, keep going
		 * to find free slot
		 */
		args.pos = increment_pos(hashmap, args.pos);
		dist += 1;
	}
	fprintf(stderr, "insertion requires too many swaps\n");
	if (rebuild != HASHMAP_RP_REBUILD)
		pmemobj_cancel(pop, args.actv, args.actv_cnt);

	return -1;
}

/*
 * index_lookup -- checks if given key exists in hashmap.
 * Returns index number if key was found, 0 otherwise.
 */
static uint64_t
index_lookup(const struct hashmap_rp *hashmap, uint64_t key)
{
	const uint64_t hash_lookup = hash(hashmap, key);
	uint64_t pos = hash_lookup;
	uint64_t dist = 0;

	const struct entry *entry_p = NULL;
	do {
		entry_p = D_RO(hashmap->entries);
		entry_p += pos;

		if (entry_p->hash == hash_lookup && entry_p->key == key)
			return pos;

		pos = increment_pos(hashmap, pos);

	} while (entry_p->hash != 0 &&
		(dist++) <= probe_distance(hashmap, entry_p->hash, pos) - 1);

	return 0;
}

/*
 * entries_cache -- cache entries from second argument in entries from first
 * argument
 */
static int
entries_cache(PMEMobjpool *pop, struct hashmap_rp *dest,
	const struct hashmap_rp *src)
{
	const struct entry *e_begin = D_RO(src->entries);
	const struct entry *e_end = e_begin + src->capacity;

	for (const struct entry *e = e_begin; e != e_end; ++e) {
		if (entry_is_empty(e->hash))
			continue;

		if (insert_helper(pop, dest, e->key,
				e->value, HASHMAP_RP_REBUILD) == -1)
			return -1;
	}
	HM_ASSERT(src->count == dest->count);

	return 0;
}

/*
 * hm_rp_rebuild -- rebuilds the hashmap with a new capacity.
 * Returns 0 on success, -1 otherwise.
 */
static int
hm_rp_rebuild(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap,
		size_t capacity_new)
{
	/*
	 * We will need 6 actions:
	 * - 1 action to set new capacity
	 * - 1 action to set new resize threshold
	 * - 1 action to alloc memory for new entries
	 * - 1 action to free old entries
	 * - 2 actions to set new oid pointing to new entries
	 */
	struct pobj_action actv[6];
	size_t actv_cnt = 0;

	size_t sz_alloc = sizeof(struct entry) * capacity_new;
	uint64_t resize_threshold_new = (uint64_t)(capacity_new *
		HASHMAP_RP_LOAD_FACTOR);

	pmemobj_set_value(pop, &actv[actv_cnt++], &D_RW(hashmap)->capacity,
		capacity_new);

	pmemobj_set_value(pop, &actv[actv_cnt++],
		&D_RW(hashmap)->resize_threshold, resize_threshold_new);

	struct hashmap_rp hashmap_rebuild;
	hashmap_rebuild.count = 0;
	hashmap_rebuild.capacity = capacity_new;
	hashmap_rebuild.resize_threshold = resize_threshold_new;
	hashmap_rebuild.entries = POBJ_XRESERVE_ALLOC(pop, struct entry,
						sz_alloc, &actv[actv_cnt],
						POBJ_XALLOC_ZERO);

	if (TOID_IS_NULL(hashmap_rebuild.entries)) {
		fprintf(stderr, "hashmap rebuild failed: %s\n",
			pmemobj_errormsg());
		goto rebuild_err;
	}
	actv_cnt++;

#ifdef DEBUG
	free(swaps_array);
	swaps_array = (int *)calloc(capacity_new, sizeof(int));
	if (!swaps_array)
		goto rebuild_err;
#endif

	if (entries_cache(pop, &hashmap_rebuild, D_RW(hashmap)) == -1)
		goto rebuild_err;

	pmemobj_persist(pop, D_RW(hashmap_rebuild.entries), sz_alloc);

	pmemobj_defer_free(pop, D_RW(hashmap)->entries.oid, &actv[actv_cnt++]);

	pmemobj_set_value(pop, &actv[actv_cnt++],
		&D_RW(hashmap)->entries.oid.pool_uuid_lo,
		hashmap_rebuild.entries.oid.pool_uuid_lo);
	pmemobj_set_value(pop, &actv[actv_cnt++],
		&D_RW(hashmap)->entries.oid.off,
		hashmap_rebuild.entries.oid.off);

	HM_ASSERT(sizeof(actv) / sizeof(actv[0]) >= actv_cnt);
	pmemobj_publish(pop, actv, actv_cnt);

	return 0;

rebuild_err:
	pmemobj_cancel(pop, actv, actv_cnt);

#ifdef DEBUG
	free(swaps_array);
#endif

	return -1;
}

/*
 * hm_rp_create --  initializes hashmap state, called after pmemobj_create
 */
int
hm_rp_create(PMEMobjpool *pop, TOID(struct hashmap_rp) *map, void *arg)
{
	struct hashmap_args *args = (struct hashmap_args *)arg;
	uint32_t seed = args ? args->seed : 0;

	hashmap_create(pop, map, seed);

	return 0;
}

/*
 * hm_rp_check -- checks if specified persistent object is an instance of
 * hashmap
 */
int
hm_rp_check(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap)
{
	return TOID_IS_NULL(hashmap) || !TOID_VALID(hashmap);
}

/*
 * hm_rp_init -- recovers hashmap state, called after pmemobj_open.
 * Since hashmap_rp is performing rebuild/insertion completely or not at all,
 * function is dummy and simply returns 0.
 */
int
hm_rp_init(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap)
{
	return 0;
}

/*
 * hm_rp_insert -- rebuilds hashmap if necessary and wraps insert_helper.
 * returns:
 * - 0 if successful,
 * - 1 if value already existed
 * - -1 if something bad happened
 */
int
hm_rp_insert(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap,
		uint64_t key, PMEMoid value)
{
	if (D_RO(hashmap)->count + 1 >= D_RO(hashmap)->resize_threshold) {
		uint64_t capacity_new = D_RO(hashmap)->capacity * 2;
		if (hm_rp_rebuild(pop, hashmap, capacity_new) != 0)
			return -1;
	}

	return insert_helper(pop, D_RW(hashmap), key, value,
		HASHMAP_RP_NO_REBUILD);
}

/*
 * hm_rp_remove -- removes specified key from the hashmap,
 * returns:
 * - key's value if successful,
 * - OID_NULL if value didn't exist or if something bad happened
 */
PMEMoid
hm_rp_remove(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap,
		uint64_t key)
{
	const uint64_t pos = index_lookup(D_RO(hashmap), key);

	if (pos == 0)
		return OID_NULL;

	struct entry *entry_p = D_RW(D_RW(hashmap)->entries);
	entry_p += pos;
	PMEMoid ret = entry_p->value;

	size_t actvcnt = 0;

	struct pobj_action actv[5];

	pmemobj_set_value(pop, &actv[actvcnt++], &entry_p->hash,
		entry_p->hash | TOMBSTONE_MASK);
	pmemobj_set_value(pop, &actv[actvcnt++],
		&entry_p->value.pool_uuid_lo, 0);
	pmemobj_set_value(pop, &actv[actvcnt++], &entry_p->value.off, 0);
	pmemobj_set_value(pop, &actv[actvcnt++], &entry_p->key, 0);
	pmemobj_set_value(pop, &actv[actvcnt++], &D_RW(hashmap)->count,
		D_RW(hashmap)->count - 1);

	HM_ASSERT(sizeof(actv) / sizeof(actv[0]) >= actvcnt);
	pmemobj_publish(pop, actv, actvcnt);

	uint64_t reduced_threshold = (uint64_t)
		(((uint64_t)(D_RO(hashmap)->capacity / 2))
			* HASHMAP_RP_LOAD_FACTOR);

	if (reduced_threshold >= INIT_ENTRIES_NUM_RP &&
		D_RW(hashmap)->count < reduced_threshold &&
		hm_rp_rebuild(pop, hashmap, D_RO(hashmap)->capacity / 2))
		return OID_NULL;

	return ret;
}

/*
 * hm_rp_get -- checks whether specified key is in the hashmap.
 * Returns associated value if key exists, OID_NULL otherwise.
 */
PMEMoid
hm_rp_get(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap,
		uint64_t key)
{
	struct entry *entry_p =
		(struct entry *)pmemobj_direct(D_RW(hashmap)->entries.oid);

	uint64_t pos = index_lookup(D_RO(hashmap), key);
	return pos == 0 ? OID_NULL : (entry_p + pos)->value;
}

/*
 * hm_rp_lookup -- checks whether specified key is in the hashmap.
 * Returns 1 if key was found, 0 otherwise.
 */
int
hm_rp_lookup(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap,
		uint64_t key)
{
	return index_lookup(D_RO(hashmap), key) != 0;
}

/*
 * hm_rp_foreach -- calls cb for all values from the hashmap
 */
int
hm_rp_foreach(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap,
	int (*cb)(uint64_t key, PMEMoid value, void *arg), void *arg)
{
	struct entry *entry_p =
		(struct entry *)pmemobj_direct(D_RO(hashmap)->entries.oid);

	int ret = 0;
	for (size_t i = 0; i < D_RO(hashmap)->capacity; ++i, ++entry_p) {
		uint64_t hash = entry_p->hash;
		if (entry_is_empty(hash))
			continue;
		ret = cb(entry_p->key, entry_p->value, arg);
		if (ret)
			return ret;
	}

	return 0;
}

/*
 * hm_rp_debug -- prints complete hashmap state
 */
static void
hm_rp_debug(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap, FILE *out)
{
#ifdef DEBUG
	fprintf(out, "debug: true, ");
#endif
	fprintf(out, "capacity: %" PRIu64 ", count: %" PRIu64 "\n",
		D_RO(hashmap)->capacity, D_RO(hashmap)->count);

	struct entry *entry_p = D_RW((D_RW(hashmap)->entries));
	for (size_t i = 0; i < D_RO(hashmap)->capacity; ++i, ++entry_p) {
		uint64_t hash = entry_p->hash;
		if (entry_is_empty(hash))
			continue;

		uint64_t key = entry_p->key;
#ifdef DEBUG
		fprintf(out, "%zu: %" PRIu64 " hash: %" PRIu64 " dist:%" PRIu32
			" swaps:%u\n", i, key, hash,
			probe_distance(D_RO(hashmap), hash, i),
			swaps_array[i]);
#else
		fprintf(out, "%zu: %" PRIu64 " dist:%u \n", i, key,
			probe_distance(D_RO(hashmap), hash, i));
#endif
	}
}

/*
 * hm_rp_count -- returns number of elements
 */
size_t
hm_rp_count(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap)
{
	return D_RO(hashmap)->count;
}

/*
 * hm_rp_cmd -- execute cmd for hashmap
 */
int
hm_rp_cmd(PMEMobjpool *pop, TOID(struct hashmap_rp) hashmap,
		unsigned cmd, uint64_t arg)
{
	switch (cmd) {
		case HASHMAP_CMD_REBUILD:
			hm_rp_rebuild(pop, hashmap, D_RO(hashmap)->capacity);
			return 0;
		case HASHMAP_CMD_DEBUG:
			if (!arg)
				return -EINVAL;
			hm_rp_debug(pop, hashmap, (FILE *)arg);
			return 0;
		default:
			return -EINVAL;
	}
}
