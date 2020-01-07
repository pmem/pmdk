// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2017, Intel Corporation */

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

int
main(int argc, char *argv[])
{
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
