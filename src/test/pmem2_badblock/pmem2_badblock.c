// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmem2_badblock.c -- pmem2 bad block tests
 */

#include "libpmem2.h"
#include "unittest.h"
#include "ut_pmem2.h"

/*
 * test_pmem2_badblock_count -- counts the number of bb in the file
 */
static int
test_pmem2_badblock_count(const struct test_case *tc, int argc,
					char *argv[])
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
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_pmem2_badblock_count),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_badblock");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
