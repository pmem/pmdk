// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2020, Intel Corporation */

/*
 * obj_pmalloc_oom_mt.c -- build multithreaded out of memory test
 *
 */

#include <stddef.h>

#include "unittest.h"

#define TEST_ALLOC_SIZE (32 * 1024)
#define LAYOUT_NAME "oom_mt"

static int allocated;
static PMEMobjpool *pop;

static void *
oom_worker(void *arg)
{
	allocated = 0;
	while (pmemobj_alloc(pop, NULL, TEST_ALLOC_SIZE, 0, NULL, NULL) == 0)
		allocated++;

	PMEMoid iter, iter2;
	POBJ_FOREACH_SAFE(pop, iter, iter2)
		pmemobj_free(&iter);

	return NULL;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pmalloc_oom_mt");

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	if ((pop = pmemobj_create(path, LAYOUT_NAME,
			PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	os_thread_t t;
	THREAD_CREATE(&t, NULL, oom_worker, NULL);
	THREAD_JOIN(&t, NULL);

	int first_thread_allocated = allocated;

	THREAD_CREATE(&t, NULL, oom_worker, NULL);
	THREAD_JOIN(&t, NULL);

	UT_ASSERTeq(first_thread_allocated, allocated);

	pmemobj_close(pop);

	DONE(NULL);
}
