// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * obj_check_remote.c -- unit tests for pmemobj_check_remote
 */

#include <stddef.h>
#include "unittest.h"
#include "libpmemobj.h"

struct vector {
	int x;
	int y;
	int z;
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_check_remote");

	if (argc < 3)
		UT_FATAL("insufficient number of arguments");

	const char *path = argv[1];
	const char *action = argv[2];
	const char *layout = NULL;
	PMEMobjpool *pop = NULL;

	if (strcmp(action, "abort") == 0) {
		pop = pmemobj_open(path, layout);
		if (pop == NULL)
			UT_FATAL("usage: %s filename abort|check", argv[0]);

		PMEMoid root = pmemobj_root(pop, sizeof(struct vector));
		struct vector *vectorp = pmemobj_direct(root);

		TX_BEGIN(pop) {
			pmemobj_tx_add_range(root, 0, sizeof(struct vector));
			vectorp->x = 5;
			vectorp->y = 10;
			vectorp->z = 15;
		} TX_ONABORT {
			UT_ASSERT(0);
		} TX_END

		int *to_modify = &vectorp->x;

		TX_BEGIN(pop) {
			pmemobj_tx_add_range_direct(to_modify, sizeof(int));
			*to_modify = 30;
			pmemobj_persist(pop, to_modify, sizeof(*to_modify));
			abort();
		} TX_END
	} else if (strcmp(action, "check") == 0) {
		int ret = pmemobj_check(path, layout);
		if (ret == 1)
			return 0;
		else
			return ret;
	} else {
		UT_FATAL("%s is not a valid action", action);
	}

	return 0;
}
