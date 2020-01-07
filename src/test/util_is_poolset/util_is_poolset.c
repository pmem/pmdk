// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016, Intel Corporation */

/*
 * util_is_poolset.c -- unit test for util_is_poolset
 *
 * usage: util_is_poolset file
 */

#include "unittest.h"
#include "set.h"
#include "pmemcommon.h"
#include <errno.h>

#define LOG_PREFIX "ut"
#define LOG_LEVEL_VAR "TEST_LOG_LEVEL"
#define LOG_FILE_VAR "TEST_LOG_FILE"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_is_poolset");

	common_init(LOG_PREFIX, LOG_LEVEL_VAR, LOG_FILE_VAR,
			MAJOR_VERSION, MINOR_VERSION);

	if (argc < 2)
		UT_FATAL("usage: %s file...",
			argv[0]);

	for (int i = 1; i < argc; i++) {
		char *fname = argv[i];
		int is_poolset = util_is_poolset_file(fname);

		UT_OUT("util_is_poolset(%s): %d", fname, is_poolset);
	}
	common_fini();

	DONE(NULL);
}
