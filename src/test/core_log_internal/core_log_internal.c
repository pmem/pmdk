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
 * Use core_log_abort() instead.
 * Use core_log_abort() mock to monitor usage of abort() function
 * inside CORE_LOG_FATAL();
 */
#define abort() core_log_abort()
extern void core_log_abort(void);

/* core_log_abort() - mock for abort() function in CORE_LOG_FATAL() */
static int Core_log_abort_no_of_calls = 0;

FUNC_MOCK(core_log_abort, void, void)
FUNC_MOCK_RUN_DEFAULT {
	Core_log_abort_no_of_calls++;
}
FUNC_MOCK_END

/* core_log() - mock */
static int Core_log_no_of_calls = 0;
static struct core_log_context {
	int initialized;
	enum core_log_level level;
	int errnum;
	const char *file_name;
	int line_no;
	const char *function_name;
	char message_format[8196];
} Core_log_context;

FUNC_MOCK(core_log, void, enum core_log_level level, int errnum,
	const char *file_name, int line_no, const char *function_name,
	const char *message_format, ...)
FUNC_MOCK_RUN_DEFAULT {
	++Core_log_no_of_calls;
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

#define CORE_LOG_UT_MESSAGE "Test message long 20Test message long 40" \
"Test message long 60Test message long 80Test message long100" \
"Test message long120Test message long140Test message long160" \
"Test message long180Test message long200Test message long220" \
"Test message long240Test message long260Test message long280" \
"Test message long300Test message long320Test message long340" \
"Test message long360Test message long380Test message long400    407"

#define TEST_SETUP(MESSAGE_TO_TEST) \
	Core_log_abort_no_of_calls = 0; \
	Core_log_context.file_name = __FILE__; \
	Core_log_context.function_name = __func__; \
	strcpy(Core_log_context.message_format, MESSAGE_TO_TEST); \
	Core_log_context.errnum = NO_ERRNO; \
	Core_log_context.initialized = 1

#define CONCAT2(A, B) A##B
#define CONCAT3(A, B, C) A##B##C

#define TEST_STEP_SETUP(STEP_LEVEL) \
	Core_log_context.level = CORE_LOG_LEVEL_##STEP_LEVEL; \
	Core_log_no_of_calls = 0; \
	Core_log_context.line_no = __LINE__

#define TEST_STEP(STEP_LEVEL) \
	TEST_STEP_SETUP(STEP_LEVEL); CONCAT2(CORE_LOG_, STEP_LEVEL) \
	(CORE_LOG_UT_MESSAGE); \
	UT_ASSERTeq(Core_log_no_of_calls, 1)

#define TEST_STEP_W_ERRNO(STEP_LEVEL, ERRNUM) \
	TEST_STEP_SETUP(STEP_LEVEL); \
	Core_log_context.errnum = ERRNUM; \
	Core_log_context.line_no = __LINE__;\
	CONCAT3(CORE_LOG_, STEP_LEVEL, _W_ERRNO) (CORE_LOG_UT_MESSAGE)

/* tests */
static int
test_CORE_LOG_INTERNAL(const struct test_case *tc, int argc, char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE);
	TEST_STEP(FATAL);
	UT_ASSERTeq(Core_log_abort_no_of_calls, 1);
	TEST_STEP(ERROR);
	TEST_STEP(WARNING);
	TEST_STEP(NOTICE);
	TEST_STEP(HARK);
	TEST_STEP(ERROR);
	UT_ASSERTeq(Core_log_abort_no_of_calls, 1);
	return NO_ARGS_CONSUMED;
}

static int
test_CORE_LOG_INTERNAL_LAST_MESSAGE(const struct test_case *tc, int argc,
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

static int
test_CORE_LOG_INTERNAL_LAST_MESSAGE_W_ERRNO(const struct test_case *tc, \
	int argc, char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE ": ");
	for (int i = 0; i < 256; i++) {
		errno = i;
		Core_log_no_of_calls = 0;
		Core_log_context.errnum = i;
		Core_log_context.level = CORE_LOG_LEVEL_ERROR_LAST;
		Core_log_context.line_no = __LINE__ + 1;
		CORE_LOG_ERROR_W_ERRNO_LAST(CORE_LOG_UT_MESSAGE);
		UT_ASSERTeq(errno, i);
		UT_ASSERTeq(Core_log_no_of_calls, 1);
	}
	errno = 0;
	return NO_ARGS_CONSUMED;
}

