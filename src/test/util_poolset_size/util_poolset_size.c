// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016, Intel Corporation */

/*
 * util_poolset_size.c -- unit test for util_poolset_size
 *
 * usage: util_poolset_size file...
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
	START(argc, argv, "util_poolset_size");

	common_init(LOG_PREFIX, LOG_LEVEL_VAR, LOG_FILE_VAR,
			MAJOR_VERSION, MINOR_VERSION);

	if (argc < 2)
		UT_FATAL("usage: %s file...",
			argv[0]);

	for (int i = 1; i < argc; i++) {
		char *fname = argv[i];
		size_t size = util_poolset_size(fname);

		UT_OUT("util_poolset_size(%s): %lu", fname, size);
	}
	common_fini();

	DONE(NULL);
}
