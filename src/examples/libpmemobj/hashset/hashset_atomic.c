/*
 * Copyright (c) 2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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

/* integer hash set implementation which uses only atomic APIs */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libpmemobj.h>
#include "hashset.h"
#include "hashset_internal.h"

/* layout definition */
POBJ_LAYOUT_BEGIN(pm_hashset);
POBJ_LAYOUT_ROOT(pm_hashset, struct hashset);
POBJ_LAYOUT_TOID(pm_hashset, struct buckets);
POBJ_LAYOUT_TOID(pm_hashset, struct entry);
POBJ_LAYOUT_END(pm_hashset);

struct entry {
	uint64_t value;

	/* list pointer */
	POBJ_LIST_ENTRY(struct entry) list;
};

struct buckets {
	/* number of buckets */
	size_t nbuckets;
	/* array of lists */
	POBJ_LIST_HEAD(entries_head, struct entry) bucket[];
};

struct hashset {
	/* random number generator seed */
	uint32_t seed;

	/* hash function coefficients */
	uint32_t hash_fun_a;
	uint32_t hash_fun_b;
	uint64_t hash_fun_p;

	/* number of values inserted */
	uint64_t count;
	/* whether "count" should be updated */
	uint32_t count_dirty;

	/* buckets */
	TOID(struct buckets) buckets;
	/* buckets, used during rehashing, null otherwise */
	TOID(struct buckets) buckets_tmp;
};

/*
 * hs_layout_name -- layout name, used for pmemobj_create/pmemobj_open
 */
const char *
hs_layout_name(void)
{
	return POBJ_LAYOUT_NAME(pm_hashset);
}

/*
 * create_entry -- entry initializer
 */
static void
create_entry(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct entry *e = ptr;

	e->value = *((uint64_t *)arg);
	memset(&e->list, 0, sizeof (e->list));

	pmemobj_persist(pop, e, sizeof (*e));
}

/*
 * create_buckets -- buckets initializer
 */
static void
create_buckets(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct buckets *b = ptr;

	b->nbuckets = *((size_t *)arg);
	pmemobj_memset_persist(pop, &b->bucket, 0,
			b->nbuckets * sizeof (b->bucket[0]));
	pmemobj_persist(pop, &b->nbuckets, sizeof (b->nbuckets));
}

/*
 * create_hashset -- hashset initializer
 */
static void
create_hashset(PMEMobjpool *pop, TOID(struct hashset) hashset, uint32_t seed)
{
	D_RW(hashset)->seed = seed;
	D_RW(hashset)->hash_fun_a = (uint32_t)(1000.0 * rand() / RAND_MAX) + 1;
	D_RW(hashset)->hash_fun_b = (uint32_t)(100000.0 * rand() / RAND_MAX);
	D_RW(hashset)->hash_fun_p = HASH_FUNC_COEFF_P;

	size_t len = INIT_BUCKETS_NUM;
	size_t sz = sizeof (struct buckets) +
			len * sizeof (struct entries_head);

	if (POBJ_ALLOC(pop, &D_RW(hashset)->buckets, struct buckets, sz,
			create_buckets, &len)) {
		fprintf(stderr, "root alloc failed: %s\n", pmemobj_errormsg());
		abort();
	}

	pmemobj_persist(pop, D_RW(hashset), sizeof (*D_RW(hashset)));
}

/*
 * hash -- the simplest hashing function,
 * see https://en.wikipedia.org/wiki/Universal_hashing#Hashing_integers
 */
static uint64_t
hash(const TOID(struct hashset) *hashset, const TOID(struct buckets) *buckets,
	uint64_t value)
{
	uint32_t a = D_RO(*hashset)->hash_fun_a;
	uint32_t b = D_RO(*hashset)->hash_fun_b;
	uint64_t p = D_RO(*hashset)->hash_fun_p;
	size_t len = D_RO(*buckets)->nbuckets;

	return ((a * value + b) % p) % len;
}

