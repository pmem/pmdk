/*
 * Copyright 2019-2020, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * pmem2_source_file_size.c -- pmem2_source_file_size unittests
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
	int ret = pmem2_source_file_size(src, &size);

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

	struct pmem2_source *src;
	PMEM2_SOURCE_FROM_FH(&src, fh);

	size_t size = SIZE_MAX;
	int ret = pmem2_source_file_size(src, &size);

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
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_normal_file_fd),
	TEST_CASE(test_normal_file_handle),
	TEST_CASE(test_tmpfile_fd),
	TEST_CASE(test_tmpfile_handle),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_source_file_size");

	util_init();
	out_init("pmem2_source_file_size", "TEST_LOG_LEVEL",
			"TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}
