// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

/*
 * obj_toid.c -- unit test for TOID_VALID, DIRECT_RO, DIRECT_RW macros
 */
#include <sys/param.h>
#include "unittest.h"

#define LAYOUT_NAME "toid"
#define TEST_NUM 5
TOID_DECLARE(struct obj, 0);

struct obj {
	int id;
};

/*
 * do_toid_valid -- validates if type number is equal to object's metadata
 */
static void
do_toid_valid(PMEMobjpool *pop)
{
	TOID(struct obj) obj;
	POBJ_NEW(pop, &obj, struct obj, NULL, NULL);
	UT_ASSERT(!TOID_IS_NULL(obj));

	UT_ASSERT(TOID_VALID(obj));
	POBJ_FREE(&obj);
}

/*
 * do_toid_no_valid -- validates if type number is not equal to
 * object's metadata
 */
static void
do_toid_no_valid(PMEMobjpool *pop)
{
	TOID(struct obj) obj;
	int ret = pmemobj_alloc(pop, &obj.oid, sizeof(struct obj), TEST_NUM,
								NULL, NULL);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(!TOID_VALID(obj));
	POBJ_FREE(&obj);
}

/*
 * do_direct_simple - checks if DIRECT_RW and DIRECT_RO macros correctly
 * write and read from member of structure represented by TOID
 */
static void
do_direct_simple(PMEMobjpool *pop)
{
	TOID(struct obj) obj;
	POBJ_NEW(pop, &obj, struct obj, NULL, NULL);
	D_RW(obj)->id = TEST_NUM;
	pmemobj_persist(pop, &D_RW(obj)->id, sizeof(D_RW(obj)->id));
	UT_ASSERTeq(D_RO(obj)->id, TEST_NUM);
	POBJ_FREE(&obj);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_toid");

	if (argc != 2)
		UT_FATAL("usage: %s [file]", argv[0]);

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(argv[1], LAYOUT_NAME, PMEMOBJ_MIN_POOL,
	    S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create");

	do_toid_valid(pop);
	do_toid_no_valid(pop);
	do_direct_simple(pop);

	pmemobj_close(pop);

	DONE(NULL);
}
