// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * obj_out_of_memory.c -- allocate objects until OOM
 */

#include <stdlib.h>
#include "unittest.h"

#define LAYOUT_NAME "out_of_memory"

struct cargs {
	size_t size;
};

static int
test_constructor(PMEMobjpool *pop, void *addr, void *args)
{
	struct cargs *a = args;
	pmemobj_memset_persist(pop, addr, rand() % 256, a->size / 2);

	return 0;
}

static void
test_alloc(PMEMobjpool *pop, size_t size)
{
	unsigned long cnt = 0;

	while (1) {
		struct cargs args = { size };
		if (pmemobj_alloc(pop, NULL, size, 0,
				test_constructor, &args) != 0)
			break;
		cnt++;
	}

	UT_OUT("size: %zu allocs: %lu", size, cnt);
}

static void
test_free(PMEMobjpool *pop)
{
	PMEMoid oid;
	PMEMoid next;

	POBJ_FOREACH_SAFE(pop, oid, next)
		pmemobj_free(&oid);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_out_of_memory");

	if (argc < 3)
		UT_FATAL("usage: %s size filename ...", argv[0]);

	size_t size = ATOUL(argv[1]);

	for (int i = 2; i < argc; i++) {
		const char *path = argv[i];

		PMEMobjpool *pop = pmemobj_create(path, LAYOUT_NAME, 0,
					S_IWUSR | S_IRUSR);
		if (pop == NULL)
			UT_FATAL("!pmemobj_create: %s", path);

		test_alloc(pop, size);

		pmemobj_close(pop);

		UT_ASSERTeq(pmemobj_check(path, LAYOUT_NAME), 1);

		/*
		 * To prevent subsequent opens from receiving exactly the same
		 * volatile memory addresses a dummy malloc has to be made.
		 * This can expose issues in which traces of previous volatile
		 * state are leftover in the persistent pool.
		 */
		void *heap_touch = MALLOC(1);

		UT_ASSERTne(pop = pmemobj_open(path, LAYOUT_NAME), NULL);

		test_free(pop);

		pmemobj_close(pop);

		FREE(heap_touch);
	}

	DONE(NULL);
}
