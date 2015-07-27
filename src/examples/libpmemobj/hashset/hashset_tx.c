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

/* integer hash set implementation which uses only transaction APIs */

#include <stdlib.h>
#include <stdio.h>

#include <libpmemobj.h>
#include "hashset.h"
#include "hashset_internal.h"

/* layout definition */
POBJ_LAYOUT_BEGIN(pm_hashset_tx);
POBJ_LAYOUT_ROOT(pm_hashset_tx, struct hashset);
POBJ_LAYOUT_TOID(pm_hashset_tx, struct buckets);
POBJ_LAYOUT_TOID(pm_hashset_tx, struct entry);
POBJ_LAYOUT_END(pm_hashset_tx);

struct entry {
	uint64_t value;

	/* next entry list pointer */
	TOID(struct entry) next;
};

struct buckets {
	/* number of buckets */
	size_t nbuckets;
	/* array of lists */
	TOID(struct entry) bucket[];
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

	/* buckets */
	TOID(struct buckets) buckets;
};

/*
 * hs_layout_name -- layout name, used for pmemobj_create/pmemobj_open
 */
const char *
hs_layout_name(void)
{
	return POBJ_LAYOUT_NAME(pm_hashset_tx);
}

/*
 * create_hashset -- hashset initializer
 */
static void
create_hashset(PMEMobjpool *pop, TOID(struct hashset) hashset, uint32_t seed)
{
	size_t len = INIT_BUCKETS_NUM;
	size_t sz = sizeof (struct buckets) +
			len * sizeof (TOID(struct entry));

	TX_BEGIN(pop) {
		TX_ADD(hashset);

		D_RW(hashset)->seed = seed;
		D_RW(hashset)->hash_fun_a =
				(uint32_t)(1000.0 * rand() / RAND_MAX) + 1;
		D_RW(hashset)->hash_fun_b =
				(uint32_t)(100000.0 * rand() / RAND_MAX);
		D_RW(hashset)->hash_fun_p = HASH_FUNC_COEFF_P;

		D_RW(hashset)->buckets = TX_ZALLOC(struct buckets, sz);
		D_RW(D_RW(hashset)->buckets)->nbuckets = len;
	} TX_ONABORT {
		fprintf(stderr, "%s: transaction aborted: %s\n", __func__,
			pmemobj_errormsg());
		abort();
	} TX_END
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
 * hs_rebuild -- rebuilds the hashset with a new number of buckets
 */
void
hs_rebuild(PMEMobjpool *pop, size_t new_len)
{
	TOID(struct hashset) hashset = POBJ_ROOT(pop, struct hashset);
	TOID(struct buckets) buckets_old = D_RO(hashset)->buckets;

	if (new_len == 0)
		new_len = D_RO(buckets_old)->nbuckets;

	printf("rebuild ");
	fflush(stdout);
	time_t t1 = time(NULL);
	size_t sz_old = sizeof (struct buckets) +
			D_RO(buckets_old)->nbuckets *
			sizeof (TOID(struct entry));
	size_t sz_new = sizeof (struct buckets) +
			new_len * sizeof (TOID(struct entry));

	TX_BEGIN(pop) {
		TX_ADD_FIELD(hashset, buckets);
		TOID(struct buckets) buckets_new =
				TX_ZALLOC(struct buckets, sz_new);
		D_RW(buckets_new)->nbuckets = new_len;
		pmemobj_tx_add_range(buckets_old.oid, 0, sz_old);

		for (size_t i = 0; i < D_RO(buckets_old)->nbuckets; ++i) {
			while (!TOID_IS_NULL(D_RO(buckets_old)->bucket[i])) {
				TOID(struct entry) en =
					D_RO(buckets_old)->bucket[i];
				uint64_t h = hash(&hashset, &buckets_new,
						D_RO(en)->value);

				D_RW(buckets_old)->bucket[i] = D_RO(en)->next;

				TX_ADD_FIELD(en, next);
				D_RW(en)->next = D_RO(buckets_new)->bucket[h];
				D_RW(buckets_new)->bucket[h] = en;
			}
		}

		D_RW(hashset)->buckets = buckets_new;
		TX_FREE(buckets_old);
	} TX_ONABORT {
		fprintf(stderr, "%s: transaction aborted: %s\n", __func__,
			pmemobj_errormsg());
		/*
		 * We don't need to do anything here, because everything is
		 * consistent. The only thing affected is performance.
		 */
	} TX_END

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

	for (var = D_RO(buckets)->bucket[h];
			!TOID_IS_NULL(var);
			var = D_RO(var)->next) {
		if (D_RO(var)->value == value)
			return 0;
		num++;
	}

	TX_BEGIN(pop) {
		TX_ADD_FIELD(D_RO(hashset)->buckets, bucket[h]);
		TX_ADD_FIELD(hashset, count);

		TOID(struct entry) e = TX_NEW(struct entry);
		D_RW(e)->value = value;
		D_RW(e)->next = D_RO(buckets)->bucket[h];
		D_RW(buckets)->bucket[h] = e;

		D_RW(hashset)->count++;
		num++;
	} TX_ONABORT {
		fprintf(stderr, "transaction aborted: %s\n",
			pmemobj_errormsg());
		return -1;
	} TX_END

	if (num > MAX_HASHSET_THRESHOLD ||
			(num > MIN_HASHSET_THRESHOLD &&
			D_RO(hashset)->count > 2 * D_RO(buckets)->nbuckets))
		hs_rebuild(pop, D_RO(buckets)->nbuckets * 2);

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
	TOID(struct entry) var, prev = TOID_NULL(struct entry);

	uint64_t h = hash(&hashset, &buckets, value);
	for (var = D_RO(buckets)->bucket[h];
			!TOID_IS_NULL(var);
			prev = var, var = D_RO(var)->next) {
		if (D_RO(var)->value == value)
			break;
	}

	if (TOID_IS_NULL(var))
		return 0;

	TX_BEGIN(pop) {
		if (TOID_IS_NULL(prev))
			TX_ADD_FIELD(D_RO(hashset)->buckets, bucket[h]);
		else
			TX_ADD_FIELD(prev, next);
		TX_ADD_FIELD(hashset, count);

		if (TOID_IS_NULL(prev))
			D_RW(buckets)->bucket[h] = D_RO(var)->next;
		else
			D_RW(prev)->next = D_RO(var)->next;
		D_RW(hashset)->count--;
		TX_FREE(var);
	} TX_ONABORT {
		fprintf(stderr, "transaction aborted: %s\n",
			pmemobj_errormsg());
		return -1;
	} TX_END

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
	for (size_t i = 0; i < D_RO(buckets)->nbuckets; ++i) {
		if (TOID_IS_NULL(D_RO(buckets)->bucket[i]))
			continue;

		for (var = D_RO(buckets)->bucket[i]; !TOID_IS_NULL(var);
				var = D_RO(var)->next)
			printf("%lu ", D_RO(var)->value);
	}
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
		if (TOID_IS_NULL(D_RO(buckets)->bucket[i]))
			continue;

		int num = 0;
		printf("%lu: ", i);
		for (var = D_RO(buckets)->bucket[i]; !TOID_IS_NULL(var);
				var = D_RO(var)->next) {
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

	for (var = D_RO(buckets)->bucket[h];
			!TOID_IS_NULL(var);
			var = D_RO(var)->next)
		if (D_RO(var)->value == value)
			return 1;

	return 0;
}

/*
 * hs_create -- initializes hashset state, called after pmemobj_create
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
}
