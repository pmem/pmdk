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

#define NO_ARGS_CONSUMED 0

#define BIG_BUF_SIZE 1024

/*
 * test_CORE_LOG_MAX_ERRNO_MSG --
 * _CORE_LOG_MAX_ERRNO_MSG >= max(strlen(strerror(errnnum)))
 *     for errnum in errnums
 */
static int
test_CORE_LOG_MAX_ERRNO_MSG(const struct test_case *tc, int argc, char *argv[])
{
	char buf[BIG_BUF_SIZE];
	int max_strerror_len = 0;
	int correct_strerror_calls = 0;

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
		if (len > max_strerror_len) {
			max_strerror_len = len;
		}
	}
	UT_ASSERT(correct_strerror_calls == 132);

	/*
	 * The key aim of this test is to make sure the assumed errno message
	 * buffer size is big enough no matter the errno value.
	 */
	UT_ASSERT(max_strerror_len + 1 <= _CORE_LOG_MAX_ERRNO_MSG);

	return NO_ARGS_CONSUMED;
}

static struct test_case test_cases[] = {
	TEST_CASE(test_CORE_LOG_MAX_ERRNO_MSG)
};

#define NTESTS ARRAY_SIZE(test_cases)

int
main(int argc, char *argv[])
{
	START(argc, argv, "core_log");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
