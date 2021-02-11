// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * pmemset_perror.c -- pmemset_perror unittests
 */

#include "libpmemset.h"
#include "file.h"
#include "source.h"
#include "out.h"
#include "unittest.h"
#include "ut_pmemset_utils.h"

/*
 * test_fail_pmemset_func_simple - simply check print message when func from
 * pmemset API fails.
 */
static int
test_fail_pmemset_func_simple(const struct test_case *tc, int argc,
		char *argv[])
{
	struct pmemset_source *src;

	/* "randomly" chosen function to be failed */
	int ret = pmemset_source_from_file(&src, NULL, 0);
	UT_ASSERTne(ret, 0);

	pmemset_perror("pmemset_source_from_file");

	return 0;
}

/*
 * test_fail_pmemset_func_format - check print message when func
 * from pmemset API fails and ellipsis operator is used
 */
static int
test_fail_pmemset_func_format(const struct test_case *tc, int argc,
		char *argv[])
{
	struct pmemset_source *src;

	/* "randomly" chosen function to be failed */
	int ret = pmemset_source_from_file(&src, NULL, 0);
	UT_ASSERTne(ret, 0);

	pmemset_perror("pmemset_source_from_file %d", 123);

	return 0;
}

/*
 * test_fail_system_func_simple - check print message when directly called
 * system func fails
 */
static int
test_fail_system_func_simple(const struct test_case *tc, int argc, char *argv[])
{
	/* "randomly" chosen function to be failed */
	int ret = os_open("XXX", O_RDONLY);
	UT_ASSERTeq(ret, -1);
	ERR("!open");

	pmemset_perror("test");

	return 0;
}

/*
 * test_fail_system_func_format - check print message when directly called
 * system func fails and ellipsis operator is used
 */
static int
test_fail_system_func_format(const struct test_case *tc, int argc, char *argv[])
{
	/* "randomly" chosen function to be failed */
	int ret = os_open("XXX", O_RDONLY);
	UT_ASSERTeq(ret, -1);
	ERR("!open");

	pmemset_perror("test %d", 123);

	return 0;
}

/*
 * test_pmemset_err_to_errno_simple - check if conversion
 * from pmemset err value to errno works fine
 */
static int
test_pmemset_err_to_errno_simple(const struct test_case *tc,
		int argc, char *argv[])
{
	int ret_errno = pmemset_err_to_errno(PMEMSET_E_NOSUPP);
	UT_ASSERTeq(ret_errno, ENOTSUP);

	ret_errno = pmemset_err_to_errno(PMEMSET_E_UNKNOWN);
	UT_ASSERTeq(ret_errno, EINVAL);

	ret_errno = pmemset_err_to_errno(-ENOTSUP);
	UT_ASSERTeq(ret_errno, ENOTSUP);

	return 0;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_fail_pmemset_func_simple),
	TEST_CASE(test_fail_pmemset_func_format),
	TEST_CASE(test_fail_system_func_simple),
	TEST_CASE(test_fail_system_func_format),
	TEST_CASE(test_pmemset_err_to_errno_simple),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmemset_perror");

	util_init();
	out_init("pmemset_perror", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}
