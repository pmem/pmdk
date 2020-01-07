// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2017, Intel Corporation */

/*
 * traces.c -- unit test for traces
 */

#define LOG_PREFIX "ut"
#define LOG_LEVEL_VAR "UT_LOG_LEVEL"
#define LOG_FILE_VAR "UT_LOG_FILE"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0

#include <sys/types.h>
#include <stdarg.h>
#include "pmemcommon.h"
#include "unittest.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "traces");

	/* Execute test */
	common_init(LOG_PREFIX, LOG_LEVEL_VAR, LOG_FILE_VAR,
			MAJOR_VERSION, MINOR_VERSION);
	LOG(0, "Log level NONE");
	LOG(1, "Log level ERROR");
	LOG(2, "Log level WARNING");
	LOG(3, "Log level INFO");
	LOG(4, "Log level DEBUG");

	/* Cleanup */
	common_fini();

	DONE(NULL);
}
