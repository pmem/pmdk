// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * core_log_internal.c -- unit test to CORE_LOG_...
 */

#undef _GNU_SOURCE
#include <string.h>
#include <syslog.h>

#include "unittest.h"
#include "log_internal.h"
#include "last_error_msg.h"

#define NO_ARGS_CONSUMED 0

/*
 * Prevent abort() from CORE_LOG_FATAL()
 * Use mock_abort() instead.
 * Use mock_abort() mock to monitor usage of abort() function
 * inside CORE_LOG_FATAL();
 */
#define abort() mock_abort()
void mock_abort(void);

/* mock_abort() - mock for abort() function in CORE_LOG_FATAL() */
static int Mock_abort_no_of_calls = 0;

void
mock_abort(void)
{
	Mock_abort_no_of_calls++;
}

/* core_log() - mock */
static int Core_log_no_of_calls = 0;
static struct {
	int initialized;
	enum core_log_level level;
	int errnum;
	const char *file_name;
	unsigned line_no;
	const char *function_name;
	const char *message_format;
} Core_log_context;

FUNC_MOCK(core_log, void, enum core_log_level level, int errnum,
	const char *file_name, unsigned line_no, const char *function_name,
	const char *message_format, ...)
FUNC_MOCK_RUN_DEFAULT {
	Core_log_no_of_calls++;
	if (Core_log_context.initialized) {
		UT_ASSERTeq(Core_log_context.level, level);
		UT_ASSERTeq(Core_log_context.errnum, errnum);
		UT_ASSERTeq(strcmp(Core_log_context.file_name, file_name), 0);
		UT_ASSERTeq(Core_log_context.line_no, line_no);
		UT_ASSERTeq(strcmp(Core_log_context.function_name,
			function_name), 0);
		UT_ASSERTeq(strcmp(Core_log_context.message_format,
			message_format), 0);
	}
}
FUNC_MOCK_END

static enum core_log_level Core_log_default_threshold;

#define CORE_LOG_UT_MESSAGE "Test message long 20Test message long 40" \
	"Test message long 60Test message long 80Test message long100" \
	"Test message long120Test message long140Test message long160" \
	"Test message long180Test message long200Test message long220" \
	"Test message long240Test message long260Test message long280" \
	"Test message long300Test message long320Test message long340" \
	"Test message long360Test message long380Test message long400    407"

#define TEST_SETUP(MESSAGE_TO_TEST) \
	core_log_set_threshold(CORE_LOG_THRESHOLD, \
		Core_log_default_threshold); \
	Mock_abort_no_of_calls = 0; \
	Core_log_context.file_name = __FILE__; \
	Core_log_context.function_name = __func__; \
	Core_log_context.message_format = MESSAGE_TO_TEST; \
	Core_log_context.errnum = NO_ERRNO; \
	Core_log_context.initialized = 1

#define CORE_LOG_(LEVEL) CORE_LOG_##LEVEL
#define CORE_LOG_W_ERRNO_(LEVEL) CORE_LOG_##LEVEL##_W_ERRNO

#define TEST_STEP_SETUP(LEVEL) \
	Core_log_context.level = CORE_LOG_LEVEL_##LEVEL; \
	Core_log_no_of_calls = 0; \
	Core_log_context.line_no = __LINE__

#define TEST_STEP(LEVEL, PASS) \
	TEST_STEP_SETUP(LEVEL); \
	CORE_LOG_(LEVEL)(CORE_LOG_UT_MESSAGE); \
	UT_ASSERTeq(Core_log_no_of_calls, PASS)

#define TEST_STEP_W_ERRNO(LEVEL, ERRNUM) \
	TEST_STEP_SETUP(LEVEL); \
	Core_log_context.errnum = ERRNUM; \
	CORE_LOG_W_ERRNO_(LEVEL) (CORE_LOG_UT_MESSAGE)

/* tests CORE_LOG_... with default threshold */
static int
test_CORE_LOG(const struct test_case *tc, int argc, char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE);
	TEST_STEP(HARK, 1);
	UT_ASSERTeq(Mock_abort_no_of_calls, 0);
	TEST_STEP(FATAL, 1);
	UT_ASSERTeq(Mock_abort_no_of_calls, 1);
	Mock_abort_no_of_calls = 0;
	TEST_STEP(ERROR, 1);
	TEST_STEP(WARNING, 1);
	TEST_STEP(NOTICE, 0);
	TEST_STEP(INFO, 0);
	TEST_STEP(DEBUG, 0);
	UT_ASSERTeq(Mock_abort_no_of_calls, 0);
	return NO_ARGS_CONSUMED;
}

/* Test for CORE_LOG_ERROR_LAST() */
static int
test_CORE_LOG_ERROR_LAST(const struct test_case *tc, int argc,
	char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE);
	Core_log_no_of_calls = 0;
	Core_log_context.level = CORE_LOG_LEVEL_ERROR_LAST;
	Core_log_context.line_no = __LINE__ + 1;
	CORE_LOG_ERROR_LAST(CORE_LOG_UT_MESSAGE);
	UT_ASSERTeq(Core_log_no_of_calls, 1);
	return NO_ARGS_CONSUMED;
}

#define DUMMY_ERRNO 0xf00d

