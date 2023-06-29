// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2023, Intel Corporation */

/*
 * pool_open.c -- a tool for verifying that an obj/blk pool opens correctly
 *
 * usage: pool_open <path> <obj|blk> <layout>
 */
#include "unittest.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "compat_incompat_features");
	if (argc < 2)
		UT_FATAL("usage: %s <path>", argv[0]);

	char *path = argv[1];

	PMEMobjpool *pop = pmemobj_open(path, "");
	if (pop == NULL) {
		UT_FATAL("!%s: pmemobj_open failed", path);
	} else {
		UT_OUT("%s: pmemobj_open succeeded", path);
		pmemobj_close(pop);
	}

	DONE(NULL);
}
