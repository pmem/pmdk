// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016, Intel Corporation */

/*
 * remote_obj_basic.c -- unit test for remote tests support
 *
 * usage: remote_obj_basic <create|open> <poolset-file>
 */

#include "unittest.h"

#define LAYOUT_NAME "remote_obj_basic"

int
main(int argc, char *argv[])
{
	PMEMobjpool *pop;

	START(argc, argv, "remote_obj_basic");

	if (argc != 3)
		UT_FATAL("usage: %s <create|open> <poolset-file>", argv[0]);

	const char *mode = argv[1];
	const char *file = argv[2];

	if (strcmp(mode, "create") == 0) {
		if ((pop = pmemobj_create(file, LAYOUT_NAME, 0,
						S_IWUSR | S_IRUSR)) == NULL)
			UT_FATAL("!pmemobj_create: %s", file);
		else
			UT_OUT("The pool set %s has been created", file);

	} else if (strcmp(mode, "open") == 0) {
		if ((pop = pmemobj_open(file, LAYOUT_NAME)) == NULL)
			UT_FATAL("!pmemobj_open: %s", file);
		else
			UT_OUT("The pool set %s has been opened", file);

	} else {
		UT_FATAL("wrong mode: %s\n", argv[1]);
	}

	pmemobj_close(pop);

	DONE(NULL);
}
