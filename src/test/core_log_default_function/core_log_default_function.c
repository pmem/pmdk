// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * core_log_default_function.c -- unit test for core_log_default_function
 */

#undef _GNU_SOURCE
#include <string.h>
#include <syslog.h>

#include "unittest.h"
#include "log_internal.h"
#include "log_default.h"

#define NO_ARGS_CONSUMED 0

#define MESSAGE "Test message"
#define TIMESTAMP "DUMMY TIMESTAMP"
#define FILE_NAME "dummy.c"
#define FILE_NAME_W_PATH "dummy_path/dummy_path/" FILE_NAME
#define FILE_NAME_ERROR "[file info error]: "
#define FUNCTION_NAME "dummy_func()"

static const char log_level_names[CORE_LOG_LEVEL_MAX][9] = {
	[CORE_LOG_LEVEL_HARK]		= "*HARK*  ",
	[CORE_LOG_LEVEL_FATAL]		= "*FATAL* ",
	[CORE_LOG_LEVEL_ERROR]		= "*ERROR* ",
	[CORE_LOG_LEVEL_WARNING]	= "*WARN*  ",
	[CORE_LOG_LEVEL_NOTICE]		= "*NOTE*  ",
	[CORE_LOG_LEVEL_INFO]		= "*INFO*  ",
	[CORE_LOG_LEVEL_DEBUG]		= "*DEBUG* ",
};

static const int log_level_syslog_severity[] = {
	[CORE_LOG_LEVEL_HARK]		= LOG_NOTICE,
	[CORE_LOG_LEVEL_FATAL]		= LOG_CRIT,
	[CORE_LOG_LEVEL_ERROR]		= LOG_ERR,
	[CORE_LOG_LEVEL_WARNING]	= LOG_WARNING,
	[CORE_LOG_LEVEL_NOTICE]		= LOG_NOTICE,
	[CORE_LOG_LEVEL_INFO]		= LOG_INFO,
	[CORE_LOG_LEVEL_DEBUG]		= LOG_DEBUG,
};

static struct {
	char *ret;
}Strchr_context;

FUNC_MOCK(strrchr, char *, const char *__s, int __c)
FUNC_MOCK_RUN_DEFAULT {
	UT_ASSERTeq(__c, '/');
	return Strchr_context.ret;
}
FUNC_MOCK_END

static struct {
	const char *file_name;
	int line_no;
	const char *function_name;
	char *file_info_buffer;
	int forceError;
}Snprintf_context;

FUNC_MOCK(snprintf, int, char *__restrict __s, size_t __maxlen,
		const char *__restrict __format, ...)
/* file info */
	FUNC_MOCK_RUN(0) {
/* second parameter after __format equal to -1 causes negative value return */
		va_list arg;
		va_start(arg, __format);
		UT_ASSERTstreq(__format, "%s: %3d: %s:  ");
		char *file_name = va_arg(arg, char *);
		UT_ASSERTstreq(file_name, Snprintf_context.file_name);
		int line_no = va_arg(arg, int);
		if (line_no >= 0)
			UT_ASSERTeq(line_no, Snprintf_context.line_no);
		else
			return -1;
		char *function_name = va_arg(arg, char *);
		UT_ASSERTstreq(function_name, Snprintf_context.function_name);
		if (Snprintf_context.file_info_buffer)
			strncpy(__s, Snprintf_context.file_info_buffer,
				__maxlen);
		va_end(arg);
		return __maxlen;
	}
/* get time prefix */
	FUNC_MOCK_RUN(1) {
		UT_ASSERTstreq(__format, "%s.%06ld ");
		if (Snprintf_context.forceError) {
			memset(__s, 'x', __maxlen);
		}
		else
			strncpy(__s, TIMESTAMP, __maxlen);
		return __maxlen;
	}