/*
 * hs_rebuild_finish -- finishes rebuild, assumes buckets_tmp is not null
 */
static void
hs_rebuild_finish(PMEMobjpool *pop)
{
	TOID(struct hashset) hashset = POBJ_ROOT(pop, struct hashset);
	TOID(struct buckets) cur = D_RO(hashset)->buckets;
	TOID(struct buckets) tmp = D_RO(hashset)->buckets_tmp;

	for (size_t i = 0; i < D_RO(cur)->nbuckets; ++i) {
		while (!POBJ_LIST_EMPTY(&D_RO(cur)->bucket[i])) {
			TOID(struct entry) en =
					POBJ_LIST_FIRST(&D_RO(cur)->bucket[i]);
			uint64_t h = hash(&hashset, &tmp, D_RO(en)->value);

			if (POBJ_LIST_MOVE_ELEMENT_HEAD(pop,
					&D_RW(cur)->bucket[i],
					&D_RW(tmp)->bucket[h],
					en, list, list)) {
				fprintf(stderr, "move failed: %s\n",
						pmemobj_errormsg());
				abort();
			}
		}
	}

	POBJ_FREE(&D_RO(hashset)->buckets);

	D_RW(hashset)->buckets = D_RO(hashset)->buckets_tmp;
	pmemobj_persist(pop, &D_RW(hashset)->buckets,
			sizeof (D_RW(hashset)->buckets));

	/*
	 * We have to set offset manually instead of substituting OID_NULL,
	 * because we won't be able to recover easily if crash happens after
	 * pool_uuid_lo, but before offset is set. Another reason why everyone
	 * should use transaction API.
	 * See recovery process in hs_init and TOID_IS_NULL macro definition.
	 */
	D_RW(hashset)->buckets_tmp.oid.off = 0;
	pmemobj_persist(pop, &D_RW(hashset)->buckets_tmp,
			sizeof (D_RW(hashset)->buckets_tmp));
}

/*
 * hs_rebuild -- rebuilds the hashset with a new number of buckets
 */
void
hs_rebuild(PMEMobjpool *pop, size_t new_len)
{
	TOID(struct hashset) hashset = POBJ_ROOT(pop, struct hashset);
	if (new_len == 0)
		new_len = D_RO(D_RO(hashset)->buckets)->nbuckets;

	printf("rebuild ");
	fflush(stdout);
	time_t t1 = time(NULL);
	size_t sz = sizeof (struct buckets) +
			new_len * sizeof (struct entries_head);

	POBJ_ALLOC(pop, &D_RW(hashset)->buckets_tmp, struct buckets, sz,
			create_buckets, &new_len);
	if (TOID_IS_NULL(D_RO(hashset)->buckets_tmp)) {
		printf("\n");
		fprintf(stderr,
			"failed to allocate temporary space of size: %lu, %s\n",
			new_len, pmemobj_errormsg());
		return;
	}

	hs_rebuild_finish(pop);
	printf("%lus\n", time(NULL) - t1);
}

/*
 * hs_insert -- inserts specified value into the hashset,
 * returns:
 * - 1 if successful,
 * - 0 if value already existed,
 * - -1 if something bad happened
 */
