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
	if (argc < 3)
		UT_FATAL("usage: %s <obj|blk> <path>", argv[0]);

	char *type = argv[1];
	char *path = argv[2];

	if (strcmp(type, "obj") == 0) {
		PMEMobjpool *pop = pmemobj_open(path, "");
		if (pop == NULL) {
			UT_FATAL("!%s: pmemobj_open failed", path);
		} else {
			UT_OUT("%s: pmemobj_open succeeded", path);
			pmemobj_close(pop);
		}
	} else if (strcmp(type, "blk") == 0) {
		PMEMblkpool *pop = pmemblk_open(path, 0);
		if (pop == NULL) {
			UT_FATAL("!%s: pmemblk_open failed", path);
		} else {
			UT_OUT("%s: pmemblk_open succeeded", path);
			pmemblk_close(pop);
		}
	} else {
		UT_FATAL("usage: %s <obj|blk> <path>", argv[0]);
	}

	DONE(NULL);
}
