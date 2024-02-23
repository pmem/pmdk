// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * core_log_last.c -- unit test to CORE_LOG_...LAST
 */

#undef _GNU_SOURCE
#include <string.h>
#include <syslog.h>

#include "unittest.h"
#include "log_internal.h"
#include "last_error_msg.h"

#define NO_ARGS_CONSUMED 0

/* strerror_r mock */
#define CORE_LOG_UT_ERRNO_SHORT 1
#define CORE_LOG_UT_ERRNO_SHORT_STR "Short errno str"
#define CORE_LOG_UT_ERRNO_INVALID 2

static int Strerror_r_no_of_calls = 0;

FUNC_MOCK(__xpg_strerror_r, int, int __errnum, char *__buf, size_t __buflen)
FUNC_MOCK_RUN_DEFAULT {
	Strerror_r_no_of_calls ++;
	switch (__errnum)
	{
	case CORE_LOG_UT_ERRNO_SHORT:
		return snprintf(__buf, __buflen, CORE_LOG_UT_ERRNO_SHORT_STR);
		break;

	case CORE_LOG_UT_ERRNO_INVALID:
		return -1;
		break;

	default:
		break;
	}
	return 0;
}
FUNC_MOCK_END

static int Syslog_no_of_calls = 0;

FUNC_MOCK(syslog, void, int __pri, const char *__fmt, ...)
FUNC_MOCK_RUN_DEFAULT {
	char buf[1024];
	va_list arg;
	va_start(arg, __fmt);
	vsnprintf(buf, 1024, __fmt, arg);
	va_end(arg);
	// __real_syslog(__pri, buf);
	Syslog_no_of_calls ++;
}
FUNC_MOCK_END

/* core_log_to_last() mock */
struct core_log_to_last_moc_context {
	int initialized;
	const char *file_name;
	int line_no;
	const char *function_name;
	char message[1024];
	int no_of_calls;
};
struct core_log_to_last_moc_context Core_log_to_last_moc_context = {0};

FUNC_MOCK(core_log_to_last, void, int errnum, const char *file_name,
	int line_no, const char *function_name, const char *message_format, ...)
FUNC_MOCK_RUN_DEFAULT {
	char buf[4096];
	va_list arg;
	va_start(arg, message_format);
	vsnprintf(buf, 4096, message_format, arg);
	va_end(arg);
	Core_log_to_last_moc_context.no_of_calls ++;
	UT_ASSERTeq(
		strcmp(Core_log_to_last_moc_context.file_name, file_name), 0);
	UT_ASSERTeq(Core_log_to_last_moc_context.line_no, line_no);
	UT_ASSERTeq(strcmp(Core_log_to_last_moc_context.function_name,
		function_name), 0);

	__real_core_log_to_last(errnum, file_name, line_no, function_name, buf);
}
FUNC_MOCK_END

#define CORE_LOG_UT_MESSAGE "Test message"

#define CORE_LOG_UT_MESSAGE_LONG \
"Test message long 20Test message long 40Test message long 60" \
"Test message long 80Test message long100Test message long120" \
"Test message long140Test message long160Test message long180" \
"Test message long200Test message long220Test message long240" \
"Test message long260Test message long280Test message long300"

#define CORE_LOG_UT_MESSAGE_TOO_LONG CORE_LOG_UT_MESSAGE_LONG \
"Test message long 321"

/* tests */

#define TEST_SETUP(message_to_test) \
	Core_log_to_last_moc_context.file_name = __FILE__; \
	Core_log_to_last_moc_context.function_name = __func__; \
	Core_log_to_last_moc_context.no_of_calls = 0; \
	strncpy(Core_log_to_last_moc_context.message, message_to_test, \
		CORE_LAST_ERROR_MSG_MAXPRINT); \
	Core_log_to_last_moc_context.initialized = 1

#define TEST_STEP_SETUP() \
	Syslog_no_of_calls = 0; \
	Core_log_to_last_moc_context.no_of_calls = 0; \
	Core_log_to_last_moc_context.line_no = __LINE__; \
	(*(char *)last_error_msg_get()) = '\0'

#define TEST_STEP_CHECK() \
	UT_OUT("%s", last_error_msg_get()); \
	UT_ASSERTeq(strcmp(Core_log_to_last_moc_context.message, \
		last_error_msg_get()), 0); \
	UT_ASSERTeq(Core_log_to_last_moc_context.no_of_calls, 1); \
	UT_ASSERTeq(Syslog_no_of_calls, 1)

