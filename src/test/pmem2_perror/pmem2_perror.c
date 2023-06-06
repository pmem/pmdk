// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2023, Intel Corporation */

/*
 * pmem2_perror.c -- pmem2_perror unittests
 */

#include "libpmem2.h"
#include "unittest.h"
#include "out.h"
#include "config.h"
#include "source.h"

/*
 * test_fail_pmem2_func_simple - simply check print message when func
 * from pmem2 API fails
 */
static int
test_fail_pmem2_func_simple(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_config cfg;
	size_t offset = (size_t)INT64_MAX + 1;

	/* "randomly" chosen function to be failed */
	int ret = pmem2_config_set_offset(&cfg, offset);
	UT_ASSERTne(ret, 0);

	pmem2_perror("pmem2_config_set_offset");

	return 0;
}

/*
 * test_fail_pmem2_func_format - check print message when func
 * from pmem2 API fails and ellipsis operator is used
 */
static int
test_fail_pmem2_func_format(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_config cfg;
	size_t offset = (size_t)INT64_MAX + 1;

	/* "randomly" chosen function to be failed */
	int ret = pmem2_config_set_offset(&cfg, offset);
	UT_ASSERTne(ret, 0);

	pmem2_perror("pmem2_config_set_offset %d", 123);

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

	pmem2_perror("test");

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

	pmem2_perror("test %d", 123);

	return 0;
}

/*
 * test_fail_pmem2_syscall_simple - check print message when system func
 * fails through pmem2_source_size func
 */
static int
test_fail_pmem2_syscall_simple(const struct test_case *tc,
		int argc, char *argv[])
{
	struct pmem2_source src;
	size_t size;

	src.type = PMEM2_SOURCE_FD;
	src.value.fd = -1;

	/* "randomly" chosen function to be failed */
	int ret = pmem2_source_size(&src, &size);
	ASSERTne(ret, 0);

	pmem2_perror("test");

	return 0;
}

/*
 * test_fail_pmem2_syscall_simple - check print message when system func
 * fails through pmem2_source_size func and ellipsis operator is used
 */
static int
test_fail_pmem2_syscall_format(const struct test_case *tc,
		int argc, char *argv[])
{
	struct pmem2_source src;
	size_t size;

	src.type = PMEM2_SOURCE_FD;
	src.value.fd = -1;

	/* "randomly" chosen function to be failed */
	int ret = pmem2_source_size(&src, &size);
	ASSERTne(ret, 0);

	pmem2_perror("test %d", 123);

	return 0;
}

/*
 * test_simple_err_to_errno_check - check if conversion
 * from pmem2 err value to errno works fine
 */
static int
test_simple_err_to_errno_check(const struct test_case *tc,
		int argc, char *argv[])
{
	int ret_errno = pmem2_err_to_errno(PMEM2_E_NOSUPP);
	UT_ASSERTeq(ret_errno, ENOTSUP);

	ret_errno = pmem2_err_to_errno(PMEM2_E_UNKNOWN);
	UT_ASSERTeq(ret_errno, EINVAL);

	ret_errno = pmem2_err_to_errno(-ENOTSUP);
	UT_ASSERTeq(ret_errno, ENOTSUP);

	return 0;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_fail_pmem2_func_simple),
	TEST_CASE(test_fail_pmem2_func_format),
	TEST_CASE(test_fail_system_func_simple),
	TEST_CASE(test_fail_system_func_format),
	TEST_CASE(test_fail_pmem2_syscall_simple),
	TEST_CASE(test_fail_pmem2_syscall_format),
	TEST_CASE(test_simple_err_to_errno_check),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_perror");

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);

	DONE(NULL);
}
