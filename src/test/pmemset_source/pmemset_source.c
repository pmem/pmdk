// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmemset_perror.c -- pmemset_perror unittests
 */

#include "libpmemset.h"

#include "out.h"
#include "source.h"
#include "unittest.h"
#include "ut_pmemset_utils.h"

/*
 * test_source_set_path - check if file path is saved in structure
 */
static int
test_source_from_file(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_source_from_file <path>");

	const char *file = argv[0];

	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	UT_ASSERT(strcmp(src->filepath, file) == 0);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_source_from_file),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmemset_source");

	util_init();
	out_init("pmemset_source", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}
