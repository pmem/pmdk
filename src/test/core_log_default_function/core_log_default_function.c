// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * core_log_default_function.c -- unit test for core_log_default_function
 */

#undef _GNU_SOURCE
#include <string.h>
#include <syslog.h>
#include <stdbool.h>

#include "unittest.h"
#include "log_internal.h"
#include "log_default.h"

#define NO_ARGS_CONSUMED 0

#define MESSAGE_MOCK ((char *)0x24689753)
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

static struct {
	const char *exp_log_level_name;
	const char *exp_file_info;
} Common;

static char *Strchr_ret;

FUNC_MOCK(strrchr, char *, const char *__s, int __c)
FUNC_MOCK_RUN_DEFAULT {
	UT_ASSERTeq(__c, '/');
	return Strchr_ret;
}
FUNC_MOCK_END

static bool Os_clock_gettime_force_error;

FUNC_MOCK(clock_gettime, int, clockid_t __clock_id, struct timespec *__tp)
FUNC_MOCK_RUN_DEFAULT {
	if (Os_clock_gettime_force_error)
		return -1;
	return _FUNC_REAL(clock_gettime)(__clock_id, __tp);
}
FUNC_MOCK_END

static struct {
	const char *exp_file_name;
	int ret;
} Snprintf;

FUNC_MOCK(snprintf, int, char *__restrict __s, size_t __maxlen,
		const char *__restrict __format, ...)
/* file info */
	FUNC_MOCK_RUN(0) {
		va_list arg;
		va_start(arg, __format);
		UT_ASSERTstreq(__format, "%s: %3u: %s: ");
		char *file_name = va_arg(arg, char *);
		UT_ASSERTstreq(file_name, Snprintf.exp_file_name);
		unsigned line_no = va_arg(arg, unsigned);
		UT_ASSERTeq(line_no, LINE_NO);
		char *function_name = va_arg(arg, char *);
		UT_ASSERTstreq(function_name, FUNCTION_NAME);
		/* Can we access the whole given buffer (*__s) */
		*(__s + __maxlen - 1) = '\0';
		va_end(arg);

		if (Snprintf.ret != 0)
			return Snprintf.ret;
		UT_ASSERT(sizeof(FILE_INFO) <= __maxlen);
		strncpy(__s, FILE_INFO, __maxlen);
		return sizeof(FILE_INFO) - 1;
	}
/* get time prefix */
	FUNC_MOCK_RUN(1) {
		UT_ASSERTstreq(__format, "%s.%06ld ");
		UT_ASSERT(sizeof(TIMESTAMP) <= __maxlen);
		strncpy(__s, TIMESTAMP, __maxlen);
		return sizeof(TIMESTAMP) - 1;
	}
FUNC_MOCK_RUN_DEFAULT {
	UT_FATAL("Unexpected #%d sprintf: %s", RCOUNTER(snprintf), __format);
}
FUNC_MOCK_END

static struct {
	int exp__pri;
} Syslog;

FUNC_MOCK(syslog, void, int __pri, const char *__fmt, ...)
FUNC_MOCK_RUN_DEFAULT {
	UT_ASSERTeq(__pri, Syslog.exp__pri);
	UT_ASSERTstreq(__fmt, "%s%s%s");
	va_list arg;
	va_start(arg, __fmt);
	char *log_level_name = va_arg(arg, char *);
	UT_ASSERTstreq(log_level_name, Common.exp_log_level_name);
	char *file_info = va_arg(arg, char *);
	UT_ASSERTstreq(file_info, Common.exp_file_info);
	char *message = va_arg(arg, char *);
	UT_ASSERTeq(message, MESSAGE_MOCK);
	va_end(arg);
}
FUNC_MOCK_END

static struct {
	char *exp_times_stamp;
} Fprintf;

FUNC_MOCK(fprintf, int, FILE *__restrict __stream, const char *__restrict __fmt,
	...)
FUNC_MOCK_RUN_DEFAULT {
	UT_ASSERTeq(__stream, stderr);
	UT_ASSERTstreq(__fmt, "%s[%ld] %s%s%s\n");
	va_list arg;
	va_start(arg, __fmt);
	char *times_tamp = va_arg(arg, char *);
	UT_ASSERTstreq(times_tamp, Fprintf.exp_times_stamp);
	va_arg(arg, int); /* skip syscall(SYS_gettid) */
	char *log_level_name = va_arg(arg, char *);
	UT_ASSERTstreq(log_level_name, Common.exp_log_level_name);
	char *file_info = va_arg(arg, char *);
	UT_ASSERTstreq(file_info, Common.exp_file_info);
	char *message = va_arg(arg, char *);
	UT_ASSERTeq(message, MESSAGE_MOCK);
	va_end(arg);
	return 0;
}
FUNC_MOCK_END

/* Tests' helpers */
#define TEST_SETUP() core_log_set_threshold(CORE_LOG_THRESHOLD_AUX, \
		CORE_LOG_LEVEL_DEBUG)

#define TEST_STEP_SETUP(_LEVEL, _FILE_NAME_SHORT) \
	do { \
		FUNC_MOCK_RCOUNTER_SET(snprintf, 0); \
		FUNC_MOCK_RCOUNTER_SET(syslog, 0); \
		FUNC_MOCK_RCOUNTER_SET(fprintf, 0); \
		Strchr_ret = "/"_FILE_NAME_SHORT; \
		Common.exp_log_level_name = log_level_names[_LEVEL]; \
		Common.exp_file_info = FILE_INFO; \
		Snprintf.exp_file_name = _FILE_NAME_SHORT; \
		Snprintf.ret = 0; \
		Syslog.exp__pri = log_level_syslog_severity[_LEVEL]; \
		Fprintf.exp_times_stamp = TIMESTAMP; \
	} while (0)

