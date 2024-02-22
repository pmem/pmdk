// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * ut_log.c -- unit test log function
 *
 */

#include "unittest.h"
#include "out.h"

static const int core_log_level_to_out_level[] = {
	[CORE_LOG_LEVEL_HARK]		= 1,
	[CORE_LOG_LEVEL_FATAL]		= 1,
	[CORE_LOG_LEVEL_ERROR]		= 1,
	[CORE_LOG_LEVEL_WARNING]	= 2,
	[CORE_LOG_LEVEL_NOTICE]		= 3,
	[CORE_LOG_LEVEL_INFO]		= 4,
	[CORE_LOG_LEVEL_DEBUG]		= 4,
};

void
ut_log_function(void *context, enum core_log_level level, const char *file_name,
	const int line_no, const char *function_name, const char *message)
{
	out_log(file_name, line_no, function_name,
		core_log_level_to_out_level[(int)level], "%s", message);
}
