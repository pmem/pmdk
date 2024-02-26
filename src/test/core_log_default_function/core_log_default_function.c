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
#define FILE_INFO_ERROR "[file info error]: "
#define FUNCTION_NAME "dummy_func()"
#define FILE_INFO FILE_NAME ": 123: " FUNCTION_NAME ": "
#define LINE_NO 1357

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

const char *Log_level_name;
const char *File_info;
const char *Message;

char *Strchr_ret;

FUNC_MOCK(strrchr, char *, const char *__s, int __c)
FUNC_MOCK_RUN_DEFAULT {
	UT_ASSERTeq(__c, '/');
	return Strchr_ret;
}
FUNC_MOCK_END

static struct {
	const char *file_name;
	const char *function_name;
	int force_error;
	int ret;
} Snprintf_context;

FUNC_MOCK(snprintf, int, char *__restrict __s, size_t __maxlen,
		const char *__restrict __format, ...)
/* file info */
	FUNC_MOCK_RUN(0) {
/* second parameter after __format equal to -1 causes negative value return */
		va_list arg;
		va_start(arg, __format);
		UT_ASSERTstreq(__format, "%s: %3d: %s: ");
		char *file_name = va_arg(arg, char *);
		UT_ASSERTstreq(file_name, Snprintf_context.file_name);
		int line_no = va_arg(arg, int);
		UT_ASSERTeq(line_no, LINE_NO);
		char *function_name = va_arg(arg, char *);
		UT_ASSERTstreq(function_name, Snprintf_context.function_name);
		/* fill-in the given buffer to verify no memory overwritten */
		memset(__s, 'x', __maxlen);
		*(__s + __maxlen - 1) = '\0';
		memcpy(__s, FILE_INFO, sizeof(FILE_INFO) + 1);
		va_end(arg);
		if (Snprintf_context.ret != 0)
			return Snprintf_context.ret;
		else
			return __maxlen;
	}
/* get time prefix */
	FUNC_MOCK_RUN(1) {
		UT_ASSERTstreq(__format, "%s.%06ld ");
		if (Snprintf_context.force_error) {
		/* fill-in the given buffer to verify no memory overwritten */
			memset(__s, 'x', __maxlen);
		}
		else
			strncpy(__s, TIMESTAMP, __maxlen);
		return __maxlen;
	}
FUNC_MOCK_RUN_DEFAULT {
	UT_FATAL("Unexpected #%d sprintf: %s", RCOUNTER(snprintf), __format);
}
FUNC_MOCK_END

static struct {
	int no_of_calls;
	int __pri;
	char *file_info;
} Syslog_context;

FUNC_MOCK(syslog, void, int __pri, const char *__fmt, ...)
FUNC_MOCK_RUN_DEFAULT {
	Syslog_context.no_of_calls++;
	UT_ASSERTeq(__pri, Syslog_context.__pri);
	UT_ASSERTstreq(__fmt, "%s%s%s");
	va_list arg;
	va_start(arg, __fmt);
	char *log_level_name = va_arg(arg, char *);
	UT_ASSERTstreq(log_level_name, Log_level_name);
	char *file_info = va_arg(arg, char *);
	UT_ASSERTstreq(file_info, Syslog_context.file_info);
	char *message = va_arg(arg, char *);
	UT_ASSERTstreq(message, Message);
	va_end(arg);
}
FUNC_MOCK_END

static struct {
	int no_of_calls;
	char *times_stamp;
	const char *file_info;
} Fprintf_context;

FUNC_MOCK(fprintf, int, FILE *__restrict __stream, const char *__restrict __fmt,
	...)
FUNC_MOCK_RUN_DEFAULT {
	Fprintf_context.no_of_calls++;
	UT_ASSERTeq(__stream, stderr);
	va_list arg;
	va_start(arg, __fmt);
	char *times_tamp = va_arg(arg, char *);
	UT_ASSERTstreq(times_tamp, Fprintf_context.times_stamp);
	va_arg(arg, int); /* skip syscall(SYS_gettid) */
	char *log_level_name = va_arg(arg, char *);
	UT_ASSERTstreq(log_level_name, Log_level_name);
	char *file_info = va_arg(arg, char *);
	UT_ASSERTstreq(file_info, Syslog_context.file_info);
	char *message = va_arg(arg, char *);
	UT_ASSERTstreq(message, Message);
	va_end(arg);
	return 0;
}
FUNC_MOCK_END

/* Testshelpers */
#define TEST_SETUP(_MESSAGE) Message = _MESSAGE

#define TEST_STEP_SETUP(_LEVEL, _FILE_NAME_SHORT, _FUNCTION_NAME) \
	do { \
		FUNC_MOCK_RCOUNTER_SET(snprintf, 0); \
		Log_level_name = log_level_names[_LEVEL]; \
		memset(&Syslog_context, 0, sizeof(Syslog_context)); \
		Syslog_context.__pri = log_level_syslog_severity[_LEVEL]; \
		Syslog_context.file_info = FILE_INFO; \
		memset(&Snprintf_context, 0, sizeof(Snprintf_context)); \
		Snprintf_context.file_name = _FILE_NAME_SHORT; \
		Snprintf_context.function_name = _FUNCTION_NAME; \
		memset(&Fprintf_context, 0, sizeof(Fprintf_context)); \
		Fprintf_context.times_stamp = TIMESTAMP; \
		Strchr_ret = "/"_FILE_NAME_SHORT; \
	} while (0)

