// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * obj_log.c -- the public interface to control the logging output
 */
#include <libpmemobj/log.h>

#include "log_internal.h"
#include "util.h"

/*
 * pmemobj_log_set_threshold -- set the log level threshold
 */
int
pmemobj_log_set_threshold(enum pmemobj_log_threshold threshold,
	enum pmemobj_log_level level)
{
	int ret = core_log_set_threshold((enum core_log_threshold)threshold,
		(enum core_log_level)level);
	return core_log_error_translate(ret);
}

/*
 * pmemobj_log_get_threshold -- get the log level threshold
 */
int
pmemobj_log_get_threshold(enum pmemobj_log_threshold threshold,
	enum pmemobj_log_level *level)
{
	int ret = core_log_get_threshold((enum core_log_threshold)threshold,
		(enum core_log_level *)level);
	return core_log_error_translate(ret);
}

/*
 * pmemobj_log_set_function -- set the log function pointer either to
 * a user-provided function pointer or to the default logging function.
 */
int
pmemobj_log_set_function(pmemobj_log_function *log_function, void *context)
{
	if (log_function == PMEMOBJ_LOG_USE_DEFAULT_FUNCTION)
		context = NULL;

	int ret = core_log_set_function((core_log_function *)log_function,
		context);
	return core_log_error_translate(ret);
}
