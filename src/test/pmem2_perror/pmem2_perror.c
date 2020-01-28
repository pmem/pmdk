// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmem2_perror.c -- pmem2_perror unittests
 */

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
 * fails through pmem2_source_file_size func
 */
static int
test_fail_pmem2_syscall_simple(const struct test_case *tc,
		int argc, char *argv[])
{
	struct pmem2_source src;
	size_t size;

#ifdef _WIN32
	src.handle = INVALID_HANDLE_VALUE;
#else
	src.fd = -1;
#endif

	/* "randomly" chosen function to be failed */
	int ret = pmem2_source_file_size(&src, &size);
	ASSERTne(ret, 0);

	pmem2_perror("test");

	return 0;
}

/*
 * test_fail_pmem2_syscall_simple - check print message when system func
 * fails through pmem2_source_file_size func and ellipsis operator is used
 */
static int
test_fail_pmem2_syscall_format(const struct test_case *tc,
		int argc, char *argv[])
{
	struct pmem2_source src;
	size_t size;

#ifdef _WIN32
	src.handle = INVALID_HANDLE_VALUE;
#else
	src.fd = -1;
#endif

	/* "randomly" chosen function to be failed */
	int ret = pmem2_source_file_size(&src, &size);
	ASSERTne(ret, 0);

	pmem2_perror("test %d", 123);

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
