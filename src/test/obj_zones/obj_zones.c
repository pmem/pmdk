// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2020, Intel Corporation */

/*
 * obj_zones.c -- allocates from a very large pool (exceeding 1 zone)
 *
 */

#include <stddef.h>
#include <page_size.h>

#include "unittest.h"

#define LAYOUT_NAME "obj_zones"

#define ALLOC_SIZE ((8191 * (256 * 1024)) - 16) /* must evenly divide a zone */

/*
 * test_create -- allocate all possible objects and log the number. It should
 * exceed what would be possible on a single zone.
 * Additionally, free one object so that we can later check that it can be
 * allocated after the next open.
 */
static void
test_create(const char *path)
{
	PMEMobjpool *pop = NULL;

	if ((pop = pmemobj_create(path, LAYOUT_NAME,
			0, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	PMEMoid oid;
	int n = 0;
	while (1) {
		if (pmemobj_alloc(pop, &oid, ALLOC_SIZE, 0, NULL, NULL) != 0)
			break;
		n++;
	}

	UT_OUT("allocated: %d", n);
	pmemobj_free(&oid);

	pmemobj_close(pop);
}

/*
 * test_open -- in the open test we should be able to allocate exactly
 * one object.
 */
static void
test_open(const char *path)
{
	PMEMobjpool *pop;
	if ((pop = pmemobj_open(path, LAYOUT_NAME)) == NULL)
		UT_FATAL("!pmemobj_open: %s", path);

	int ret = pmemobj_alloc(pop, NULL, ALLOC_SIZE, 0, NULL, NULL);
	UT_ASSERTeq(ret, 0);

	ret = pmemobj_alloc(pop, NULL, ALLOC_SIZE, 0, NULL, NULL);
	UT_ASSERTne(ret, 0);

	pmemobj_close(pop);
}

/*
 * test_malloc_free -- test if alloc until OOM/free/alloc until OOM sequence
 *	produces the same number of allocations for the second alloc loop.
 */
static void
test_malloc_free(const char *path)
{
	PMEMobjpool *pop = NULL;
	if ((pop = pmemobj_create(path, LAYOUT_NAME,
			0, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	size_t alloc_size = PMEM_PAGESIZE * 32;
	size_t max_allocs = 1000000;
	PMEMoid *oid = MALLOC(sizeof(PMEMoid) * max_allocs);
	size_t n = 0;
	while (1) {
		if (pmemobj_alloc(pop, &oid[n], alloc_size, 0, NULL, NULL) != 0)
			break;
		n++;
		UT_ASSERTne(n, max_allocs);
	}
	size_t first_run_allocated = n;

	for (size_t i = 0; i < n; ++i) {
		pmemobj_free(&oid[i]);
	}

	n = 0;
	while (1) {
		if (pmemobj_alloc(pop, &oid[n], alloc_size, 0, NULL, NULL) != 0)
			break;
		n++;
	}
	UT_ASSERTeq(first_run_allocated, n);

	pmemobj_close(pop);
	FREE(oid);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_zones");

	if (argc != 3)
		UT_FATAL("usage: %s file-name [open|create]", argv[0]);

	const char *path = argv[1];
	char op = argv[2][0];
	if (op == 'c')
		test_create(path);
	else if (op == 'o')
		test_open(path);
	else if (op == 'f')
		test_malloc_free(path);
	else
		UT_FATAL("invalid operation");

	DONE(NULL);
}