#define TEST_STEP_CHECK(_FPRINTF_CALLED) \
	do { \
		UT_ASSERTeq(Syslog_context.no_of_calls, 1); \
		UT_ASSERTeq(Fprintf_context.no_of_calls, _FPRINTF_CALLED); \
	} while (0)

/* basic test with a normal message pass through */
static int
test_default_function(const struct test_case *tc, int argc, char *argv[])
{
	TEST_SETUP(MESSAGE);
	for (enum core_log_level treshold = CORE_LOG_LEVEL_HARK;
		treshold < CORE_LOG_LEVEL_MAX; treshold++) {
		core_log_set_threshold(CORE_LOG_THRESHOLD_AUX, treshold);
		for (enum core_log_level level = CORE_LOG_LEVEL_HARK;
			level < CORE_LOG_LEVEL_MAX; level++) {
			TEST_STEP_SETUP(level, FILE_NAME, FUNCTION_NAME);
			core_log_default_function(NULL, level, FILE_NAME_W_PATH,
				LINE_NO, FUNCTION_NAME, MESSAGE);
			if (level == CORE_LOG_LEVEL_HARK || level > treshold)
				TEST_STEP_CHECK(0);
			else
				TEST_STEP_CHECK(1);
		}
	}

	return NO_ARGS_CONSUMED;
}

/* test to check that information about a bad file is printed */
static int
test_default_function_bad_file_name(const struct test_case *tc, int argc,
	char *argv[])
{
	core_log_set_threshold(CORE_LOG_THRESHOLD_AUX, CORE_LOG_LEVEL_DEBUG);
	TEST_SETUP(MESSAGE);
	TEST_STEP_SETUP(CORE_LOG_LEVEL_DEBUG, FILE_INFO_ERROR, FUNCTION_NAME);
	Snprintf_context.ret = -1;
	Syslog_context.file_info = FILE_INFO_ERROR;
	core_log_default_function(NULL, CORE_LOG_LEVEL_DEBUG, FILE_NAME_W_PATH,
		LINE_NO, FUNCTION_NAME, MESSAGE);
	TEST_STEP_CHECK(1);

	return NO_ARGS_CONSUMED;
}

/* test to check that short file name (w/o path) is properly printed */
static int
test_default_function_short_file_name(const struct test_case *tc, int argc,
	char *argv[])
{
	core_log_set_threshold(CORE_LOG_THRESHOLD_AUX, CORE_LOG_LEVEL_DEBUG);
	TEST_SETUP(MESSAGE);
	TEST_STEP_SETUP(CORE_LOG_LEVEL_DEBUG, FILE_NAME, FUNCTION_NAME);
	Strchr_ret = NULL;
	core_log_default_function(NULL, CORE_LOG_LEVEL_DEBUG, FILE_NAME,
		LINE_NO, FUNCTION_NAME, MESSAGE);
	TEST_STEP_CHECK(1);

	return NO_ARGS_CONSUMED;
}

/* test to check no fileinfo when filename is NULL */
static int
test_default_function_no_file_name(const struct test_case *tc, int argc,
	char *argv[])
{
	core_log_set_threshold(CORE_LOG_THRESHOLD_AUX, CORE_LOG_LEVEL_DEBUG);
	TEST_SETUP(MESSAGE);
	TEST_STEP_SETUP(CORE_LOG_LEVEL_DEBUG, "", FUNCTION_NAME);
	/* skip file_info snprintf() */
	FUNC_MOCK_RCOUNTER_SET(snprintf, 1);
	Syslog_context.file_info = "";
	core_log_default_function(NULL, CORE_LOG_LEVEL_DEBUG, NULL,
		LINE_NO, FUNCTION_NAME, MESSAGE);
	TEST_STEP_CHECK(1);

	return NO_ARGS_CONSUMED;
}

/* test to check timestamp error */
static int
test_default_function_bad_timestamp(const struct test_case *tc, int argc,
	char *argv[])
{
	core_log_set_threshold(CORE_LOG_THRESHOLD_AUX, CORE_LOG_LEVEL_DEBUG);
	TEST_SETUP(MESSAGE);
	TEST_STEP_SETUP(CORE_LOG_LEVEL_DEBUG, FILE_NAME, FUNCTION_NAME);
	Snprintf_context.force_error = 1; /* fail the file_info snprintf() */
	Fprintf_context.times_stamp = "[time error] ";
	core_log_default_function(NULL, CORE_LOG_LEVEL_DEBUG, FILE_NAME,
		LINE_NO, FUNCTION_NAME, MESSAGE);
	TEST_STEP_CHECK(1);

	return NO_ARGS_CONSUMED;
}

static struct test_case test_cases[] = {
	TEST_CASE(test_default_function),
	TEST_CASE(test_default_function_bad_file_name),
	TEST_CASE(test_default_function_short_file_name),
	TEST_CASE(test_default_function_no_file_name),
	TEST_CASE(test_default_function_bad_timestamp),
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "core_log_default_function");
	TEST_CASE_PROCESS(argc, argv, test_cases, ARRAY_SIZE(test_cases));
	DONE(NULL);
}
