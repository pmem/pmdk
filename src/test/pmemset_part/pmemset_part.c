// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmemset_perror.c -- pmemset_perror unittests
 */

#include "libpmemset.h"

#include <string.h>

#include "out.h"
#include "source.h"
#include "unittest.h"
#include "ut_pmemset_utils.h"

static void
source_allocate(struct pmemset_source **src)
{
	struct pmemset_source *srcp = malloc(sizeof(struct pmemset_source));
	*src = srcp;
}

/*
 * test_part_new_invalid_source_path - create a new part from a source
 * with invalid path assigned.
 */
static int
test_part_new_invalid_source_path(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_new_valid_source_path <path>");

	const char *file = argv[0];

	struct pmemset_source *src;
	source_allocate(&src);
	src->filepath = strdup(file);

	int ret = pmemset_part_new(NULL, NULL, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_INVALID_FILE_PATH);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_part_new_invalid_source_path),
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
