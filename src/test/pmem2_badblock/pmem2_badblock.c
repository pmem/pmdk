// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * pmem2_badblock.c -- pmem2 bad block tests
 */

#include "libpmem2.h"
#include "unittest.h"
#include "ut_pmem2.h"

#include <errno.h>

/*
 * test_pmem2_badblock_count -- counts the number of bb in the file
 */
static int
test_pmem2_badblock_count(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_pmem2_badblock_count <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_source *src;
	int ret = pmem2_source_from_fd(&src, fd);
	UT_ASSERTeq(ret, 0);

	struct pmem2_badblock_context *bbctx;
	ret = pmem2_badblock_context_new(&bbctx, src);
	UT_ASSERTeq(ret, 0);

	int count = 0;
	struct pmem2_badblock bb;
	while ((ret = pmem2_badblock_next(bbctx, &bb)) == 0)
		++count;

	UT_OUT("BB: %d\n", count);

	pmem2_badblock_context_delete(&bbctx);

	pmem2_source_delete(&src);
	CLOSE(fd);

	return 1;
}

/*
 * test_pmem2_src_mcsafe_badblock_read -- test mcsafe read operation with
 *                                        encountered badblock
 */
static int
test_pmem2_src_mcsafe_badblock_read(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_pmem2_src_mcsafe_badblock_read <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_source *src;
	int ret = pmem2_source_from_fd(&src, fd);
	UT_ASSERTeq(ret, 0);

	size_t bufsize = 4096;
	void *buf = MALLOC(bufsize);
	ret = pmem2_source_pread_mcsafe(src, buf, bufsize, 0);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_IO_FAIL);

	pmem2_source_delete(&src);
	CLOSE(fd);

	return 1;
}

/*
 * test_pmem2_src_mcsafe_badblock_write -- test mcsafe write operation with
 *                                         encountered badblock
 */
static int
test_pmem2_src_mcsafe_badblock_write(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_pmem2_src_mcsafe_badblock_write <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_source *src;
	int ret = pmem2_source_from_fd(&src, fd);
	UT_ASSERTeq(ret, 0);

	size_t bufsize = 4096;
	void *buf = MALLOC(bufsize);
	ret = pmem2_source_pwrite_mcsafe(src, buf, bufsize, 0);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_IO_FAIL);

	pmem2_source_delete(&src);
	CLOSE(fd);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_pmem2_badblock_count),
	TEST_CASE(test_pmem2_src_mcsafe_badblock_read),
	TEST_CASE(test_pmem2_src_mcsafe_badblock_write),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_badblock");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
