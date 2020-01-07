// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * obj_check.c -- unit tests for pmemobj_check
 */

#include <stddef.h>

#include "unittest.h"
#include "libpmemobj.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_check");
	if (argc < 2 || argc > 5)
		UT_FATAL("usage: obj_check <file> [-l <layout>] [-o]");

	const char *path = argv[1];
	const char *layout = NULL;
	PMEMobjpool *pop = NULL;
	int open = 0;

	for (int i = 2; i < argc; ++i) {
		if (strcmp(argv[i], "-o") == 0)
			open = 1;
		else if (strcmp(argv[i], "-l") == 0) {
			layout = argv[i + 1];
			i++;
		} else
			UT_FATAL("Unrecognized argument: %s", argv[i]);
	}

	if (open) {
		pop = pmemobj_open(path, layout);
		if (pop == NULL)
			UT_OUT("!%s: pmemobj_open", path);
		else
			UT_OUT("%s: pmemobj_open: Success", path);
	}

	int ret = pmemobj_check(path, layout);

	switch (ret) {
	case 1:
		UT_OUT("consistent");
		break;
	case 0:
		UT_OUT("not consistent: %s", pmemobj_errormsg());
		break;
	default:
		UT_OUT("error: %s", pmemobj_errormsg());
		break;
	}

	if (pop != NULL)
		pmemobj_close(pop);

	DONE(NULL);
}