FUNC_MOCK_RUN_DEFAULT {
	va_list arg;
	va_start(arg, __format);
	UT_OUT("sprintf - %d: %s", RCOUNTER(snprintf), __format);
	UT_FATAL("Unknown sprintf");
	va_end(arg);
}
FUNC_MOCK_END

static struct {
	int no_of_calls;
	int __pri;
	const char *log_level_name;
	char *file_info;
	const char *message;
}Syslog_context;

FUNC_MOCK(syslog, void, int __pri, const char *__fmt, ...)
FUNC_MOCK_RUN_DEFAULT {
	Syslog_context.no_of_calls++;
	UT_ASSERTeq(__pri, Syslog_context.__pri);
	UT_ASSERTeq(strcmp("%s%s%s", __fmt), 0);
	va_list arg;
	va_start(arg, __fmt);
	char *log_level_name = va_arg(arg, char *);
	UT_ASSERTstreq(log_level_name, Syslog_context.log_level_name);
	char *file_info = va_arg(arg, char *);
	if (Syslog_context.file_info)
		UT_ASSERTstreq(file_info, Syslog_context.file_info);
	char *message = va_arg(arg, char *);
	if (Syslog_context.message)
		UT_ASSERTstreq(message, Syslog_context.message);
	va_end(arg);
}
FUNC_MOCK_END

static struct {
	int no_of_calls;
	char *times_stamp;
} Fprintf_context;

FUNC_MOCK(fprintf, int, FILE *__restrict __stream, \
	const char *__restrict __fmt, ...)
FUNC_MOCK_RUN_DEFAULT {
	UT_ASSERTeq(__stream, stderr);
	Fprintf_context.no_of_calls++;
	if (Fprintf_context.times_stamp) {
		va_list arg;
		va_start(arg, __fmt);
		char *times_tamp = va_arg(arg, char *);
		UT_ASSERTstreq(times_tamp, Fprintf_context.times_stamp);
		va_end(arg);
	}
	return 0;
}
FUNC_MOCK_END

/* Tsts helpers */
static void
TEST_SETUP(const char *message)
{
	Syslog_context.message = message;
}

static void
TEST_STEP_SETUP(enum core_log_level LEVEL, const char *file_name, int line_no,
	const char *function_name)
{
	Syslog_context.no_of_calls = 0;
	Syslog_context.__pri = log_level_syslog_severity[LEVEL];
	Syslog_context.log_level_name = log_level_names[LEVEL];
	FUNC_MOCK_RCOUNTER_SET(snprintf, 0);
	Snprintf_context.file_name = file_name;
	Snprintf_context.line_no = line_no;
	Snprintf_context.function_name = function_name;
	Snprintf_context.forceError = 0;
	Fprintf_context.no_of_calls = 0;
	Fprintf_context.times_stamp = NULL;

}

#define TEST_STEP_CHECK(FPRINTF_CALLED) \
	do { \
		UT_ASSERTeq(Syslog_context.no_of_calls, 1); \
		UT_ASSERTeq(Fprintf_context.no_of_calls, FPRINTF_CALLED); \
	} while (0)

/* basic tests with a normal message pass through */
static int
testDefaultFunction(const struct test_case *tc, int argc, char *argv[])
{
	core_log_set_threshold(CORE_LOG_THRESHOLD_AUX, CORE_LOG_LEVEL_DEBUG);
	TEST_SETUP(MESSAGE);
	for (enum core_log_level treshold = CORE_LOG_LEVEL_HARK;
		treshold < CORE_LOG_LEVEL_MAX; treshold++) {
		core_log_set_threshold(CORE_LOG_THRESHOLD_AUX, treshold);
		for (enum core_log_level level = CORE_LOG_LEVEL_HARK;
			level < CORE_LOG_LEVEL_MAX; level++) {

			TEST_STEP_SETUP(level, FILE_NAME, 0, FUNCTION_NAME);
			core_log_default_function(NULL, level, FILE_NAME_W_PATH,
				0, FUNCTION_NAME, MESSAGE);
			TEST_STEP_CHECK(level == CORE_LOG_LEVEL_HARK? 0 :
					level > treshold? 0 : 1);
		}
	}
	return NO_ARGS_CONSUMED;
}

