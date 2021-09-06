// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * pmemset_badblock.c -- pmemset bad block tests
 */

#include "libpmemset.h"
#include "unittest.h"
#include "ut_pmemset_utils.h"

#include <errno.h>

/*
 * test_pmemset_src_mcsafe_badblock_read -- test mcsafe read operation with
 *                                          encountered badblock
 */
static int
test_pmemset_src_mcsafe_badblock_read(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_pmemset_src_mcsafe_badblock_read <file>");

	char *file = argv[0];
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	size_t bufsize = 4096;
	void *buf = MALLOC(bufsize);
	ret = pmemset_source_pread_mcsafe(src, buf, bufsize, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_IO_FAIL);

	FREE(buf);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_pmemset_src_mcsafe_badblock_write -- test mcsafe write operation with
 *                                           encountered badblock
 */
static int
test_pmemset_src_mcsafe_badblock_write(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_pmemset_src_mcsafe_badblock_write <file>");

	char *file = argv[0];
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	size_t bufsize = 4096;
	void *buf = MALLOC(bufsize);
	memset(buf, '6', bufsize);
	ret = pmemset_source_pread_mcsafe(src, buf, bufsize, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_IO_FAIL);

	FREE(buf);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_pmemset_src_mcsafe_badblock_read),
	TEST_CASE(test_pmemset_src_mcsafe_badblock_write),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmemset_badblock");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
