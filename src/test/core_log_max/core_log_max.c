// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * core_log_max.c -- unit test to verify max size of log buffers
 */

#undef _GNU_SOURCE
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "log_internal.h"
#include "last_error_msg.h"
#include "unittest.h"
#include "call_all.h"

#define NO_ARGS_CONSUMED 0

#define BIG_BUF_SIZE 4096

#define MAX_STRERROR_NUM 0x54

static void
call_all_strerror_r(int *max_strerror_len, int *max_streerror_num)
{
	char buf[BIG_BUF_SIZE];
	int correct_strerror_calls = 0;

	*max_strerror_len = 0;
	*max_streerror_num = 0;

	/*
	 * In general, valid errno values are all positive values of type int,
	 * but at the time of writing only the first 134 values are allocated.
	 * Out of which, 2 are not implemented hence 132 ought to be available.
	 * If not as expected please review the assumptions.
	 */
	for (int errnum = 0; errnum < 256; ++errnum) {
		int ret = strerror_r(errnum, buf, BIG_BUF_SIZE);

		/*
		 * It is not forced on strerror_r(3) to end up correctly to
		 * accommodate not-implemented errno values already existing in
		 * Linux and to freely go over the biggest errno value known at
		 * the time of writing this comment and potentially discover
		 * newly introduced values.
		 */
		if (ret != 0) {
			continue;
		}

		++correct_strerror_calls;

		int len = strlen(buf);
		if (len > *max_strerror_len) {
			*max_strerror_len = len;
			*max_streerror_num = errnum;
		}
	}
	UT_ASSERT(correct_strerror_calls == 132);
}

/* tests */

/*
 * test_CORE_LOG_MAX_ERRNO_MSG --
 * _CORE_LOG_MAX_ERRNO_MSG >= max(strlen(strerror(errnnum)))
 *     for errnum in errnums
 */
static int
test_CORE_LOG_MAX_ERRNO_MSG(const struct test_case *tc, int argc, char *argv[])
{
	int max_strerror_len = 0;
	int max_streerror_num = 0;

	call_all_strerror_r(&max_strerror_len, &max_streerror_num);

	/*
	 * The assumed errno message buffer size is big enough no matter
	 * the errno value.
	 */
	UT_ASSERT(max_strerror_len + 1 <= _CORE_LOG_MAX_ERRNO_MSG);

	/*
	 * Other tests in this group makes use of this value so just make sure
	 * the generated strerror will be as long as it is possible.
	 */
	UT_ASSERTeq(max_streerror_num, MAX_STRERROR_NUM);

	return NO_ARGS_CONSUMED;
}

static int Max_TLS_message_len;
static char The_longest_TLS_message[BIG_BUF_SIZE];
static int Total_TLS_message_num;

/*
 * A hard-coded value as obtained when the call_all_*() source code was
 * generated.
 */
#define TOTAL_TLS_MESSAGE_NUM_EXPECTED 311

static int
test_ERR_W_ERRNO(const struct test_case *tc, int argc, char *argv[])
{
	Max_TLS_message_len = 0;
	Total_TLS_message_num = 0;

	call_all_CORE_LOG_ERROR_LAST();
	call_all_ERR_WO_ERRNO();
	call_all_CORE_LOG_ERROR_W_ERRNO_LAST(MAX_STRERROR_NUM);
	call_all_ERR_W_ERRNO(MAX_STRERROR_NUM);

	UT_OUT("The_longest_TLS_message: %s", The_longest_TLS_message);
	UT_ASSERTeq(Total_TLS_message_num, TOTAL_TLS_MESSAGE_NUM_EXPECTED);
	UT_ASSERTeq(Max_TLS_message_len + 1, CORE_LAST_ERROR_MSG_MAXPRINT);

	return NO_ARGS_CONSUMED;
}

#define TOTAL_MESSAGE_NUM_EXPECTED 213
static int Max_message_len = 0;
static int Total_message_num = 0;
static char The_longest_message[BIG_BUF_SIZE];

FUNC_MOCK(core_log, void, enum core_log_level level, int errnum,
	const char *file_name, unsigned line_no, const char *function_name,
	const char *message_format, ...)
	FUNC_MOCK_RUN_DEFAULT {
		char buf[BIG_BUF_SIZE] = "";
		va_list arg;
		va_start(arg, message_format);
		int ret = vsnprintf(buf, BIG_BUF_SIZE, message_format, arg);
		UT_ASSERT(ret > 0);
		UT_ASSERTeq(ret, strlen(buf));
		if (errnum != NO_ERRNO)
			ret += _CORE_LOG_MAX_ERRNO_MSG;

		if (level == CORE_LOG_LEVEL_ERROR_LAST) {
			if (ret > Max_TLS_message_len) {
				Max_TLS_message_len = ret;
				strncpy(The_longest_TLS_message, buf,
					BIG_BUF_SIZE);
			}
			++Total_TLS_message_num;
		} else {
			if (ret > Max_message_len) {
				Max_message_len = ret;
			strncpy(The_longest_message, buf, BIG_BUF_SIZE);
			}
			++Total_message_num;
		}
		return;
	}
FUNC_MOCK_END

static int
test_CORE_LOG(const struct test_case *tc, int argc, char *argv[])
{
	Max_message_len = 0;
	Total_message_num = 0;

	call_all_CORE_LOG_WARNING();
	call_all_CORE_LOG_WARNING_W_ERRNO(MAX_STRERROR_NUM);
	call_all_CORE_LOG_ERROR();
	call_all_CORE_LOG_ERROR_W_ERRNO(MAX_STRERROR_NUM);
	call_all_CORE_LOG_FATAL();
	call_all_CORE_LOG_FATAL_W_ERRNO(MAX_STRERROR_NUM);

	UT_OUT("The_longest_message: %s", The_longest_message);
/*
 * + 1 for '\0' and another
 * + 1 as a means for detecting too-long log messages.
 * Please see _CORE_LOG_MSG_MAXPRINT for details.
 */
	UT_ASSERTeq(Max_message_len + 2, _CORE_LOG_MSG_MAXPRINT);
	UT_ASSERTeq(Total_message_num, TOTAL_MESSAGE_NUM_EXPECTED);

	return NO_ARGS_CONSUMED;
}

static struct test_case test_cases[] = {
	TEST_CASE(test_CORE_LOG_MAX_ERRNO_MSG),
	TEST_CASE(test_ERR_W_ERRNO),
	TEST_CASE(test_CORE_LOG),
};

#define NTESTS ARRAY_SIZE(test_cases)

int
main(int argc, char *argv[])
{
	START(argc, argv, "core_log_max");

	UT_COMPILE_ERROR_ON(sizeof(PATH) != 128);

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
