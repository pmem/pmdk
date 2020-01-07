// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * obj_memcheck_register.c - tests that verifies that objects are registered
 *	correctly in memcheck
 */

#include "unittest.h"

static void
test_create(const char *path)
{
	PMEMobjpool *pop = NULL;

	if ((pop = pmemobj_create(path, "register",
		PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	PMEMoid oid = pmemobj_root(pop, 1024);

	TX_BEGIN(pop) {
		pmemobj_tx_alloc(1024, 0);
		pmemobj_tx_add_range(oid, 0, 10);
	} TX_END

	pmemobj_close(pop);
}

static void
test_open(const char *path)
{
	PMEMobjpool *pop = NULL;

	if ((pop = pmemobj_open(path, "register")) == NULL)
		UT_FATAL("!pmemobj_open: %s", path);

	PMEMoid oid = pmemobj_root(pop, 1024);

	TX_BEGIN(pop) {
		pmemobj_tx_add_range(oid, 0, 10);
	} TX_END

	pmemobj_close(pop);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_memcheck_register");

	if (argc != 3)
		UT_FATAL("usage: %s [c|o] file-name", argv[0]);

	switch (argv[1][0]) {
	case 'c':
		test_create(argv[2]);
		break;
	case 'o':
		test_open(argv[2]);
		break;
	default:
		UT_FATAL("usage: %s [c|o] file-name", argv[0]);
		break;
	}

	DONE(NULL);
}
