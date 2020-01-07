// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * obj_ctl_debug.c -- tests for the ctl debug namesapce entry points
 */

#include "unittest.h"
#include "../../libpmemobj/obj.h"

#define LAYOUT "obj_ctl_debug"
#define BUFFER_SIZE 128
#define ALLOC_PATTERN 0xAC

static void
test_alloc_pattern(PMEMobjpool *pop)
{
	int ret;
	int pattern;
	PMEMoid oid;

	/* check default pattern */
	ret = pmemobj_ctl_get(pop, "debug.heap.alloc_pattern", &pattern);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(pattern, PALLOC_CTL_DEBUG_NO_PATTERN);

	/* check set pattern */
	pattern = ALLOC_PATTERN;
	ret = pmemobj_ctl_set(pop, "debug.heap.alloc_pattern", &pattern);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(pop->heap.alloc_pattern, pattern);

	/* check alloc with pattern */
	ret = pmemobj_alloc(pop, &oid, BUFFER_SIZE, 0, NULL, NULL);
	UT_ASSERTeq(ret, 0);

	char *buff = pmemobj_direct(oid);
	int i;
	for (i = 0; i < BUFFER_SIZE; i++)
		/* should trigger memcheck error: read uninitialized values */
		UT_ASSERTeq(*(buff + i), (char)pattern);

	pmemobj_free(&oid);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ctl_debug");

	if (argc < 2)
		UT_FATAL("usage: %s filename", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop;

	if ((pop = pmemobj_create(path, LAYOUT, PMEMOBJ_MIN_POOL,
		S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_open: %s", path);

	test_alloc_pattern(pop);

	pmemobj_close(pop);

	DONE(NULL);
}
