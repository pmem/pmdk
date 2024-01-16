// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2024, Intel Corporation */

/*
 * traces_custom_function.c -- unit test for traces with custom print or
 * vsnprintf functions
 *
 * usage: traces_custom_function [v|p]
 *
 */

#define LOG_PREFIX "trace_func"
#define LOG_LEVEL_VAR "TRACE_LOG_LEVEL"
#define LOG_FILE_VAR "TRACE_LOG_FILE"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0

#include <sys/types.h>
#include <stdarg.h>
#include "pmemcommon.h"
#include "unittest.h"
#include "log_internal.h"
#include "out.h"

/*
 * print_custom_function -- Custom function to handle output
 *
 * This is called from the library to print text instead of output to stderr.
 */
static void
print_custom_function(const char *s)
{
	if (s) {
		UT_OUT("CUSTOM_PRINT: %s", s);
	} else {
		UT_OUT("CUSTOM_PRINT(NULL)");
	}
}

/*
 * vsnprintf_custom_function -- Custom vsnprintf implementation
 *
 * It modifies format by adding @@ in front of each conversion specification.
 */
static int
vsnprintf_custom_function(char *str, size_t size, const char *format,
		va_list ap)
{
	char *format2 = MALLOC(strlen(format) * 3);
	int i = 0;
	int ret_val;

	while (*format != '\0') {
		if (*format == '%') {
			format2[i++] = '@';
			format2[i++] = '@';
		}
		format2[i++] = *format++;
	}
	format2[i++] = '\0';

	ret_val = vsnprintf(str, size, format2, ap);
	FREE(format2);

	return ret_val;
}

static void
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

		out_log(base_file_name, line_no, function_name, 1, "%s",
			message);
	}
}

int
main(int argc, char *argv[])
{
	core_log_set_function(ut_log_function);

	START(argc, argv, "traces_custom_function");

	if (argc != 2)
		UT_FATAL("usage: %s [v|p]", argv[0]);

	out_set_print_func(print_custom_function);

	common_init(LOG_PREFIX, LOG_LEVEL_VAR, LOG_FILE_VAR,
			MAJOR_VERSION, MINOR_VERSION);

	switch (argv[1][0]) {
	case 'p': {
		LOG(0, "Log level NONE");
		LOG(1, "Log level ERROR");
		LOG(2, "Log level WARNING");
		LOG(3, "Log level INFO");
		LOG(4, "Log level DEBUG");
	}
		break;
	case 'v':
		out_set_vsnprintf_func(vsnprintf_custom_function);

		LOG(0, "no format");
		LOG(0, "pointer: %p", (void *)0x12345678);
		LOG(0, "string: %s", "Hello world!");
		LOG(0, "number: %u", 12345678);
		errno = EINVAL;
		LOG(0, "!error");
		break;
	default:
		UT_FATAL("usage: %s [v|p]", argv[0]);
	}

	/* Cleanup */
	common_fini();

	DONE(NULL);
}
