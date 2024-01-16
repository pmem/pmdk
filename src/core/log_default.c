// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2024, Intel Corporation */

/*
 * log_default.c -- the default logging function with support for logging either
 * to syslog or to stderr
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <stdarg.h>
#include <syslog.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include "log_internal.h"
#include "log_default.h"
#include "os.h"

static const char log_level_names[6][9] = {
	[CORE_LOG_LEVEL_FATAL]		= "*FATAL* ",
	[CORE_LOG_LEVEL_ERROR]		= "*ERROR* ",
	[CORE_LOG_LEVEL_WARNING]	= "*WARN*  ",
	[CORE_LOG_LEVEL_NOTICE]		= "*NOTE*  ",
	[CORE_LOG_LEVEL_INFO]		= "*INFO*  ",
	[CORE_LOG_LEVEL_DEBUG]		= "*DEBUG* ",
};

static const int log_level_syslog_severity[] = {
	[CORE_LOG_LEVEL_FATAL]		= LOG_CRIT,
	[CORE_LOG_LEVEL_ERROR]		= LOG_ERR,
	[CORE_LOG_LEVEL_WARNING]	= LOG_WARNING,
	[CORE_LOG_LEVEL_NOTICE]		= LOG_NOTICE,
	[CORE_LOG_LEVEL_INFO]		= LOG_INFO,
	[CORE_LOG_LEVEL_DEBUG]		= LOG_DEBUG,
};

/*
 * get_timestamp_prefix -- provide actual time in a readable string
 *
 * NOTE
 * This function is static now, so we know all possible calls of snprintf()
 * and we conclude it can not fail.
 *
 * ASSUMPTIONS:
 * - buf != NULL && buf_size >= 16
 */
static void
get_timestamp_prefix(char *buf, size_t buf_size)
{
	struct tm info;
	char date[24];
	struct timespec ts;
	long usec;

	const char error_message[] = "[time error] ";

	if (os_clock_gettime(CLOCK_REALTIME, &ts))
		goto err_message;

	if (NULL == localtime_r(&ts.tv_sec, &info))
		goto err_message;

	usec = ts.tv_nsec / 1000;
	if (!strftime(date, sizeof(date), "%b %d %H:%M:%S", &info))
		goto err_message;

	/* it cannot fail - please see the note above */
	(void) snprintf(buf, buf_size, "%s.%06ld ", date, usec);
	if (strnlen(buf, buf_size) == buf_size)
		goto err_message;

	return;

err_message:
	memcpy(buf, error_message, sizeof(error_message));
}

/*
 * core_log_default_function -- default logging function used to log a message
 * to syslog and/or stderr
 *
 * The message is started with prefix composed from file, line, func parameters followed by string
 * pointed by format. If format includes format specifiers (subsequences beginning with %),
 * the additional arguments following format are formatted and inserted in the message.
 *
 * ASSUMPTIONS:
 * - level >= CORE_LOG_LEVEL_FATAL && level <= CORE_LOG_LEVEL_DEBUG
 * - level <= Core_log_threshold[LOG_THRESHOLD]
 * - file == NULL || (file != NULL && function != NULL)
 */
void
core_log_default_function(enum core_log_level level, const char *file_name, const int line_no,
	const char *function_name, const char *message_format, ...)
{
	char file_info_buffer[256] = "";
	const char *file_info = file_info_buffer;
	char message[1024] = "";
	const char file_info_error[] = "[file info error]: ";

	if (CORE_LOG_DISABLED == level)
		return;

	va_list arg;
	va_start(arg, message_format);
	if (vsnprintf(message, sizeof(message), message_format, arg) < 0) {
		va_end(arg);
		return;
	}
	va_end(arg);

	if (file_name) {
		/* extract base_file_name */
		const char *base_file_name = strrchr(file_name, '/');
		if (!base_file_name)
			base_file_name = file_name;
		else
			/* skip '/' */
			base_file_name++;

		if (snprintf(file_info_buffer, sizeof(file_info_buffer),
				"%s: %3d: %s: ", base_file_name, line_no,
				function_name) < 0) {
			file_info = file_info_error;
		}
	}

	if (level <= Core_log_threshold[CORE_LOG_THRESHOLD_AUX] ||
	    level == CORE_LOG_LEVEL_ALWAYS) {
		char times_tamp[45] = "";
		get_timestamp_prefix(times_tamp, sizeof(times_tamp));
		(void) fprintf(stderr, "%s[%ld] %s%s%s", times_tamp, syscall(SYS_gettid),
			log_level_names[(level == CORE_LOG_LEVEL_ALWAYS) ?
						CORE_LOG_LEVEL_DEBUG : level],
			file_info, message);
	}

	/* do not log to syslog in case of CORE_LOG_LEVEL_ALWAYS */
	if (CORE_LOG_LEVEL_ALWAYS == level)
		return;

	/* assumed: level <= Core_log_threshold[CORE_LOG_THRESHOLD] */
	syslog(log_level_syslog_severity[level], "%s%s%s",
		log_level_names[level], file_info, message);
}

/*
 * core_log_default_init -- open a connection to the system logger
 */
void
core_log_default_init(const char *prefix)
{
	openlog(prefix, LOG_PID, LOG_LOCAL7);
}


/*
 * core_log_default_fini -- close the descriptor being used to write to the system logger
 */
void
core_log_default_fini(void)
{
	closelog();
}
