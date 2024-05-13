// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * core_log_no_func.c -- unit test for core_log() and core_log_va() when
 * no logging function is attached.
 */

#include <stdbool.h>

#include "unittest.h"
#include "core_log_common.h"

/* tests */

/*
 * Check:
 * - if Core_log_function == 0:
 *   - the log function is not called
 */
static int
test_no_log_function(const struct test_case *tc, int argc, char *argv[])
{
	/* Pass the message all the way to the logging function. */
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_ERROR);

	test_log_function_call_helper(CORE_LOG_LEVEL_ERROR,
		false /* log_function_called */);

	return NO_ARGS_CONSUMED;
}

/*
 * Check:
 * - if Core_log_function == 0 and level == CORE_LOG_LEVEL_ERROR_LAST:
 *   - the log function is not called
 */
static int
test_no_log_function_CORE_LOG_LEVEL_ERROR_LAST(const struct test_case *tc,
	int argc, char *argv[])
{
	/* Pass the message all the way to the logging function. */
	core_log_set_threshold(CORE_LOG_THRESHOLD, CORE_LOG_LEVEL_ERROR);

	test_log_function_call_helper(CORE_LOG_LEVEL_ERROR_LAST,
		false /* log_function_called */);

	return NO_ARGS_CONSUMED;
}

static struct test_case test_cases[] = {
	TEST_CASE(test_no_log_function),
	TEST_CASE(test_no_log_function_CORE_LOG_LEVEL_ERROR_LAST),
};

#define NTESTS ARRAY_SIZE(test_cases)

int
main(int argc, char *argv[])
{
	START(argc, argv, "core_log_no_func");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
