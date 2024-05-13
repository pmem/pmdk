// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2024, Intel Corporation */

/*
 * traces.c -- unit test for traces
 */

#define LOG_PREFIX "trace"
#define LOG_LEVEL_VAR "TRACE_LOG_LEVEL"
#define LOG_FILE_VAR "TRACE_LOG_FILE"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0

#include <sys/types.h>
#include <stdarg.h>
#include "unittest.h"
#include "pmemcommon.h"

int
main(int argc, char *argv[])
{

	START(argc, argv, "out_err");

	/* Execute test */
	common_init(LOG_PREFIX, LOG_LEVEL_VAR, LOG_FILE_VAR,
			MAJOR_VERSION, MINOR_VERSION);

	errno = 0;
	ERR_WO_ERRNO("ERR #%d", 1);
	UT_OUT("%s", last_error_msg_get());

	errno = 0;
	ERR_W_ERRNO("ERR #%d", 2);
	UT_OUT("%s", last_error_msg_get());

	errno = EINVAL;
	ERR_W_ERRNO("ERR #%d", 3);
	UT_OUT("%s", last_error_msg_get());

	/* Cleanup */
	common_fini();

	DONE(NULL);
}
