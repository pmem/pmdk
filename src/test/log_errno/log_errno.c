// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2023, Intel Corporation */

/*
 * traces.c -- unit test for traces
 */

#if 0
#define LOG_PREFIX "trace"
#define LOG_LEVEL_VAR "TRACE_LOG_LEVEL"
#define LOG_FILE_VAR "TRACE_LOG_FILE"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#endif

#include <sys/types.h>
#include <stdarg.h>
#include "unittest.h"
#include "log_internal.h"

int
main(int argc, char *argv[])
{
	// char buff[UT_MAX_ERR_MSG];

	core_log_init();
	START(argc, argv, "log_errno");

	CORE_LOG_ERROR_WITH_ERRNO("open file %s", "lolek");
	DONE(NULL);
}
