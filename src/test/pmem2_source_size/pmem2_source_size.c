// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2023, Intel Corporation */

/*
 * pmem2_source_size.c -- pmem2_source_size unittests
 */

#include <stdint.h>

#include "fault_injection.h"
#include "unittest.h"
#include "ut_pmem2.h"
#include "ut_fh.h"
#include "config.h"
#include "out.h"

typedef void (*test_fun)(const char *path, os_off_t size);

/*
 * test_normal_file - tests normal file (common)
 */
static void
test_normal_file(const char *path, os_off_t expected_size,
		enum file_handle_type type)
{
	struct FHandle *fh = UT_FH_OPEN(type, path, FH_RDWR);

	struct pmem2_source *src;
	PMEM2_SOURCE_FROM_FH(&src, fh);

	size_t size;
	int ret = pmem2_source_size(src, &size);

	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(size, expected_size);

	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);
}

/*
 * test_normal_file_fd - tests normal file using a file descriptor
 */
static int
test_normal_file_fd(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_normal_file_fd <file> <expected_size>");

	char *path = argv[0];
	os_off_t expected_size = ATOLL(argv[1]);

	test_normal_file(path, expected_size, FH_FD);

	return 2;
}

/*
 * test_tmpfile - tests temporary file
 */
static void
test_tmpfile(const char *dir, os_off_t requested_size,
		enum file_handle_type type)
{
	struct FHandle *fh = UT_FH_OPEN(type, dir, FH_RDWR | FH_TMPFILE);
	UT_FH_TRUNCATE(fh, requested_size);

	struct pmem2_source *src;
	PMEM2_SOURCE_FROM_FH(&src, fh);

	size_t size = SIZE_MAX;
	int ret = pmem2_source_size(src, &size);

	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(size, requested_size);

	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);
}

/*
 * test_tmpfile_fd - tests temporary file using file descriptor interface
 */
static int
test_tmpfile_fd(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_tmpfile_fd <file> <requested_size>");

	char *dir = argv[0];
	os_off_t requested_size = ATOLL(argv[1]);

	test_tmpfile(dir, requested_size, FH_FD);

	return 2;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_normal_file_fd),
	TEST_CASE(test_tmpfile_fd),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_source_size");

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);

	DONE(NULL);
}
