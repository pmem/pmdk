// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * core_log.c -- unit test for core_log(), core_log_va() and
 * core_log_set_function()
 */

#include <syslog.h>

#include "last_error_msg.h"
#include "unittest.h"

#include "core_log_common.h"

/* tests */

/*
 * Check:
 * - CORE_LOG_LEVEL_ERROR_LAST -> CORE_LOG_LEVEL_ERROR
 * - buf == last_error_msg_get();
 * - buf_len == CORE_LAST_ERROR_MSG_MAXPRINT
 */
static int
test_CORE_LOG_LEVEL_ERROR_LAST(const struct test_case *tc, int argc,
	char *argv[])
{
	/* Pass the message all the way to the logging function. */
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_ERROR);

	reset_mocks();

	/* set the expectations */
	Common.use_last_error_msg = true;
	Vsnprintf_.ret = 0; /* empty but successful */
	Log_function_.exp_level = CORE_LOG_LEVEL_ERROR;

	core_log(CORE_LOG_LEVEL_ERROR_LAST, NO_ERRNO, FILE_NAME, LINE_NO,
		FUNC_NAME, MSG_FORMAT);

	/* check the call counters */
	UT_ASSERTeq(RCOUNTER(last_error_msg_get), CALLED);
	UT_ASSERTeq(RCOUNTER(vsnprintf), CALLED);
	UT_ASSERTeq(RCOUNTER(__xpg_strerror_r), NOT_CALLED);
	UT_ASSERTeq(RCOUNTER(core_log_default_function), CALLED);
	return NO_ARGS_CONSUMED;
}

/*
 * Check:
 * - The log preparation stops after the failed vsnprintf() call.
 * - The log function is not called.
 */
static int
test_vsnprintf_fail(const struct test_case *tc, int argc, char *argv[])
{
	/* Pass the message all the way to the logging function. */
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_ERROR);

	reset_mocks();

	/* set the expectations */
	Vsnprintf_.ret = -1; /* negative value if an output error */
	Log_function_.exp_level = CORE_LOG_LEVEL_ERROR;
	Common.use_last_error_msg = false;

	core_log(CORE_LOG_LEVEL_ERROR, NO_ERRNO, FILE_NAME, LINE_NO,
		FUNC_NAME, MSG_FORMAT);

	/* check the call counters */
	UT_ASSERTeq(RCOUNTER(last_error_msg_get), NOT_CALLED);
	UT_ASSERTeq(RCOUNTER(vsnprintf), CALLED);
	UT_ASSERTeq(RCOUNTER(__xpg_strerror_r), NOT_CALLED);
	UT_ASSERTeq(RCOUNTER(core_log_default_function), NOT_CALLED);
	return NO_ARGS_CONSUMED;
}

/*
 * Check:
 * - NO_ERRNO means no strerror_r() call.
 * - The produced log message is passed to the log function.
 */
static int
test_NO_ERRNO(const struct test_case *tc, int argc, char *argv[])
{
	/* Pass the message all the way to the logging function. */
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_ERROR);

	reset_mocks();

	/* set the expectations */
	Vsnprintf_.ret = 0; /* leave a lot of space for the error string */
	Log_function_.exp_level = CORE_LOG_LEVEL_ERROR;
	Common.use_last_error_msg = false;

	core_log(CORE_LOG_LEVEL_ERROR, NO_ERRNO, FILE_NAME, LINE_NO,
		FUNC_NAME, MSG_FORMAT);

	/* check the call counters */
	UT_ASSERTeq(RCOUNTER(last_error_msg_get), NOT_CALLED);
	UT_ASSERTeq(RCOUNTER(vsnprintf), CALLED);
	UT_ASSERTeq(RCOUNTER(__xpg_strerror_r), NOT_CALLED);
	UT_ASSERTeq(RCOUNTER(core_log_default_function), CALLED);
	return NO_ARGS_CONSUMED;
}

/*
 * Check:
 * - fully fill the provided buffer in three ways:
 *   (1) exactly (`- 1` for the terminating null-byte)
 *   (2) one character too many (the null-byte would end up just after
 *       the actual buffer space)
 *   (3) two characters too many (no space for one character of the message and
 *       for the null-byte)
 * - the strerror_r() is not called despite an errno is provided.
 */
static int
test_no_space_for_strerror_r(const struct test_case *tc, int argc, char *argv[])
{
	/* Pass the message all the way to the logging function. */
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_ERROR);

	for (int i = _CORE_LOG_MSG_MAXPRINT - 1;
			i <= _CORE_LOG_MSG_MAXPRINT + 1; ++i) {
		test_no_space_for_strerror_r_helper(i);
	}

	return NO_ARGS_CONSUMED;
}

