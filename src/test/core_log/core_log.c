// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * core_log.c -- unit test for core_log() and core_log_va()
 */

#include <stdbool.h>
#include <stddef.h>

#include "last_error_msg.h"
#include "unittest.h"

#define NO_ARGS_CONSUMED 0

#define FILE_NAME "dummy.c"
#define LINE_NO 123
#define FUNC_NAME "dummy_func"
#define MSG_FORMAT "dummy msg format"
#define DUMMY_ARG 456
#define LAST_ERROR_MSG_MOCK ((char *)0x1a547e58)
#define ANYTHING_BUT_LAST_ERROR_MSG ((char *)0xa7481768)
#define DUMMY_CONTEXT ((void *)0x800df00d)
#define DUMMY_ERRNO1 500
#define DUMMY_ERRNO2 313
#define BASIC_MESSAGE_LEN 131

/* XXX to be removed when introduced to the unittest.sh */
#define UT_ASSERTstreq(a, b) \
	UT_ASSERTeq(strcmp((a), (b)), 0)

static struct {
	char *exp_buf;
	size_t exp_buf_len;
} Common;

FUNC_MOCK(last_error_msg_get, const char *, void)
FUNC_MOCK_RUN_DEFAULT {
	return LAST_ERROR_MSG_MOCK;
}
FUNC_MOCK_END

static struct {
	bool use_mock;
	int ret;
} Vsnprintf_ = {
	.use_mock = false
};

FUNC_MOCK(vsnprintf, int, char *__restrict __s, size_t __maxlen,
	const char *__restrict __format, va_list __arg)
FUNC_MOCK_RUN_DEFAULT {
	if (!Vsnprintf_.use_mock) {
		return _FUNC_REAL(vsnprintf)(__s, __maxlen, __format, __arg);
	}
	/*
	 * Have to arm this fuse ASAP since all the output from the unittest
	 * framework relies on it to work as normal.
	 */
	Vsnprintf_.use_mock = false;

	if (Common.exp_buf == ANYTHING_BUT_LAST_ERROR_MSG) {
		UT_ASSERTne(__s, LAST_ERROR_MSG_MOCK);
	} else {
		UT_ASSERTeq(__s, Common.exp_buf);
	}
	UT_ASSERTeq(__maxlen, Common.exp_buf_len);
	UT_ASSERTstreq(__format, MSG_FORMAT);
	int arg0 = va_arg(__arg, int);
	UT_ASSERTeq(arg0, DUMMY_ARG);

	return Vsnprintf_.ret;
}
FUNC_MOCK_END

static struct {
	char *exp__buf;
	size_t exp__buflen;
	bool before_glibc_2_13;
	int error;
} Strerror_r;

FUNC_MOCK(__xpg_strerror_r, int, int __errnum, char *__buf, size_t __buflen)
FUNC_MOCK_RUN_DEFAULT {
	UT_ASSERTeq(__errnum, DUMMY_ERRNO1);
	UT_ASSERTeq(__buf, Strerror_r.exp__buf);
	UT_ASSERTeq(__buflen, Strerror_r.exp__buflen);

	if (Strerror_r.error == EXIT_SUCCESS) {
		return 0;
	} else if (Strerror_r.before_glibc_2_13) {
		errno = Strerror_r.error;
		return -1;
	} else {
		return Strerror_r.error;
	}
}
FUNC_MOCK_END

static struct {
	bool use_mock;
	int rcounter;
	enum core_log_level exp_level;
} Log_function_ = {
	.use_mock = false
};

static void
log_function(void *context, enum core_log_level level, const char *file_name,
	const int line_no, const char *function_name, const char *message)
{
	/*
	 * As part of the attaching a logging function process a few basic
	 * information pieces are genuinely logged using it. So we have to be
	 * ready to ignore them instead of validating them.
	 */
	if (!Log_function_.use_mock) {
		return;
	}

	Log_function_.rcounter++;

	UT_ASSERTeq(context, DUMMY_CONTEXT);
	UT_ASSERTeq(level, Log_function_.exp_level);
	UT_ASSERTstreq(file_name, FILE_NAME);
	UT_ASSERTeq(line_no, LINE_NO);
	UT_ASSERTstreq(function_name, FUNC_NAME);
	if (Common.exp_buf == ANYTHING_BUT_LAST_ERROR_MSG) {
		UT_ASSERTne(message, LAST_ERROR_MSG_MOCK);
	} else {
		UT_ASSERTeq(message, Common.exp_buf);
	}
}

