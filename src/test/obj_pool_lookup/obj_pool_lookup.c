// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * obj_pool_lookup.c -- unit test for pmemobj_pool and pmemobj_pool_of
 */
#include "unittest.h"

#define MAX_PATH_LEN 255
#define LAYOUT_NAME "pool_lookup"

#define ALLOC_SIZE 100

static void
define_path(char *str, size_t size, const char *dir, unsigned i)
{
	int ret = snprintf(str, size, "%s"OS_DIR_SEP_STR"testfile%d",
			dir, i);
	if (ret < 0 || ret >= size)
		UT_FATAL("snprintf: %d", ret);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pool_lookup");

	if (argc != 3)
		UT_FATAL("usage: %s [directory] [# of pools]", argv[0]);

	unsigned npools = ATOU(argv[2]);
	const char *dir = argv[1];
	int r;

	/* check before pool creation */
	PMEMoid some_oid = {2, 3};

	UT_ASSERTeq(pmemobj_pool_by_ptr(&some_oid), NULL);
	UT_ASSERTeq(pmemobj_pool_by_oid(some_oid), NULL);

	PMEMobjpool **pops = MALLOC(npools * sizeof(PMEMobjpool *));
	void **guard_after = MALLOC(npools * sizeof(void *));

	size_t length = strlen(dir) + MAX_PATH_LEN;
	char *path = MALLOC(length);
	for (unsigned i = 0; i < npools; ++i) {
		define_path(path, length, dir, i);
		pops[i] = pmemobj_create(path, LAYOUT_NAME, PMEMOBJ_MIN_POOL,
				S_IWUSR | S_IRUSR);

		/*
		 * Reserve a page after the pool for address checks, if it
		 * doesn't map precisely at that address - it's OK.
		 */
		guard_after[i] =
			MMAP((char *)pops[i] + PMEMOBJ_MIN_POOL, Ut_pagesize,
				PROT_READ | PROT_WRITE,
				MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

		UT_ASSERTne(guard_after[i], NULL);

		if (pops[i] == NULL)
			UT_FATAL("!pmemobj_create");
	}

	PMEMoid *oids = MALLOC(npools * sizeof(PMEMoid));

	for (unsigned i = 0; i < npools; ++i) {
		r = pmemobj_alloc(pops[i], &oids[i], ALLOC_SIZE, 1, NULL, NULL);
		UT_ASSERTeq(r, 0);
	}

	PMEMoid invalid = {123, 321};

	UT_ASSERTeq(pmemobj_pool_by_oid(OID_NULL), NULL);
	UT_ASSERTeq(pmemobj_pool_by_oid(invalid), NULL);

	for (unsigned i = 0; i < npools; ++i) {
		UT_ASSERTeq(pmemobj_pool_by_oid(oids[i]), pops[i]);
	}

	UT_ASSERTeq(pmemobj_pool_by_ptr(NULL), NULL);
	UT_ASSERTeq(pmemobj_pool_by_ptr((void *)0xCBA), NULL);

	void *valid_ptr = MALLOC(ALLOC_SIZE);
	UT_ASSERTeq(pmemobj_pool_by_ptr(valid_ptr), NULL);
	FREE(valid_ptr);

	for (unsigned i = 0; i < npools; ++i) {
		void *before_pool = (char *)pops[i] - 1;
		void *after_pool = (char *)pops[i] + PMEMOBJ_MIN_POOL + 1;
		void *start_pool = (char *)pops[i];
		void *end_pool = (char *)pops[i] + PMEMOBJ_MIN_POOL - 1;
		void *edge = (char *)pops[i] + PMEMOBJ_MIN_POOL;
		void *middle = (char *)pops[i] + (PMEMOBJ_MIN_POOL / 2);
		void *in_oid = (char *)pmemobj_direct(oids[i]) +
			(ALLOC_SIZE / 2);
		UT_ASSERTeq(pmemobj_pool_by_ptr(before_pool), NULL);
		UT_ASSERTeq(pmemobj_pool_by_ptr(after_pool), NULL);
		UT_ASSERTeq(pmemobj_pool_by_ptr(start_pool), pops[i]);
		UT_ASSERTeq(pmemobj_pool_by_ptr(end_pool), pops[i]);
		UT_ASSERTeq(pmemobj_pool_by_ptr(edge), NULL);
		UT_ASSERTeq(pmemobj_pool_by_ptr(middle), pops[i]);
		UT_ASSERTeq(pmemobj_pool_by_ptr(in_oid), pops[i]);
		pmemobj_close(pops[i]);
		UT_ASSERTeq(pmemobj_pool_by_ptr(middle), NULL);
		UT_ASSERTeq(pmemobj_pool_by_ptr(in_oid), NULL);

		MUNMAP(guard_after[i], Ut_pagesize);
	}

	for (unsigned i = 0; i < npools; ++i) {
		UT_ASSERTeq(pmemobj_pool_by_oid(oids[i]), NULL);

		define_path(path, length, dir, i);
		pops[i] = pmemobj_open(path, LAYOUT_NAME);
		UT_ASSERTne(pops[i], NULL);

		UT_ASSERTeq(pmemobj_pool_by_oid(oids[i]), pops[i]);

		pmemobj_close(pops[i]);
	}

	FREE(path);
	FREE(pops);
	FREE(guard_after);
	FREE(oids);

	DONE(NULL);
}
