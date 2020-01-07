// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

#include "unittest.h"
#include "valgrind_internal.h"

/*
 * Layout definition
 */
POBJ_LAYOUT_BEGIN(mc);
POBJ_LAYOUT_ROOT(mc, struct root);
POBJ_LAYOUT_TOID(mc, struct struct1);
POBJ_LAYOUT_END(mc);

struct struct1 {
	int fld;
	int dyn[];
};

struct root {
	TOID(struct struct1) s1;
	TOID(struct struct1) s2;
};

static void
test_memcheck_bug(void)
{
#if VG_MEMCHECK_ENABLED
	volatile char tmp[100];

	VALGRIND_CREATE_MEMPOOL(tmp, 0, 0);
	VALGRIND_MEMPOOL_ALLOC(tmp, tmp + 8, 16);
	VALGRIND_MEMPOOL_FREE(tmp, tmp + 8);
	VALGRIND_MEMPOOL_ALLOC(tmp, tmp + 8, 16);
	VALGRIND_MAKE_MEM_NOACCESS(tmp, 8);
	tmp[7] = 0x66;
#endif
}

static void
test_memcheck_bug2(void)
{
#if VG_MEMCHECK_ENABLED
	volatile char tmp[1000];

	VALGRIND_CREATE_MEMPOOL(tmp, 0, 0);

	VALGRIND_MEMPOOL_ALLOC(tmp, tmp + 128, 128);
	VALGRIND_MEMPOOL_FREE(tmp, tmp + 128);

	VALGRIND_MEMPOOL_ALLOC(tmp, tmp + 256, 128);
	VALGRIND_MEMPOOL_FREE(tmp, tmp + 256);

	/*
	 * This should produce warning:
	 * Address ... is 0 bytes inside a block of size 128 bytes freed.
	 * instead, it produces a warning:
	 * Address ... is 0 bytes after a block of size 128 freed
	 */
	int *data = (int *)(tmp + 256);
	*data = 0x66;
#endif
}

static void
test_everything(const char *path)
{
	PMEMobjpool *pop = NULL;

	if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(mc),
			PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	struct root *rt = D_RW(POBJ_ROOT(pop, struct root));

	POBJ_ALLOC(pop, &rt->s1, struct struct1, sizeof(struct struct1),
			NULL, NULL);
	struct struct1 *s1 = D_RW(rt->s1);
	struct struct1 *s2;

	POBJ_ALLOC(pop, &rt->s2, struct struct1, sizeof(struct struct1),
			NULL, NULL);
	s2 = D_RW(rt->s2);
	POBJ_FREE(&rt->s2);

	/* read of uninitialized variable */
	if (s1->fld)
		UT_OUT("%d", 1);

	/* write to freed object */
	s2->fld = 7;

	pmemobj_persist(pop, s2, sizeof(*s2));

	POBJ_ALLOC(pop, &rt->s2, struct struct1, sizeof(struct struct1),
			NULL, NULL);
	s2 = D_RW(rt->s2);
	memset(s2, 0, pmemobj_alloc_usable_size(rt->s2.oid));
	s2->fld = 12; /* ok */

	/* invalid write */
	s2->dyn[100000] = 9;

	/* invalid write */
	s2->dyn[1000] = 9;

	pmemobj_persist(pop, s2, sizeof(struct struct1));

	POBJ_REALLOC(pop, &rt->s2, struct struct1,
			sizeof(struct struct1) + 100 * sizeof(int));
	s2 = D_RW(rt->s2);
	s2->dyn[0] = 9; /* ok */
	pmemobj_persist(pop, s2, sizeof(struct struct1) + 100 * sizeof(int));

	POBJ_FREE(&rt->s2);
	/* invalid write to REALLOCated and FREEd object */
	s2->dyn[0] = 9;
	pmemobj_persist(pop, s2, sizeof(struct struct1) + 100 * sizeof(int));

	POBJ_ALLOC(pop, &rt->s2, struct struct1, sizeof(struct struct1),
			NULL, NULL);
	POBJ_REALLOC(pop, &rt->s2, struct struct1,
			sizeof(struct struct1) + 30 * sizeof(int));
	s2 = D_RW(rt->s2);
	s2->dyn[0] = 0;
	s2->dyn[29] = 29;
	pmemobj_persist(pop, s2, sizeof(struct struct1) + 30 * sizeof(int));
	POBJ_FREE(&rt->s2);

	s2->dyn[0] = 9;
	pmemobj_persist(pop, s2, sizeof(struct struct1) + 30 * sizeof(int));

	pmemobj_close(pop);
}

static void usage(const char *a)
{
	UT_FATAL("usage: %s [m|t] file-name", a);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_memcheck");

	/* root doesn't count */
	UT_COMPILE_ERROR_ON(POBJ_LAYOUT_TYPES_NUM(mc) != 1);

	if (argc < 2)
		usage(argv[0]);

	if (strcmp(argv[1], "m") == 0)
		test_memcheck_bug();
	else if (strcmp(argv[1], "t") == 0) {
		if (argc < 3)
			usage(argv[0]);
		test_everything(argv[2]);
	} else
		usage(argv[0]);

	test_memcheck_bug2();

	DONE(NULL);
}