static void
reset_call_counters(void)
{
	FUNC_MOCK_RCOUNTER_SET(vsnprintf, 0);
	FUNC_MOCK_RCOUNTER_SET(__xpg_strerror_r, 0);
	Log_function_.rcounter = 0;
}

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
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_DEBUG);

	/* set the expectations */
	Common.exp_buf = LAST_ERROR_MSG_MOCK;
	Common.exp_buf_len = CORE_LAST_ERROR_MSG_MAXPRINT;
	Vsnprintf_.use_mock = true;
	Vsnprintf_.ret = 0; /* empty but successful */
	Log_function_.exp_level = CORE_LOG_LEVEL_ERROR;

	reset_call_counters();

	core_log(CORE_LOG_LEVEL_ERROR_LAST, NO_ERRNO, FILE_NAME, LINE_NO,
		FUNC_NAME, MSG_FORMAT, DUMMY_ARG);

	/* check the call counters */
	UT_ASSERTeq(RCOUNTER(vsnprintf), 1);
	UT_ASSERTeq(RCOUNTER(__xpg_strerror_r), 0);
	UT_ASSERTeq(Log_function_.rcounter, 1);
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
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_DEBUG);

	/* set the expectations */
	Vsnprintf_.use_mock = true;
	Vsnprintf_.ret = -1; /* negative value if an output error */
	Log_function_.exp_level = CORE_LOG_LEVEL_ERROR;
	Common.exp_buf = ANYTHING_BUT_LAST_ERROR_MSG;
	Common.exp_buf_len = _CORE_LOG_MSG_MAXPRINT;

	reset_call_counters();

	core_log(CORE_LOG_LEVEL_ERROR, NO_ERRNO, FILE_NAME, LINE_NO,
		FUNC_NAME, MSG_FORMAT, DUMMY_ARG);

	/* check the call counters */
	UT_ASSERTeq(RCOUNTER(vsnprintf), 1);
	UT_ASSERTeq(RCOUNTER(__xpg_strerror_r), 0);
	UT_ASSERTeq(Log_function_.rcounter, 0);
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
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_DEBUG);

	/* set the expectations */
	Vsnprintf_.use_mock = true;
	Vsnprintf_.ret = 0; /* leave a lot of space for the error string */
	Log_function_.exp_level = CORE_LOG_LEVEL_ERROR;
	Common.exp_buf = ANYTHING_BUT_LAST_ERROR_MSG;
	Common.exp_buf_len = _CORE_LOG_MSG_MAXPRINT;

	reset_call_counters();

	core_log(CORE_LOG_LEVEL_ERROR, NO_ERRNO, FILE_NAME, LINE_NO,
		FUNC_NAME, MSG_FORMAT, DUMMY_ARG);

	/* check the call counters */
	UT_ASSERTeq(RCOUNTER(vsnprintf), 1);
	UT_ASSERTeq(RCOUNTER(__xpg_strerror_r), 0);
	UT_ASSERTeq(Log_function_.rcounter, 1);
	return NO_ARGS_CONSUMED;
}

static void
test_no_space_for_strerror_r_helper(int core_message_length)
{
	/* set the expectations */
	Vsnprintf_.use_mock = true;
	Vsnprintf_.ret = core_message_length;
	Log_function_.exp_level = CORE_LOG_LEVEL_ERROR;
	Common.exp_buf = ANYTHING_BUT_LAST_ERROR_MSG;
	Common.exp_buf_len = _CORE_LOG_MSG_MAXPRINT;

	reset_call_counters();

	core_log(CORE_LOG_LEVEL_ERROR, DUMMY_ERRNO1, FILE_NAME, LINE_NO,
		FUNC_NAME, MSG_FORMAT, DUMMY_ARG);

	/* check the call counters */
	UT_ASSERTeq(RCOUNTER(vsnprintf), 1);
	UT_ASSERTeq(RCOUNTER(__xpg_strerror_r), 0);
	UT_ASSERTeq(Log_function_.rcounter, 1);
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
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_DEBUG);

	for (int i = _CORE_LOG_MSG_MAXPRINT - 1;
			i <= _CORE_LOG_MSG_MAXPRINT + 1; ++i) {
		test_no_space_for_strerror_r_helper(i);
	}

	return NO_ARGS_CONSUMED;
}

static void
test_strerror_r_fail_helper(bool before_glibc_2_13)
{
	/* set the expectations */
	Vsnprintf_.use_mock = true;
	Vsnprintf_.ret = BASIC_MESSAGE_LEN;
	Log_function_.exp_level = CORE_LOG_LEVEL_ERROR;
	Common.exp_buf = LAST_ERROR_MSG_MOCK;
	Common.exp_buf_len = CORE_LAST_ERROR_MSG_MAXPRINT;
	Strerror_r.exp__buf = Common.exp_buf + Vsnprintf_.ret;
	Strerror_r.exp__buflen = Common.exp_buf_len - (size_t)Vsnprintf_.ret;
	Strerror_r.error = DUMMY_ERRNO2;
	Strerror_r.before_glibc_2_13 = before_glibc_2_13;

	reset_call_counters();

	core_log(CORE_LOG_LEVEL_ERROR_LAST, DUMMY_ERRNO1, FILE_NAME, LINE_NO,
		FUNC_NAME, MSG_FORMAT, DUMMY_ARG);

	/* check the call counters */
	UT_ASSERTeq(RCOUNTER(vsnprintf), 1);
	UT_ASSERTeq(RCOUNTER(__xpg_strerror_r), 1);
	UT_ASSERTeq(Log_function_.rcounter, 1);
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
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_DEBUG);

	test_strerror_r_fail_helper(true);
	test_strerror_r_fail_helper(false);

	return NO_ARGS_CONSUMED;
}

