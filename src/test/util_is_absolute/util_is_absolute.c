// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016, Intel Corporation */

/*
 * util_is_absolute.c -- unit test for testing if path is absolute
 *
 * usage: util_is_absolute path [path ...]
 */

#include "unittest.h"
#include "file.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_is_absolute");

	for (int i = 1; i < argc; i++) {
		UT_OUT("\"%s\" - %d", argv[i],
				util_is_absolute_path(argv[i]));
	}

	DONE(NULL);
}
