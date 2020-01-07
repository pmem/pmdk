// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * obj_root.c -- unit tests for pmemobj_root
 */

#include "unittest.h"

#define FILE_SIZE ((size_t)0x440000000) /* 17 GB */

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_root");
	if (argc < 2)
		UT_FATAL("usage: obj_root <file> [l]");

	const char *path = argv[1];
	PMEMobjpool *pop = NULL;
	os_stat_t st;
	int long_test = 0;

	if (argc >= 3 && argv[2][0] == 'l')
		long_test = 1;

	os_stat(path, &st);
	UT_ASSERTeq(st.st_size, FILE_SIZE);

	if ((pop = pmemobj_create(path, NULL, 0, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	errno = 0;
	PMEMoid oid = pmemobj_root(pop, 0);
	UT_ASSERT(OID_EQUALS(oid, OID_NULL));
	UT_ASSERTeq(errno, EINVAL);

	if (long_test) {
		oid = pmemobj_root(pop, PMEMOBJ_MAX_ALLOC_SIZE);
		UT_ASSERT(!OID_EQUALS(oid, OID_NULL));
	}

	oid = pmemobj_root(pop, 1);
	UT_ASSERT(!OID_EQUALS(oid, OID_NULL));

	oid = pmemobj_root(pop, 0);
	UT_ASSERT(!OID_EQUALS(oid, OID_NULL));

	errno = 0;
	oid = pmemobj_root(pop, FILE_SIZE);
	UT_ASSERT(OID_EQUALS(oid, OID_NULL));
	UT_ASSERTeq(errno, ENOMEM);

	errno = 0;
	oid = pmemobj_root(pop, SIZE_MAX);
	UT_ASSERT(OID_EQUALS(oid, OID_NULL));
	UT_ASSERTeq(errno, ENOMEM);

	pmemobj_close(pop);

	DONE(NULL);
}
