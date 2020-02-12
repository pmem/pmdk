// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * pmem2_config_get_file_size.c -- pmem2_config_get_file_size unittests
 */

#include <stdint.h>

#include "fault_injection.h"
#include "unittest.h"
#include "ut_pmem2_utils.h"
#include "ut_pmem2_config.h"
#include "ut_fh.h"
#include "config.h"
#include "out.h"

typedef void (*test_fun)(const char *path, os_off_t size);

/*
 * test_notset_fd - tests what happens when file descriptor was not set
 */
static int
test_notset_fd(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	size_t size;
	int ret = pmem2_config_get_file_size(&cfg, &size);

	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_FILE_HANDLE_NOT_SET);

	return 0;
}

static void
init_cfg(struct pmem2_config *cfg, struct FHandle *f)
{
	pmem2_config_init(cfg);
	PMEM2_CONFIG_SET_FHANDLE(cfg, f);
}

/*
 * test_normal_file - tests normal file (common)
 */
static void
test_normal_file(const char *path, os_off_t expected_size,
		enum file_handle_type type)
{
	struct FHandle *fh = UT_FH_OPEN(type, path, FH_RDWR);

	struct pmem2_config cfg;
	init_cfg(&cfg, fh);

	size_t size = SIZE_MAX;
	int ret = pmem2_config_get_file_size(&cfg, &size);

	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(size, expected_size);

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
 * test_normal_file_handle - tests normal file using a HANDLE
 */
static int
test_normal_file_handle(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_normal_file_handle"
				" <file> <expected_size>");

	char *path = argv[0];
	os_off_t expected_size = ATOLL(argv[1]);

	test_normal_file(path, expected_size, FH_HANDLE);

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

	struct pmem2_config cfg;

	pmem2_config_init(&cfg);
	init_cfg(&cfg, fh);

	size_t size = SIZE_MAX;
	int ret = pmem2_config_get_file_size(&cfg, &size);

	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(size, requested_size);

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
 * test_tmpfile_handle - tests temporary file using file handle interface
 */
static int
test_tmpfile_handle(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_tmpfile_handle <file> <requested_size>");

	char *dir = argv[0];
	os_off_t requested_size = ATOLL(argv[1]);

	test_tmpfile(dir, requested_size, FH_HANDLE);

	return 2;
}

/*
 * test_directory - tests directory path (common)
 */
static void
test_directory(const char *dir, enum file_handle_type type)
{
	struct FHandle *fh = UT_FH_OPEN(type, dir, FH_RDONLY | FH_DIRECTORY);

	struct pmem2_config cfg;
	init_cfg(&cfg, fh);

	size_t size;
	int ret = pmem2_config_get_file_size(&cfg, &size);

	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_TYPE);

	UT_FH_CLOSE(fh);
}

/*
 * test_directory_fd - tests directory path using file descriptor interface
 */
static int
test_directory_fd(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_directory_fd <file>");

	char *dir = argv[0];

	test_directory(dir, FH_FD);

	return 1;
}

/*
 * test_directory_handle - tests directory path using file handle interface
 */
static int
test_directory_handle(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_directory_handle <file>");

	char *dir = argv[0];

	test_directory(dir, FH_HANDLE);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_notset_fd),
	TEST_CASE(test_normal_file_fd),
	TEST_CASE(test_normal_file_handle),
	TEST_CASE(test_tmpfile_fd),
	TEST_CASE(test_tmpfile_handle),
	TEST_CASE(test_directory_fd),
	TEST_CASE(test_directory_handle),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_config_get_file_size");

	util_init();
	out_init("pmem2_config_get_file_size", "TEST_LOG_LEVEL",
			"TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}