/* Test for CORE_LOG_ERROR_W_ERRNO_LAST() w/ errno */
static int
test_CORE_LOG_ERROR_W_ERRNO_LAST(const struct test_case *tc, int argc, \
	char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE ": ");
	errno = DUMMY_ERRNO;
	Core_log_no_of_calls = 0;
	Core_log_context.errnum = DUMMY_ERRNO;
	Core_log_context.level = CORE_LOG_LEVEL_ERROR_LAST;
	Core_log_context.line_no = __LINE__ + 1;
	CORE_LOG_ERROR_W_ERRNO_LAST(CORE_LOG_UT_MESSAGE);
	UT_ASSERTeq(errno, DUMMY_ERRNO);
	UT_ASSERTeq(Core_log_no_of_calls, 1);
	errno = 0;
	return NO_ARGS_CONSUMED;
}

/* Test all macros that pass errno */
static int
test_CORE_LOG_W_ERRNO(const struct test_case *tc,
	int argc, char *argv[])
{
	int errnum = DUMMY_ERRNO;
	TEST_SETUP(CORE_LOG_UT_MESSAGE ": ");
	Mock_abort_no_of_calls = 0;
	errno = errnum;
	TEST_STEP_W_ERRNO(FATAL, errnum);
	UT_ASSERTeq(Mock_abort_no_of_calls, 1);
	UT_ASSERTeq(errno, errnum);
	errno = ++errnum;
	TEST_STEP_W_ERRNO(ERROR, errnum);
	UT_ASSERTeq(errno, errnum);
	errno = ++errnum;
	TEST_STEP_W_ERRNO(WARNING, errnum);
	UT_ASSERTeq(errno, errnum);
	UT_ASSERTeq(Mock_abort_no_of_calls, 1);
	errno = 0;
	return NO_ARGS_CONSUMED;
}

#define CORE_LOG_TRESHOLD_STEP(LEVEL, ABORT_NO_OF_CALL, CORE_LOG_NO_OF_CALL) \
	do { \
		TEST_STEP_SETUP(LEVEL); \
		Mock_abort_no_of_calls = 0; \
		CORE_LOG_(LEVEL)(CORE_LOG_UT_MESSAGE); \
		UT_ASSERTeq(Mock_abort_no_of_calls, ABORT_NO_OF_CALL); \
		UT_ASSERTeq(Core_log_no_of_calls, CORE_LOG_NO_OF_CALL); \
	} while (0)

#define CORE_LOG_TRESHOLD_STEP_ALL(HARK_PASS, FATAL_PASS, ERROR_PASS, \
		WARNING_PASS, NOTICE_PASS, INFO_PASS, DEBUG_PASS) \
	Mock_abort_no_of_calls = 0; \
	CORE_LOG_TRESHOLD_STEP(HARK, 0, HARK_PASS); \
	CORE_LOG_TRESHOLD_STEP(FATAL, 1, FATAL_PASS); \
	Mock_abort_no_of_calls = 0; \
	CORE_LOG_TRESHOLD_STEP(ERROR, 0, ERROR_PASS); \
	CORE_LOG_TRESHOLD_STEP(WARNING, 0, WARNING_PASS); \
	CORE_LOG_TRESHOLD_STEP(NOTICE, 0, NOTICE_PASS); \
	CORE_LOG_TRESHOLD_STEP(INFO, 0, INFO_PASS); \
	CORE_LOG_TRESHOLD_STEP(DEBUG, 0, DEBUG_PASS)

/* Test all possible tresholds */
static int
test_CORE_LOG_TRESHOLD(const struct test_case *tc, int argc, char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE);
	Mock_abort_no_of_calls = 0;
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_HARK);
	CORE_LOG_TRESHOLD_STEP_ALL(1, 0, 0, 0, 0, 0, 0);
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_FATAL);
	CORE_LOG_TRESHOLD_STEP_ALL(1, 1, 0, 0, 0, 0, 0);
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_ERROR);
	CORE_LOG_TRESHOLD_STEP_ALL(1, 1, 1, 0, 0, 0, 0);
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_WARNING);
	CORE_LOG_TRESHOLD_STEP_ALL(1, 1, 1, 1, 0, 0, 0);
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_NOTICE);
	CORE_LOG_TRESHOLD_STEP_ALL(1, 1, 1, 1, 1, 0, 0);
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_INFO);
	CORE_LOG_TRESHOLD_STEP_ALL(1, 1, 1, 1, 1, 1, 0);
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_DEBUG);
	CORE_LOG_TRESHOLD_STEP_ALL(1, 1, 1, 1, 1, 1, 1);
	return NO_ARGS_CONSUMED;
}

/* Validate the default threshold of the release build (no DEBUG set). */
static int
test_CORE_LOG_TRESHOLD_DEFAULT(const struct test_case *tc, int argc,
	char *argv[])
{
	UT_ASSERTeq(Core_log_default_threshold, CORE_LOG_LEVEL_WARNING);
	return NO_ARGS_CONSUMED;
}

static struct test_case test_cases[] = {
	TEST_CASE(test_CORE_LOG),
	TEST_CASE(test_CORE_LOG_ERROR_LAST),
	TEST_CASE(test_CORE_LOG_ERROR_W_ERRNO_LAST),
	TEST_CASE(test_CORE_LOG_W_ERRNO),
	TEST_CASE(test_CORE_LOG_TRESHOLD),
	TEST_CASE(test_CORE_LOG_TRESHOLD_DEFAULT),
};

#define NTESTS ARRAY_SIZE(test_cases)

/* Restore original abort() definition as it is defined in stdlib.h */
#undef abort
extern void abort(void) __THROW __attribute__((__noreturn__));

int
main(int argc, char *argv[])
{
	core_log_get_threshold(CORE_LOG_THRESHOLD, &Core_log_default_threshold);
	core_log_set_function(CORE_LOG_USE_DEFAULT_FUNCTION);

	START(argc, argv, "core_log_internal");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
