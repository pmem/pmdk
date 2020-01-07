// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017, Intel Corporation */

/*
 * obj_ctl_heap_size.c -- tests for the ctl entry points: heap.size.*
 */

#include "unittest.h"

#define LAYOUT "obj_ctl_heap_size"
#define CUSTOM_GRANULARITY ((1 << 20) * 10)
#define OBJ_SIZE 1024

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ctl_heap_size");

	if (argc != 3)
		UT_FATAL("usage: %s poolset [w|x]", argv[0]);

	const char *path = argv[1];
	char t = argv[2][0];

	PMEMobjpool *pop;

	if ((pop = pmemobj_open(path, LAYOUT)) == NULL)
		UT_FATAL("!pmemobj_open: %s", path);

	int ret = 0;
	size_t disable_granularity = 0;
	ret = pmemobj_ctl_set(pop, "heap.size.granularity",
		&disable_granularity);
	UT_ASSERTeq(ret, 0);

	/* allocate until OOM */
	while (pmemobj_alloc(pop, NULL, OBJ_SIZE, 0, NULL, NULL) == 0)
		;

	if (t == 'x') {
		ssize_t extend_size = CUSTOM_GRANULARITY;
		ret = pmemobj_ctl_exec(pop, "heap.size.extend", &extend_size);
		UT_ASSERTeq(ret, 0);
	} else if (t == 'w') {
		ssize_t new_granularity = CUSTOM_GRANULARITY;
		ret = pmemobj_ctl_set(pop, "heap.size.granularity",
			&new_granularity);
		UT_ASSERTeq(ret, 0);

		ssize_t curr_granularity;
		ret = pmemobj_ctl_get(pop, "heap.size.granularity",
			&curr_granularity);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(new_granularity, curr_granularity);
	} else {
		UT_ASSERT(0);
	}

	/* should succeed */
	ret = pmemobj_alloc(pop, NULL, OBJ_SIZE, 0, NULL, NULL);
	UT_ASSERTeq(ret, 0);

	pmemobj_close(pop);

	DONE(NULL);
}
