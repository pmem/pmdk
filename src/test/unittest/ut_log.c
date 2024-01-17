// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * ut_log.c -- unit test log function
 *
 */

#include "unittest.h"
#include "out.h"
#include "log_internal.h"

#ifdef USE_LOG_PMEMOBJ
#include "libpmemobj/log.h"
#endif

static const int core_log_level_to_out_level[] = {
	[CORE_LOG_LEVEL_FATAL]		= 1,
	[CORE_LOG_LEVEL_ERROR]		= 1,
	[CORE_LOG_LEVEL_WARNING]	= 2,
	[CORE_LOG_LEVEL_NOTICE]		= 3,
	[CORE_LOG_LEVEL_INFO]		= 4,
	[CORE_LOG_LEVEL_DEBUG]		= 4,
};

static inline void
ut_log_function_va(void *context, enum core_log_level level,
	const char *file_name, const int line_no, const char *function_name,
	const char *message_format, va_list arg)
{
	if (level == CORE_LOG_LEVEL_ALWAYS)
		level = CORE_LOG_LEVEL_ERROR;

	out_log_va(file_name, line_no, function_name,
		core_log_level_to_out_level[(int)level], message_format, arg);
}

#ifdef USE_LOG_PMEMCORE
void
ut_log_function(void *context, enum core_log_level level, const char *file_name,
	const int line_no, const char *function_name,
	const char *message_format, ...)
{
	va_list arg;
	va_start(arg, message_format);
	ut_log_function_va(context, level, file_name, line_no, function_name,
		message_format, arg);
	va_end(arg);
}
#endif

#ifdef USE_LOG_PMEMOBJ
void
ut_log_function_pmemobj(void *context, enum pmemobj_log_level level,
	const char *file_name, const int line_no, const char *function_name,
	const char *message_format, ...)
{
	va_list arg;
	va_start(arg, message_format);
	ut_log_function_va(context, (enum core_log_level)level, file_name,
		line_no, function_name, message_format, arg);
	va_end(arg);
}
#endif