/* test to check that information about bad file is printed */
static int
testDefaultFunctionBadFileInfo(const struct test_case *tc, int argc,
	char *argv[])
{
	core_log_set_threshold(CORE_LOG_THRESHOLD_AUX, CORE_LOG_LEVEL_DEBUG);
	TEST_SETUP(MESSAGE);
	TEST_STEP_SETUP(CORE_LOG_LEVEL_DEBUG, FILE_NAME_ERROR, 0,
		FUNCTION_NAME);
	core_log_default_function(NULL, CORE_LOG_LEVEL_DEBUG, FILE_NAME_W_PATH,
		-1, FUNCTION_NAME, MESSAGE);
	TEST_STEP_CHECK(1);

	return NO_ARGS_CONSUMED;
}

/* test to check that short file name (w/o path) is properly printed */
static int
testDefaultFunctionShortFileName(const struct test_case *tc, int argc,
	char *argv[])
{
	core_log_set_threshold(CORE_LOG_THRESHOLD_AUX, CORE_LOG_LEVEL_DEBUG);
	TEST_SETUP(MESSAGE);
	TEST_STEP_SETUP(CORE_LOG_LEVEL_DEBUG, FILE_NAME, 1, FUNCTION_NAME);
	core_log_default_function(NULL, CORE_LOG_LEVEL_DEBUG, FILE_NAME, 1,
		FUNCTION_NAME, MESSAGE);
	TEST_STEP_CHECK(1);

	return NO_ARGS_CONSUMED;
}

/* test to check no fileinfo when filename is NULL */
static int
testDefaultFunctionNoFileName(const struct test_case *tc, int argc,
	char *argv[])
{
	core_log_set_threshold(CORE_LOG_THRESHOLD_AUX, CORE_LOG_LEVEL_DEBUG);
	TEST_SETUP(MESSAGE);
	TEST_STEP_SETUP(CORE_LOG_LEVEL_DEBUG, "", 1, FUNCTION_NAME);
	/* skip file_info snprintf() */
	FUNC_MOCK_RCOUNTER_SET(snprintf, 1);
	core_log_default_function(NULL, CORE_LOG_LEVEL_DEBUG, NULL, 1,
		FUNCTION_NAME, MESSAGE);
	TEST_STEP_CHECK(1);

	return NO_ARGS_CONSUMED;
}

/* test to check timestamp error */
static int
testDefaultFunctionBadTimestamp(const struct test_case *tc, int argc,
	char *argv[])
{
	core_log_set_threshold(CORE_LOG_THRESHOLD_AUX, CORE_LOG_LEVEL_DEBUG);
	TEST_SETUP(MESSAGE);
	TEST_STEP_SETUP(CORE_LOG_LEVEL_DEBUG, FILE_NAME, 1, FUNCTION_NAME);
	Snprintf_context.forceError = 1;
	Fprintf_context.times_stamp = "[time error] ";
	/* skip file_info snprintf() */
	core_log_default_function(NULL, CORE_LOG_LEVEL_DEBUG, FILE_NAME, 1,
		FUNCTION_NAME, MESSAGE);
	TEST_STEP_CHECK(1);

	return NO_ARGS_CONSUMED;
}

static struct test_case test_cases[] = {
	TEST_CASE(testDefaultFunction),
	TEST_CASE(testDefaultFunctionBadFileInfo),
	TEST_CASE(testDefaultFunctionShortFileName),
	TEST_CASE(testDefaultFunctionNoFileName),
	TEST_CASE(testDefaultFunctionBadTimestamp),
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "core_log_default_function");
	TEST_CASE_PROCESS(argc, argv, test_cases, ARRAY_SIZE(test_cases));
	DONE(NULL);
}