#define TEST_STEP_CHECK(_SNPRINTF, _FPRINTF) \
	do { \
		UT_ASSERTeq(RCOUNTER(syslog), 1); \
		UT_ASSERTeq(RCOUNTER(snprintf), _SNPRINTF); \
		UT_ASSERTeq(RCOUNTER(fprintf), _FPRINTF); \
	} while (0)

/* basic test with a normal message pass through */
static int
test_default_function(const struct test_case *tc, int argc, char *argv[])
{
	TEST_SETUP();
	for (enum core_log_level treshold = CORE_LOG_LEVEL_HARK;
		treshold < CORE_LOG_LEVEL_MAX; treshold++) {
		core_log_set_threshold(CORE_LOG_THRESHOLD_AUX, treshold);
		for (enum core_log_level level = CORE_LOG_LEVEL_HARK;
			level < CORE_LOG_LEVEL_MAX; level++) {
			TEST_STEP_SETUP(level, FILE_NAME);
			core_log_default_function(level, FILE_NAME_W_PATH,
				LINE_NO, FUNCTION_NAME, MESSAGE_MOCK);
			if (level == CORE_LOG_LEVEL_HARK || level > treshold)
				TEST_STEP_CHECK(1, 0);
			else
				TEST_STEP_CHECK(2, 1);
		}
	}

	return NO_ARGS_CONSUMED;
}

/* test to check that information about a bad file is printed */
static int
test_default_function_bad_file_name(const struct test_case *tc, int argc,
	char *argv[])
{
	TEST_SETUP();
	TEST_STEP_SETUP(CORE_LOG_LEVEL_DEBUG, FILE_INFO_ERROR);
	Snprintf.ret = -1;
	Common.exp_file_info = FILE_INFO_ERROR;
	core_log_default_function(CORE_LOG_LEVEL_DEBUG, FILE_NAME_W_PATH,
		LINE_NO, FUNCTION_NAME, MESSAGE_MOCK);
	TEST_STEP_CHECK(2, 1);

	return NO_ARGS_CONSUMED;
}

/* test to check that short file name (w/o path) is properly printed */
static int
test_default_function_short_file_name(const struct test_case *tc, int argc,
	char *argv[])
{
	core_log_set_threshold(CORE_LOG_THRESHOLD_AUX, CORE_LOG_LEVEL_DEBUG);
	TEST_SETUP();
	TEST_STEP_SETUP(CORE_LOG_LEVEL_DEBUG, FILE_NAME);
	Strchr_ret = NULL;
	core_log_default_function(CORE_LOG_LEVEL_DEBUG, FILE_NAME,
		LINE_NO, FUNCTION_NAME, MESSAGE_MOCK);
	TEST_STEP_CHECK(2, 1);

	return NO_ARGS_CONSUMED;
}

/* test to check no fileinfo when file_name is NULL */
static int
test_default_function_no_file_name(const struct test_case *tc, int argc,
	char *argv[])
{
	TEST_SETUP();
	TEST_STEP_SETUP(CORE_LOG_LEVEL_DEBUG, "");
	FUNC_MOCK_RCOUNTER_SET(snprintf, 1); /* skip file_info snprintf() */
	Common.exp_file_info = "";
	core_log_default_function(CORE_LOG_LEVEL_DEBUG, NULL,
		LINE_NO, FUNCTION_NAME, MESSAGE_MOCK);
	TEST_STEP_CHECK(2, 1);

	return NO_ARGS_CONSUMED;
}

/* test to check no fileinfo when file_name and function_name are NULL */
static int
test_default_function_no_function_name(const struct test_case *tc, int argc,
	char *argv[])
{
	TEST_SETUP();
	TEST_STEP_SETUP(CORE_LOG_LEVEL_DEBUG, "");
	FUNC_MOCK_RCOUNTER_SET(snprintf, 1); /* skip file_info snprintf() */
	Common.exp_file_info = "";
	core_log_default_function(CORE_LOG_LEVEL_DEBUG, NULL, LINE_NO, NULL,
		MESSAGE_MOCK);
	TEST_STEP_CHECK(2, 1);

	return NO_ARGS_CONSUMED;
}

/* test to check timestamp error */
static int
test_default_function_bad_timestamp(const struct test_case *tc, int argc,
	char *argv[])
{
	TEST_SETUP();
	TEST_STEP_SETUP(CORE_LOG_LEVEL_DEBUG, FILE_NAME);
	Os_clock_gettime_force_error = true; /* fail the file_info snprintf() */
	Fprintf.exp_times_stamp = "[time error] ";
	core_log_default_function(CORE_LOG_LEVEL_DEBUG, FILE_NAME, LINE_NO,
		FUNCTION_NAME, MESSAGE_MOCK);
	TEST_STEP_CHECK(1, 1);

	return NO_ARGS_CONSUMED;
}

static struct test_case test_cases[] = {
	TEST_CASE(test_default_function),
	TEST_CASE(test_default_function_bad_file_name),
	TEST_CASE(test_default_function_short_file_name),
	TEST_CASE(test_default_function_no_file_name),
	TEST_CASE(test_default_function_no_function_name),
	TEST_CASE(test_default_function_bad_timestamp),
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "core_log_default_function");
	TEST_CASE_PROCESS(argc, argv, test_cases, ARRAY_SIZE(test_cases));
	DONE(NULL);
}