static int
test_CORE_LOG_INTERNAL_W_ERRNO(const struct test_case *tc,
	int argc, char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE ": ");
	for (int i = 0; i < 256; i++) {
		errno = i;
		Core_log_abort_no_of_calls = 0;
		TEST_STEP_W_ERRNO(FATAL, i);
		UT_ASSERTeq(Core_log_abort_no_of_calls, 1);
		UT_ASSERTeq(errno, i);
		TEST_STEP_W_ERRNO(ERROR, i);
		UT_ASSERTeq(errno, i);
		TEST_STEP_W_ERRNO(WARNING, i);
		UT_ASSERTeq(errno, i);
		UT_ASSERTeq(Core_log_abort_no_of_calls, 1);
	}
	errno = 0;
	return NO_ARGS_CONSUMED;
}

#define CORE_LOG_TRESHOLD_STEP(STEP_LEVEL, ABORT_NO_OF_CALL, \
		CORE_LOG_NO_OF_CALL) \
	do { \
		TEST_STEP_SETUP(STEP_LEVEL); \
		Core_log_abort_no_of_calls = 0; \
		CONCAT2(CORE_LOG_, STEP_LEVEL)(CORE_LOG_UT_MESSAGE); \
		UT_ASSERTeq(Core_log_abort_no_of_calls, ABORT_NO_OF_CALL); \
		UT_ASSERTeq(Core_log_no_of_calls, CORE_LOG_NO_OF_CALL); \
	} while (0)

#define CORE_LOG_TRESHOLD_STEP_ALL(FATAL_PASS, ERROR_PASS, WARNING_PASS, \
	NOTICE_PASS, HARK_PASS) \
	Core_log_abort_no_of_calls = 0; \
	CORE_LOG_TRESHOLD_STEP(FATAL, 1, FATAL_PASS); \
	Core_log_abort_no_of_calls = 0; \
	CORE_LOG_TRESHOLD_STEP(ERROR, 0, ERROR_PASS); \
	CORE_LOG_TRESHOLD_STEP(WARNING, 0, WARNING_PASS); \
	CORE_LOG_TRESHOLD_STEP(NOTICE, 0, NOTICE_PASS); \
	CORE_LOG_TRESHOLD_STEP(HARK, 0, HARK_PASS)

static int
test_CORE_LOG_INTERNAL_TRESHOLD(const struct test_case *tc, int argc,
	char *argv[])
{
	TEST_SETUP(CORE_LOG_UT_MESSAGE);
	Core_log_abort_no_of_calls = 0;
	CORE_LOG_TRESHOLD_STEP(FATAL, 1, 1);
	Core_log_abort_no_of_calls = 0;
	CORE_LOG_TRESHOLD_STEP(ERROR, 0, 1);
	CORE_LOG_TRESHOLD_STEP(WARNING, 0, 1);
	CORE_LOG_TRESHOLD_STEP(NOTICE, 0, 1);
	CORE_LOG_TRESHOLD_STEP(HARK, 0, 1);
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_HARK);
	CORE_LOG_TRESHOLD_STEP_ALL(0, 0, 0, 0, 1);
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_FATAL);
	CORE_LOG_TRESHOLD_STEP_ALL(1, 0, 0, 0, 1);
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_ERROR);
	CORE_LOG_TRESHOLD_STEP_ALL(1, 1, 0, 0, 1);
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_WARNING);
	CORE_LOG_TRESHOLD_STEP_ALL(1, 1, 1, 0, 1);
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_NOTICE);
	CORE_LOG_TRESHOLD_STEP_ALL(1, 1, 1, 1, 1);
	return NO_ARGS_CONSUMED;
}

static struct test_case test_cases[] = {
	TEST_CASE(test_CORE_LOG_INTERNAL),
	TEST_CASE(test_CORE_LOG_INTERNAL_LAST_MESSAGE),
	TEST_CASE(test_CORE_LOG_INTERNAL_LAST_MESSAGE_W_ERRNO),
	TEST_CASE(test_CORE_LOG_INTERNAL_W_ERRNO),
	TEST_CASE(test_CORE_LOG_INTERNAL_TRESHOLD),
};

#define NTESTS ARRAY_SIZE(test_cases)

/* Restore original abort() definition as it is defined in stdlib.h */
#undef abort
extern void abort(void) __THROW __attribute__((__noreturn__));

#undef LOG_SET_PMEMCORE_FUNC
#define LOG_SET_PMEMCORE_FUNC

int
main(int argc, char *argv[])
{
	START(argc, argv, "core_log");
	core_log_set_function(CORE_LOG_USE_DEFAULT_FUNCTION, NULL);
	openlog("core_log", 0, 0);

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	closelog();
	DONE(NULL);
}
