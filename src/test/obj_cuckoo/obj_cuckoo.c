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
 * obj_cuckoo.c -- unit test for cuckoo hash table
 */

#include <errno.h>

#include "unittest.h"
#include "cuckoo.h"
#include "util.h"
#include "libpmemobj.h"

#define TEST_INSERTS 100
#define TEST_VAL(x) ((void *)((uintptr_t)(x)))

static int Rcounter_malloc;

static void *
__wrap_malloc(size_t size)
{
	switch (util_fetch_and_add32(&Rcounter_malloc, 1)) {
		case 1: /* internal out_err malloc */
		default:
			return malloc(size);
		case 2: /* tab malloc */
		case 0: /* cuckoo malloc */
			return NULL;
	}
}

static void
test_cuckoo_new_delete(void)
{
	struct cuckoo *c = NULL;

	/* cuckoo malloc fail */
	c = cuckoo_new();
	UT_ASSERT(c == NULL);

	/* tab malloc fail */
	c = cuckoo_new();
	UT_ASSERT(c == NULL);

	/* all ok */
	c = cuckoo_new();
	UT_ASSERT(c != NULL);

	cuckoo_delete(c);
}

static void
test_insert_get_remove(void)
{
	struct cuckoo *c = cuckoo_new();
	UT_ASSERT(c != NULL);

	for (int i = 0; i < TEST_INSERTS; ++i)
		UT_ASSERT(cuckoo_insert(c, i, TEST_VAL(i)) == 0);

	for (int i = 0; i < TEST_INSERTS; ++i)
		UT_ASSERT(cuckoo_get(c, i) == TEST_VAL(i));

	for (int i = 0; i < TEST_INSERTS; ++i)
		UT_ASSERT(cuckoo_remove(c, i) == TEST_VAL(i));

	for (int i = 0; i < TEST_INSERTS; ++i)
		UT_ASSERT(cuckoo_remove(c, i) == NULL);

	for (int i = 0; i < TEST_INSERTS; ++i)
		UT_ASSERT(cuckoo_get(c, i) == NULL);

	cuckoo_delete(c);
}

/*
 * rand64 -- (internal) 64-bit random function of doubtful quality, but good
 *	enough for the test.
 */
static uint64_t
rand64(void)
{
	return (((uint64_t)rand()) << 32) | rand();
}

#define NVALUES (100000)
#define TEST_VALUE ((void *)0x1)
#define INITIAL_SEED 54321

/*
 * test_load_factor -- calculates the average load factor of the hash table
 *	when inserting <0, 1M> elements in random order.
 *
 * The factor itself isn't really that important because the implementation
 *	is optimized for lookup speed, but it should be reasonable.
 */
static void
test_load_factor(void)
{
	struct cuckoo *c = cuckoo_new();
	UT_ASSERT(c != NULL);

	/*
	 * The seed is intentionally constant so that the test result is
	 * consistent (at least on the same platform).
	 */
	srand(INITIAL_SEED);

	float avg_load = 0.f;
	rand64();
	int inserted = 0;
	for (int i = 0; ; ++i) {
		if (cuckoo_insert(c, rand64() % NVALUES, TEST_VALUE) == 0) {
			inserted++;
			avg_load += (float)inserted / cuckoo_get_size(c);
			if (inserted == NVALUES)
				break;
		}
	}
	avg_load /= inserted;

	UT_ASSERT(avg_load >= 0.4f);

	cuckoo_delete(c);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_cuckoo");

	Malloc = __wrap_malloc;

	test_cuckoo_new_delete();
	test_insert_get_remove();
	test_load_factor();

	DONE(NULL);
}
