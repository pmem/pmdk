// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

/*
 * obj_heap_state.c -- volatile heap state verification test
 */

#include <stddef.h>

#include "unittest.h"

#define LAYOUT_NAME "heap_state"
#define ROOT_SIZE 256
#define ALLOCS 100
#define ALLOC_SIZE 50

static char buf[ALLOC_SIZE];

static int
test_constructor(PMEMobjpool *pop, void *addr, void *args)
{
	/* do not use pmem_memcpy_persist() here */
	pmemobj_memcpy_persist(pop, addr, buf, ALLOC_SIZE);

	return 0;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_heap_state");

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	for (int i = 0; i < ALLOC_SIZE; i++)
		buf[i] = rand() % 256;

	PMEMobjpool *pop = NULL;

	if ((pop = pmemobj_create(path, LAYOUT_NAME,
			0, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	pmemobj_root(pop, ROOT_SIZE); /* just to trigger allocation */

	pmemobj_close(pop);

	pop = pmemobj_open(path, LAYOUT_NAME);
	UT_ASSERTne(pop, NULL);

	for (int i = 0; i < ALLOCS; ++i) {
		PMEMoid oid;
		pmemobj_alloc(pop, &oid, ALLOC_SIZE, 0,
				test_constructor, NULL);
		UT_OUT("%d %lu", i, oid.off);
	}

	pmemobj_close(pop);

	DONE(NULL);
}