int
hs_insert(PMEMobjpool *pop, uint64_t value)
{
	TOID(struct hashset) hashset = POBJ_ROOT(pop, struct hashset);
	TOID(struct buckets) buckets = D_RO(hashset)->buckets;
	TOID(struct entry) var;

	uint64_t h = hash(&hashset, &buckets, value);
	int num = 0;

	POBJ_LIST_FOREACH(var, &D_RO(buckets)->bucket[h], list) {
		if (D_RO(var)->value == value)
			return 0;
		num++;
	}

	D_RW(hashset)->count_dirty = 1;
	pmemobj_persist(pop, &D_RW(hashset)->count_dirty,
			sizeof (D_RW(hashset)->count_dirty));

	PMEMoid oid = POBJ_LIST_INSERT_NEW_HEAD(pop, &D_RW(buckets)->bucket[h],
			list, sizeof (struct entry), create_entry, &value);
	if (OID_IS_NULL(oid)) {
		fprintf(stderr, "failed to allocate entry: %s\n",
			pmemobj_errormsg());
		return -1;
	}

	D_RW(hashset)->count++;
	pmemobj_persist(pop, &D_RW(hashset)->count,
			sizeof (D_RW(hashset)->count));

	D_RW(hashset)->count_dirty = 0;
	pmemobj_persist(pop, &D_RW(hashset)->count_dirty,
			sizeof (D_RW(hashset)->count_dirty));

	num++;
	if (num > MAX_HASHSET_THRESHOLD ||
			(num > MIN_HASHSET_THRESHOLD &&
			D_RO(hashset)->count > 2 * D_RO(buckets)->nbuckets))
		hs_rebuild(pop, D_RW(buckets)->nbuckets * 2);

	return 1;
}

/*
 * hs_remove -- removes specified value from the hashset,
 * returns:
 * - 1 if successful,
 * - 0 if value didn't exist,
 * - -1 if something bad happened
 */
int
hs_remove(PMEMobjpool *pop, uint64_t value)
{
	TOID(struct hashset) hashset = POBJ_ROOT(pop, struct hashset);
	TOID(struct buckets) buckets = D_RO(hashset)->buckets;
	TOID(struct entry) var;

	uint64_t h = hash(&hashset, &buckets, value);
	POBJ_LIST_FOREACH(var, &D_RW(buckets)->bucket[h], list) {
		if (D_RO(var)->value == value)
			break;
	}

	if (TOID_IS_NULL(var))
		return 0;

	D_RW(hashset)->count_dirty = 1;
	pmemobj_persist(pop, &D_RW(hashset)->count_dirty,
			sizeof (D_RW(hashset)->count_dirty));

	if (POBJ_LIST_REMOVE_FREE(pop, &D_RW(buckets)->bucket[h], var, list)) {
		fprintf(stderr, "list remove failed: %s\n",
			pmemobj_errormsg());
		return -1;
	}

	D_RW(hashset)->count--;
	pmemobj_persist(pop, &D_RW(hashset)->count,
			sizeof (D_RW(hashset)->count));

	D_RW(hashset)->count_dirty = 0;
	pmemobj_persist(pop, &D_RW(hashset)->count_dirty,
			sizeof (D_RW(hashset)->count_dirty));

	if (D_RO(hashset)->count < D_RO(buckets)->nbuckets)
		hs_rebuild(pop, D_RO(buckets)->nbuckets / 2);

	return 1;
}

/*
 * hs_print -- prints all values from the hashset
 */
void
hs_print(PMEMobjpool *pop)
{
	TOID(struct hashset) hashset = POBJ_ROOT(pop, struct hashset);
	TOID(struct buckets) buckets = D_RO(hashset)->buckets;
	TOID(struct entry) var;

	printf("count: %lu\n", D_RO(hashset)->count);
	for (size_t i = 0; i < D_RO(buckets)->nbuckets; ++i)
		POBJ_LIST_FOREACH(var, &D_RO(buckets)->bucket[i], list)
			printf("%lu ", D_RO(var)->value);
	printf("\n");
}

/*
 * hs_debug -- prints complete hashset state
 */