#define TEST_STEP(MESSAGE) \
	TEST_STEP_SETUP(); \
	CORE_LOG_ERROR_LAST(MESSAGE); \
	TEST_STEP_CHECK()

#define TEST_STEP_W_ERRNO(MESSAGE) \
	TEST_STEP_SETUP(); \
	CORE_LOG_ERROR_W_ERRNO_LAST(MESSAGE); \
	TEST_STEP_CHECK()

/* basic tests with normal message */
static int
test_CORE_LOG_BASIC(const struct test_case *tc, int argc, char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE);
	TEST_STEP(CORE_LOG_UT_MESSAGE);
	return NO_ARGS_CONSUMED;
}

/* basic tests with max length message */
static int
test_CORE_LOG_BASIC_LONG(const struct test_case *tc, int argc, char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE_LONG);
	TEST_STEP(CORE_LOG_UT_MESSAGE_LONG);
	return NO_ARGS_CONSUMED;
}

/* basic tests with too long  message */
static int
test_CORE_LOG_BASIC_TOO_LONG(const struct test_case *tc, int argc, char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE_LONG);
	TEST_STEP(CORE_LOG_UT_MESSAGE_TOO_LONG);
	return NO_ARGS_CONSUMED;
}

/* basic test with errno message */
static int
test_CORE_LOG_BASIC_W_ERRNO(const struct test_case *tc, int argc, char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE ": " CORE_LOG_UT_ERRNO_SHORT_STR);
	errno = CORE_LOG_UT_ERRNO_SHORT;
	TEST_STEP_W_ERRNO(CORE_LOG_UT_MESSAGE);
	UT_ASSERTeq(errno, CORE_LOG_UT_ERRNO_SHORT);
	return NO_ARGS_CONSUMED;
}

/* basic test with errno message and too long error message */
static int
test_CORE_LOG_BASIC_TOO_LONG_W_ERRNO(const struct test_case *tc,
	int argc, char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE_LONG);
	errno = CORE_LOG_UT_ERRNO_SHORT;
	TEST_STEP_W_ERRNO(CORE_LOG_UT_MESSAGE_TOO_LONG);
	UT_ASSERTeq(errno, CORE_LOG_UT_ERRNO_SHORT);
	return NO_ARGS_CONSUMED;
}

/* basic test with errno message that does not produce error text */
static int
test_CORE_LOG_BASIC_W_ERRNO_BAD(const struct test_case *tc, int argc,
	char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE ": ");
	errno = CORE_LOG_UT_ERRNO_INVALID;
	TEST_STEP_W_ERRNO(CORE_LOG_UT_MESSAGE);
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
		TEST_STEP_SETUP(); CORE_LOG_ERROR_LAST(CORE_LOG_UT_MESSAGE);
		UT_OUT("%s", last_error_msg_get());
		UT_ASSERTeq(strcmp(Core_log_to_last_moc_context.message,
			last_error_msg_get()), 0);
		if (level < CORE_LOG_LEVEL_ERROR) {
			UT_ASSERTeq(Syslog_no_of_calls, 0);
		} else {
			UT_ASSERTeq(Syslog_no_of_calls, 1);
		}
	}
	return NO_ARGS_CONSUMED;
}

static struct test_case test_cases[] = {
	TEST_CASE(test_CORE_LOG_BASIC),
	TEST_CASE(test_CORE_LOG_BASIC_LONG),
	TEST_CASE(test_CORE_LOG_BASIC_TOO_LONG),
	TEST_CASE(test_CORE_LOG_BASIC_TOO_LONG_W_ERRNO),
	TEST_CASE(test_CORE_LOG_BASIC_W_ERRNO),
	TEST_CASE(test_CORE_LOG_BASIC_W_ERRNO_BAD),
	TEST_CASE(test_CORE_LOG_TRESHOLD),
};

/* force test tu use default lgging function */
#undef LOG_SET_PMEMCORE_FUNC
#define LOG_SET_PMEMCORE_FUNC

int
main(int argc, char *argv[])
{
	START(argc, argv, "core_log_last_last");
	core_log_set_function(NULL, NULL);
	core_log_set_threshold(CORE_LOG_THRESHOLD_AUX, CORE_LOG_LEVEL_HARK);
	openlog("core_log_last", 0, 0);

	TEST_CASE_PROCESS(argc, argv, test_cases, ARRAY_SIZE(test_cases));

	closelog();
	DONE(NULL);
}
