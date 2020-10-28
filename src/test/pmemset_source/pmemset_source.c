// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmemset_source.c -- pmemset_source unittests
 */
#include "fault_injection.h"
#include "libpmemset.h"
#include "unittest.h"
#include "ut_pmemset_utils.h"
#include "out.h"
#include "source.h"

/*
 * test_set_from_pmem2_valid - test valid pmemset_source allocation
 */
static int
test_set_from_pmem2_valid(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_set_from_pmem2_valid <file>");

	char *file = argv[0];

	struct pmem2_source *src_pmem2;
	struct pmemset_source *src_set;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&src_pmem2, fd);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_source_from_pmem2(&src_set, src_pmem2);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src_set, NULL);

	CLOSE(fd);

	return 1;
}

/*
 * test_set_from_pmem2_null- test pmemset_source_from_pmem2 with null pmem2
 */
static int
test_set_from_pmem2_null(const struct test_case *tc, int argc, char *argv[])
{
	struct pmemset_source *src_set;

	int ret = pmemset_source_from_pmem2(&src_set, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_INVALID_PMEM2_SOURCE);
	UT_ASSERTeq(src_set, NULL);

	return 0;
}

/*
 * test_alloc_src_enomem - test pmemset_source allocation with error injection
 */
static int
test_alloc_src_enomem(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_alloc_src_enomem <file>");

	char *file = argv[0];

	struct pmem2_source *src_pmem2;
	struct pmemset_source *src_set;
	if (!core_fault_injection_enabled()) {
		return 1;
	}
	int fd = OPEN(file, O_RDWR);

	core_inject_fault_at(PMEM_MALLOC, 1, "pmemset_malloc");

	int ret = pmem2_source_from_fd(&src_pmem2, fd);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_source_from_pmem2(&src_set, src_pmem2);
	UT_PMEMSET_EXPECT_RETURN(ret, -ENOMEM);
	UT_ASSERTeq(src_set, NULL);
	CLOSE(fd);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_set_from_pmem2_null),
	TEST_CASE(test_alloc_src_enomem),
	TEST_CASE(test_set_from_pmem2_valid),
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
