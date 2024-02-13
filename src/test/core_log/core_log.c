// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * core_log.c -- unit test for core_log
 */

#undef _GNU_SOURCE
#include <string.h>
#include <stdbool.h>

#include "log_internal.h"
#include "unittest.h"
#include "unittest.h"
#include "call_all.h"

#define NO_ARGS_CONSUMED 0

#define BIG_BUF_SIZE 1024

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
	 * The key aim of this test is to make sure the assumed errno message
	 * buffer size is big enough no matter the errno value.
	 */
	UT_ASSERT(max_strerror_len + 1 <= _CORE_LOG_MAX_ERRNO_MSG);

	/*
	 * Other tests in this group makes use of this value so just make sure
	 * the generated strerror will be as long as it is possible.
	 */
	UT_ASSERTeq(max_streerror_num, MAX_STRERROR_NUM);

	return NO_ARGS_CONSUMED;
}

static void
log_function(void *context, enum core_log_level level,
	const char *file_name, const int line_no, const char *function_name,
	const char *message)
{
	size_t *max_message_len = (size_t *)context;
	size_t len = strlen(message);

	if (len > *max_message_len) {
		*max_message_len = len;
	}
}

static int
test_ERR_W_ERRNO(const struct test_case *tc, int argc, char *argv[])
{
	size_t max_message_len = 0;
	core_log_set_function(log_function, &max_message_len);

	call_all_CORE_LOG_ERROR_LAST();
	call_all_ERR_WO_ERRNO();
	call_all_CORE_LOG_ERROR_W_ERRNO_LAST(MAX_STRERROR_NUM);
	call_all_ERR_W_ERRNO(MAX_STRERROR_NUM);

	UT_ASSERTeq(max_message_len, 0); /* XXX */

	return NO_ARGS_CONSUMED;
}

static struct test_case test_cases[] = {
	TEST_CASE(test_CORE_LOG_MAX_ERRNO_MSG),
	TEST_CASE(test_ERR_W_ERRNO),
};

#define NTESTS ARRAY_SIZE(test_cases)

int
main(int argc, char *argv[])
{
	START(argc, argv, "core_log");

	UT_COMPILE_ERROR_ON(sizeof(PATH) != 128);

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
