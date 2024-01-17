// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * ut_log.c -- unit test log function
 *
 */

#include "unittest.h"
#include "out.h"

void
ut_log_function(enum core_log_level level, const char *file_name,
	const int line_no, const char *function_name,
	const char *message_format, ...)
{
	if (file_name) {
		/* extract base_file_name */
		const char *base_file_name = strrchr(file_name, '/');
		if (!base_file_name)
			base_file_name = file_name;
		else
			/* skip '/' */
			base_file_name++;

		char message[1024] = "";
		va_list arg;
		va_start(arg, message_format);
		if (vsnprintf(message, sizeof(message), message_format, arg)
				< 0) {
			va_end(arg);
			return;
		}
		va_end(arg);

		/* remove '\n' from the end of the line */
		/* '\n' is added by out_log */
		message[strlen(message)-1] = '\0';
		int log_level = 0;
		switch (level)
		{
		case CORE_LOG_LEVEL_WARNING:
			log_level = 2;
			break;

		case CORE_LOG_LEVEL_NOTICE:
			log_level = 3;
			break;

		case CORE_LOG_LEVEL_INFO:
		case CORE_LOG_LEVEL_DEBUG:
			log_level = 4;
			break;
		
		case CORE_LOG_LEVEL_FATAL:
		case CORE_LOG_LEVEL_ERROR:
		default: /* icnlude CORE_LOG_LEVEL_ALWAYS */
			log_level = 1;
			break;
		}
		out_log(base_file_name, line_no, function_name, log_level, "%s",
			message);
	}
}
