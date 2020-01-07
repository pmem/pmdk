// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * remote_basic.c -- unit test for remote tests support
 *
 * usage: remote_basic <file-to-be-checked>
 */

#include "file.h"
#include "unittest.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "remote_basic");

	if (argc != 2)
		UT_FATAL("usage: %s <file-to-be-checked>", argv[0]);

	const char *file = argv[1];

	int exists = util_file_exists(file);
	if (exists < 0)
		UT_FATAL("!util_file_exists");

	if (!exists)
		UT_FATAL("File '%s' does not exist", file);
	else
		UT_OUT("File '%s' exists", file);

	UT_OUT("An example of OUT message");

	UT_ERR("An example of ERR message");

	DONE(NULL);
}
