// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

/*
 * obj_recreate.c -- recreate pool on dirty file and check consistency
 */

#include "unittest.h"

POBJ_LAYOUT_BEGIN(recreate);
POBJ_LAYOUT_ROOT(recreate, struct root);
POBJ_LAYOUT_TOID(recreate, struct foo);
POBJ_LAYOUT_END(recreate);

struct foo {
	int bar;
};

struct root {
	TOID(struct foo) foo;
};

#define LAYOUT_NAME "obj_recreate"
#define N PMEMOBJ_MIN_POOL

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_recreate");

	/* root doesn't count */
	UT_COMPILE_ERROR_ON(POBJ_LAYOUT_TYPES_NUM(recreate) != 1);

	if (argc < 2)
		UT_FATAL("usage: %s file-name [trunc]", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop = NULL;

	/* create pool 2*N */
	pop = pmemobj_create(path, LAYOUT_NAME, 2 * N, S_IWUSR | S_IRUSR);
	if (pop == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	/* allocate 1.5*N */
	TOID(struct root) root = (TOID(struct root))pmemobj_root(pop,
		(size_t)(1.5 * N));

	/* use root object for something */
	POBJ_NEW(pop, &D_RW(root)->foo, struct foo, NULL, NULL);

	pmemobj_close(pop);

	int fd = OPEN(path, O_RDWR);

	if (argc >= 3 && strcmp(argv[2], "trunc") == 0) {
		UT_OUT("truncating");
		/* shrink file to N */
		FTRUNCATE(fd, N);
	}

	size_t zero_len = Ut_pagesize;
	/* zero first page */
	void *p = MMAP(NULL, zero_len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	memset(p, 0, zero_len);
	MUNMAP(p, zero_len);
	CLOSE(fd);

	/* create pool on existing file */
	pop = pmemobj_create(path, LAYOUT_NAME, 0, S_IWUSR | S_IRUSR);
	if (pop == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	/* try to allocate 0.7*N */
	root = (TOID(struct root))pmemobj_root(pop, (size_t)(0.5 * N));

	if (TOID_IS_NULL(root))
		UT_FATAL("couldn't allocate root object");

	/* validate root object is empty */
	if (!TOID_IS_NULL(D_RW(root)->foo))
		UT_FATAL("root object is already filled after pmemobj_create!");

	pmemobj_close(pop);

	DONE(NULL);
}