void
hs_debug(PMEMobjpool *pop)
{
	TOID(struct hashset) hashset = POBJ_ROOT(pop, struct hashset);
	TOID(struct buckets) buckets = D_RO(hashset)->buckets;
	TOID(struct entry) var;

	printf("a: %u b: %u p: %lu\n", D_RO(hashset)->hash_fun_a,
		D_RO(hashset)->hash_fun_b, D_RO(hashset)->hash_fun_p);
	printf("count: %lu, buckets: %lu\n", D_RO(hashset)->count,
		D_RO(buckets)->nbuckets);

	for (size_t i = 0; i < D_RO(buckets)->nbuckets; ++i) {
		if (POBJ_LIST_EMPTY(&D_RO(buckets)->bucket[i]))
			continue;

		int num = 0;
		printf("%lu: ", i);
		POBJ_LIST_FOREACH(var, &D_RO(buckets)->bucket[i], list) {
			printf("%lu ", D_RO(var)->value);
			num++;
		}
		printf("(%d)\n", num);
	}
}

/*
 * hs_check -- checks whether specified value is in the hashset
 */
int
hs_check(PMEMobjpool *pop, uint64_t value)
{
	TOID(struct hashset) hashset = POBJ_ROOT(pop, struct hashset);
	TOID(struct buckets) buckets = D_RO(hashset)->buckets;
	TOID(struct entry) var;

	uint64_t h = hash(&hashset, &buckets, value);

	POBJ_LIST_FOREACH(var, &D_RO(buckets)->bucket[h], list)
		if (D_RO(var)->value == value)
			return 1;

	return 0;
}

/*
 * hs_create --  initializes hashset state, called after pmemobj_create
 */
void
hs_create(PMEMobjpool *pop, uint32_t seed)
{
	create_hashset(pop, POBJ_ROOT(pop, struct hashset), seed);
}

/*
 * hs_init -- recovers hashset state, called after pmemobj_open
 */
void
hs_init(PMEMobjpool *pop)
{
	TOID(struct hashset) hashset = POBJ_ROOT(pop, struct hashset);

	printf("seed: %u\n", D_RO(hashset)->seed);
	srand(D_RO(hashset)->seed);

	/* handle rebuild interruption */
	if (!TOID_IS_NULL(D_RO(hashset)->buckets_tmp)) {
		printf("rebuild, previous attempt crashed\n");
		if (TOID_EQUALS(D_RO(hashset)->buckets,
				D_RO(hashset)->buckets_tmp)) {
			/* see comment in hs_rebuild_finish */
			D_RW(hashset)->buckets_tmp.oid.off = 0;
			pmemobj_persist(pop, &D_RW(hashset)->buckets_tmp,
					sizeof (D_RW(hashset)->buckets_tmp));
		} else if (TOID_IS_NULL(D_RW(hashset)->buckets)) {
			D_RW(hashset)->buckets = D_RW(hashset)->buckets_tmp;
			pmemobj_persist(pop, &D_RW(hashset)->buckets,
					sizeof (D_RW(hashset)->buckets));
			/* see comment in hs_rebuild_finish */
			D_RW(hashset)->buckets_tmp.oid.off = 0;
			pmemobj_persist(pop, &D_RW(hashset)->buckets_tmp,
					sizeof (D_RW(hashset)->buckets_tmp));
		} else {
			hs_rebuild_finish(pop);
		}
	}

	/* handle insert or remove interruption */
	if (D_RO(hashset)->count_dirty) {
		printf("count dirty, recalculating\n");
		TOID(struct entry) var;
		TOID(struct buckets) buckets = D_RO(hashset)->buckets;
		uint64_t cnt = 0;

		for (size_t i = 0; i < D_RO(buckets)->nbuckets; ++i)
			POBJ_LIST_FOREACH(var, &D_RO(buckets)->bucket[i], list)
				cnt++;

		printf("old count: %lu, new count: %lu\n",
			D_RO(hashset)->count, cnt);
		D_RW(hashset)->count = cnt;
		pmemobj_persist(pop, &D_RW(hashset)->count,
				sizeof (D_RW(hashset)->count));

		D_RW(hashset)->count_dirty = 0;
		pmemobj_persist(pop, &D_RW(hashset)->count_dirty,
				sizeof (D_RW(hashset)->count_dirty));
	}
}