static void
test_log_function_call_helper(enum core_log_level level,
	bool call_log_function)
{
	/* set the expectations */
	Vsnprintf_.use_mock = true;
	Vsnprintf_.ret = BASIC_MESSAGE_LEN;
	Log_function_.exp_level = level;
	Common.exp_buf = ANYTHING_BUT_LAST_ERROR_MSG;
	Common.exp_buf_len = _CORE_LOG_MSG_MAXPRINT;

	reset_call_counters();

	core_log(level, NO_ERRNO, FILE_NAME, LINE_NO, FUNC_NAME, MSG_FORMAT,
		DUMMY_ARG);

	/* check the call counters */
	UT_ASSERTeq(RCOUNTER(vsnprintf), 1);
	UT_ASSERTeq(RCOUNTER(__xpg_strerror_r), 0);
	UT_ASSERTeq(Log_function_.rcounter, call_log_function ? 1 : 0);
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
			bool call_log_function = (level <= threshold);
			test_log_function_call_helper(level, call_log_function);
		}
	}

	return NO_ARGS_CONSUMED;
}

/*
 * Check:
 * - if Core_log_function == 0:
 *   - the log function is not called
 */
static int
test_no_log_function(const struct test_case *tc, int argc, char *argv[])
{
	/* Pass the message all the way to the logging function. */
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_DEBUG);

	/*
	 * Can't provide a NULL pointer as a log function via
	 * the core_log_set_function() API since it will automatically connect
	 * the default log function instead. Hence, the direct assignment.
	 */
	Core_log_function = 0;
	bool call_log_function = false;

	test_log_function_call_helper(CORE_LOG_LEVEL_ERROR, call_log_function);

	/* restore the log function used by the rest of the tests */
	Log_function_.use_mock = false; /* to ignore initial messages */
	core_log_set_function(log_function, DUMMY_CONTEXT);
	Log_function_.use_mock = true;

	return NO_ARGS_CONSUMED;
}

static int
test_happy_day(const struct test_case *tc, int argc, char *argv[])
{
	/* Pass the message all the way to the logging function. */
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_DEBUG);

	/* set the expectations */
	Vsnprintf_.use_mock = true;
	Vsnprintf_.ret = BASIC_MESSAGE_LEN;
	Log_function_.exp_level = CORE_LOG_LEVEL_ERROR;
	Common.exp_buf = LAST_ERROR_MSG_MOCK;
	Common.exp_buf_len = CORE_LAST_ERROR_MSG_MAXPRINT;
	Strerror_r.exp__buf = Common.exp_buf + Vsnprintf_.ret;
	Strerror_r.exp__buflen = Common.exp_buf_len - (size_t)Vsnprintf_.ret;
	Strerror_r.error = EXIT_SUCCESS;

	reset_call_counters();

	core_log(CORE_LOG_LEVEL_ERROR_LAST, DUMMY_ERRNO1, FILE_NAME, LINE_NO,
		FUNC_NAME, MSG_FORMAT, DUMMY_ARG);

	/* check the call counters */
	UT_ASSERTeq(RCOUNTER(vsnprintf), 1);
	UT_ASSERTeq(RCOUNTER(__xpg_strerror_r), 1);
	UT_ASSERTeq(Log_function_.rcounter, 1);

	return NO_ARGS_CONSUMED;
}

static struct test_case test_cases[] = {
	TEST_CASE(test_CORE_LOG_LEVEL_ERROR_LAST),
	TEST_CASE(test_vsnprintf_fail),
	TEST_CASE(test_NO_ERRNO),
	TEST_CASE(test_no_space_for_strerror_r),
	TEST_CASE(test_strerror_r_fail),
	TEST_CASE(test_level_gt_threshold),
	TEST_CASE(test_no_log_function),
	TEST_CASE(test_happy_day),
};

#define NTESTS ARRAY_SIZE(test_cases)

int
main(int argc, char *argv[])
{
	START(argc, argv, "core_log");
	core_log_set_function(log_function, DUMMY_CONTEXT);
	/*
	 * Once the logging function is attached no more genuine messages are
	 * expected so we can permanently switch to using the mock aka validate
	 * everything going in.
	 */
	Log_function_.use_mock = true;
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