/*
 * Check:
 * - strerror_r() fails in two ways:
 *   (1) before glibc 2.13
 *   (2) since glibc 2.13
 * - The produced log message is passed to the log function.
 */
static int
test_strerror_r_fail(const struct test_case *tc, int argc, char *argv[])
{
	/* Pass the message all the way to the logging function. */
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_ERROR);

	test_strerror_r_fail_helper(true);
	test_strerror_r_fail_helper(false);

	return NO_ARGS_CONSUMED;
}

/*
 * Check:
 * - if level <= threshold:
 *   - the log function is called
 * - else:
 *   - the log function is not called
 */
static int
test_level_gt_threshold(const struct test_case *tc, int argc, char *argv[])
{
	for (enum core_log_level threshold = CORE_LOG_LEVEL_HARK;
			threshold < CORE_LOG_LEVEL_MAX; ++threshold) {
		core_log_set_threshold(CORE_LOG_THRESHOLD, threshold);

		for (enum core_log_level level = CORE_LOG_LEVEL_HARK;
				level < CORE_LOG_LEVEL_MAX; ++level) {
			bool log_function_called = (level <= threshold);
			test_log_function_call_helper(level,
				log_function_called);
		}
	}

	return NO_ARGS_CONSUMED;
}

static int
test_happy_day_helper(core_log_function *log_function)
{
	/* Pass the message all the way to the logging function. */
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_ERROR);

	/*
	 * Disable the validation as custom_log_function() may be called from
	 * core_log_set_function().
	 */
	if (log_function == CORE_LOG_USE_DEFAULT_FUNCTION) {
		FUNC_MOCK_RCOUNTER_SET(core_log_default_function,
			NOT_VALIDATED_CALL);
	} else {
		FUNC_MOCK_RCOUNTER_SET(custom_log_function,
			NOT_VALIDATED_CALL);
	}

	core_log_set_function(log_function);

	reset_mocks();

	/* set the expectations */
	Vsnprintf_.ret = BASIC_MESSAGE_LEN;
	Log_function_.exp_level = CORE_LOG_LEVEL_ERROR;
	Common.use_last_error_msg = true;
	Strerror_r.exp__buf = LAST_ERROR_MSG_MOCK + Vsnprintf_.ret;
	Strerror_r.exp__buflen = CORE_LAST_ERROR_MSG_MAXPRINT -
		(size_t)Vsnprintf_.ret;
	Strerror_r.error = EXIT_SUCCESS;

	core_log(CORE_LOG_LEVEL_ERROR_LAST, DUMMY_ERRNO1, FILE_NAME, LINE_NO,
		FUNC_NAME, MSG_FORMAT);

	/* check the call counters */
	UT_ASSERTeq(RCOUNTER(last_error_msg_get), CALLED);
	UT_ASSERTeq(RCOUNTER(vsnprintf), CALLED);
	UT_ASSERTeq(RCOUNTER(__xpg_strerror_r), CALLED);
	if (log_function == CORE_LOG_USE_DEFAULT_FUNCTION) {
		UT_ASSERTeq(RCOUNTER(core_log_default_function), CALLED);
		UT_ASSERTeq(RCOUNTER(custom_log_function), NOT_CALLED);
	} else {
		UT_ASSERTeq(RCOUNTER(core_log_default_function), NOT_CALLED);
		UT_ASSERTeq(RCOUNTER(custom_log_function), CALLED);
	}

	return NO_ARGS_CONSUMED;
}

static int
test_happy_day(const struct test_case *tc, int argc, char *argv[])
{
	return test_happy_day_helper(CORE_LOG_USE_DEFAULT_FUNCTION);
}

/* Happy day scenario with custom logging function */
static int
test_set_custom_function(const struct test_case *tc, int argc, char *argv[])
{
	return test_happy_day_helper(custom_log_function);
}

static struct test_case test_cases[] = {
	TEST_CASE(test_CORE_LOG_LEVEL_ERROR_LAST),
	TEST_CASE(test_vsnprintf_fail),
	TEST_CASE(test_NO_ERRNO),
	TEST_CASE(test_no_space_for_strerror_r),
	TEST_CASE(test_strerror_r_fail),
	TEST_CASE(test_level_gt_threshold),
	TEST_CASE(test_happy_day),
	TEST_CASE(test_set_custom_function),
};

#define NTESTS ARRAY_SIZE(test_cases)

int
main(int argc, char *argv[])
{
	START(argc, argv, "core_log");
	core_log_set_function(CORE_LOG_USE_DEFAULT_FUNCTION);
	openlog(NULL, LOG_NDELAY, LOG_USER);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	closelog();
	DONE(NULL);
}
