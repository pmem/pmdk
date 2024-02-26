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

// #define CORE_LOG_UT_MAX_BUFF_SIZE 4096

FUNC_MOCK(snprintf, int, char *__restrict __s, size_t __maxlen,
		const char *__restrict __format, ...)
/* file info */
	FUNC_MOCK_RUN(0) {
		va_list arg;
		va_start(arg, __format);
		UT_ASSERTeq(strcmp("%s: %3d: %s: ", __format), 0);
		strncpy(__s, "DUMMY FILE_INFO", __maxlen);
		return 10;
		va_end(arg);
	}
/* get time prefix */
	FUNC_MOCK_RUN(1) {
		va_list arg;
		va_start(arg, __format);
		UT_ASSERTeq(strcmp("%s.%06ld ", __format), 0);
		strncpy(__s, "DUMMY TIMESTAMP ", __maxlen);
		va_end(arg);
	}
	FUNC_MOCK_RUN(2) {
		va_list arg;
		va_start(arg, __format);
		UT_OUT("%s", __format);
	//	UT_ASSERTeq(strcmp("%s[%ld] %s%s%s\n", __format), 0);
		return vsnprintf(__s, __maxlen, __format, arg);
		va_end(arg);

	}
FUNC_MOCK_RUN_DEFAULT {
	va_list arg;
	va_start(arg, __format);
	UT_OUT("%d", RCOUNTER(snprintf));
	return vsnprintf(__s, __maxlen, __format, arg);
	va_end(arg);
}
FUNC_MOCK_END

static int Syslog_no_of_calls;
static char Syslog_log_level_name[9] = "";
static const char *Syslog_message;

FUNC_MOCK(syslog, void, int __pri, const char *__fmt, ...)
FUNC_MOCK_RUN_DEFAULT {
	va_list arg;
	va_start(arg, __fmt);
	UT_ASSERTeq(__pri, LOG_ERR);
	UT_ASSERTeq(strcmp("%s%s%s", __fmt), 0);
	char *log_level_name = va_arg(arg, char *);
	UT_ASSERTeq(strncmp(Syslog_log_level_name, &log_level_name[1],
		strlen(Syslog_log_level_name)), 0);
	char *file_info = va_arg(arg, char *);
	UT_ASSERTeq(strcmp("DUMMY FILE_INFO", file_info), 0);
	char *message = va_arg(arg, char *);
	UT_ASSERTeq(strcmp(Syslog_message, message), 0);
	va_end(arg);
	Syslog_no_of_calls ++;
}
FUNC_MOCK_END

#define CORE_LOG_UT_MESSAGE "Test message"

#if 0
#define CORE_LOG_UT_MESSAGE_LONG \
"Test message long 20Test message long 40Test message long 60" \
"Test message long 80Test message long100Test message long120" \
"Test message long140Test message long160Test message long180" \
"Test message long200Test message long220Test message long240" \
"Test message long260Test message long280Test message long300"

#define CORE_LOG_UT_MESSAGE_TOO_LONG CORE_LOG_UT_MESSAGE_LONG \
"Test message long 321"
#endif

/* tests */

#define TEST_SETUP(message_to_test) \
	Syslog_message = message_to_test;

#define TEST_STEP_SETUP(level) \
	Syslog_no_of_calls = 0; \
	strcpy(Syslog_log_level_name, #level)

#define TEST_STEP_CHECK() \
	UT_ASSERTeq(Syslog_no_of_calls, 1)

#define TEST_STEP(LEVEL, MESSAGE) \
	TEST_STEP_SETUP(LEVEL); \
	core_log_default_function(NULL, CORE_LOG_LEVEL_ERROR, \
	"file name", 123, "function_name", MESSAGE); \
	TEST_STEP_CHECK()

/* basic tests with a normal message */
static int
test_1(const struct test_case *tc, int argc, char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE);
	TEST_STEP(ERROR, CORE_LOG_UT_MESSAGE);
	return NO_ARGS_CONSUMED;
}

/* test to check that core_log_to_last() works w/ every thresholds */
static int
test_CORE_LOG_TRESHOLD(const struct test_case *tc, int argc, char *argv[])
{
	// core_log_set_function(CORE_LOG_USE_DEFAULT_FUNCTION, NULL);
	TEST_SETUP(CORE_LOG_UT_MESSAGE);
	for (enum core_log_level level = CORE_LOG_LEVEL_HARK;
				level < CORE_LOG_LEVEL_MAX; level ++) {
		core_log_set_threshold(CORE_LOG_THRESHOLD, level);
		/* must be in one line for proper __LINE__ value */
	}
	return NO_ARGS_CONSUMED;
}

static struct test_case test_cases[] = {
	TEST_CASE(test_1),
	TEST_CASE(test_CORE_LOG_TRESHOLD),
};

/* force the test to use the default logging function */
#undef LOG_SET_PMEMCORE_FUNC
#define LOG_SET_PMEMCORE_FUNC

int
main(int argc, char *argv[])
{
	START(argc, argv, "core_log_default_function");

	TEST_CASE_PROCESS(argc, argv, test_cases, ARRAY_SIZE(test_cases));

	DONE(NULL);
}
