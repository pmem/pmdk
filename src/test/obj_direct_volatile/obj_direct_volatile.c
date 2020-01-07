// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * obj_direct_volatile.c -- unit test for pmemobj_direct_volatile()
 */
#include "unittest.h"

static PMEMobjpool *pop;

struct test {
	PMEMvlt(int) count;
};

#define TEST_OBJECTS 100
#define TEST_WORKERS 10
static struct test *tests[TEST_OBJECTS];

static int
test_constructor(void *ptr, void *arg)
{
	int *count = ptr;
	util_fetch_and_add32(count, 1);

	return 0;
}

static void *
test_worker(void *arg)
{
	for (int i = 0; i < TEST_OBJECTS; ++i) {
		int *count = pmemobj_volatile(pop, &tests[i]->count.vlt,
			&tests[i]->count.value, sizeof(tests[i]->count.value),
			test_constructor, NULL);
		UT_ASSERTne(count, NULL);
		UT_ASSERTeq(*count, 1);
	}
	return NULL;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_direct_volatile");

	if (argc != 2)
		UT_FATAL("usage: %s file", argv[0]);

	char *path = argv[1];
	pop = pmemobj_create(path, "obj_direct_volatile",
		PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR);
	if (pop == NULL)
		UT_FATAL("!pmemobj_create");

	for (int i = 0; i < TEST_OBJECTS; ++i) {
		PMEMoid oid;
		pmemobj_zalloc(pop, &oid, sizeof(struct test), 1);
		UT_ASSERT(!OID_IS_NULL(oid));
		tests[i] = pmemobj_direct(oid);
	}

	os_thread_t t[TEST_WORKERS];

	for (int i = 0; i < TEST_WORKERS; ++i) {
		THREAD_CREATE(&t[i], NULL, test_worker, NULL);
	}

	for (int i = 0; i < TEST_WORKERS; ++i) {
		THREAD_JOIN(&t[i], NULL);
	}

	for (int i = 0; i < TEST_OBJECTS; ++i) {
		UT_ASSERTeq(tests[i]->count.value, 1);
	}

	pmemobj_close(pop);

	DONE(NULL);
}
