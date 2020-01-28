// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmem2_perror.c -- pmem2_perror unittests
 */

#include "unittest.h"
#include "ut_pmem2_utils.h"
#include "out.h"

/*
 * test_simple_check - simply check print message
 */
static int
test_simple_check(const struct test_case *tc, int argc, char *argv[])
{
	/* "randomly" chosen errno value */
	errno = EINVAL;
	ERR("!");

	pmem2_perror("test");

	return 0;
}

/*
 * test_format_check - check print message with used ellipsis operator
 */
static int
test_format_check(const struct test_case *tc, int argc, char *argv[])
{
	/* "randomly" chosen errno value */
	errno = EISDIR;
	ERR("!");

	pmem2_perror("test %d", 123);

	return 0;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_simple_check),
	TEST_CASE(test_format_check),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_perror");

	util_init();
	out_init("pmem2_perror", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}
