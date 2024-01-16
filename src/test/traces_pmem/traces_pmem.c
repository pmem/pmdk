// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2023, Intel Corporation */

/*
 * traces_pmem.c -- unit test traces for libraries pmem
 */

#include "unittest.h"

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
		if (vsnprintf(message, sizeof(message), message_format, arg) < 0) {
			va_end(arg);
			return;
		}
		va_end(arg);

		/* remove '\n' from the end of the line,
		as it is added by out_log */
		message[strlen(message)-1] = '\0';

		out_log(base_file_name, line_no, function_name, 1, message);
	}
}

int
main(int argc, char *argv[])
{
	core_log_set_function(ut_log_function);

	START(argc, argv, "traces_pmem");

	UT_ASSERT(!pmem_check_version(PMEM_MAJOR_VERSION,
				PMEM_MINOR_VERSION));
	UT_ASSERT(!pmemobj_check_version(PMEMOBJ_MAJOR_VERSION,
				PMEMOBJ_MINOR_VERSION));

	DONE(NULL);
}
