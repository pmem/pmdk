// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmemset_config.c -- pmemset_config unittests
 */
#include "fault_injection.h"
#include "libpmemset.h"
#include "unittest.h"
#include "ut_pmemset_utils.h"
#include "out.h"
#include "config.h"
#include "ravl_interval.h"

/*
 * test_new_create_and_delete_valid - test pmemset_new allocation
 */
static int
test_new_create_and_delete_valid(const struct test_case *tc, int argc,
		char *argv[])
{
	struct pmemset_config *cfg;
	struct pmemset *set;
	pmemset_config_new(&cfg);

	int ret = pmemset_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(set, NULL);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(set, NULL);

	pmemset_config_delete(&cfg);

	return 0;
}

/*
 * test_alloc_new_enomem - test pmemset_new allocation with error injection in
 * set allocation
 */
static int
test_alloc_new_enomem(const struct test_case *tc, int argc, char *argv[])
{
	struct pmemset_config *cfg;
	struct pmemset *set;

	pmemset_config_new(&cfg);
	int ret = pmemset_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	if (!core_fault_injection_enabled())
		return 0;

	core_inject_fault_at(PMEM_MALLOC, 1, "pmemset_malloc");

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, -ENOMEM);
	UT_ASSERTeq(set, NULL);

	pmemset_config_delete(&cfg);

	return 0;
}

/*
 * test_alloc_new_tree_enomem - test pmemset_new allocation with error
 * injection in tree allocation
 */
static int
test_alloc_new_tree_enomem(const struct test_case *tc, int argc, char *argv[])
{
	struct pmemset_config *cfg;
	struct pmemset *set;

	pmemset_config_new(&cfg);
	int ret = pmemset_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	if (!core_fault_injection_enabled())
		return 0;

	core_inject_fault_at(PMEM_MALLOC, 1, "ravl_interval_new");

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, -ENOMEM);
	UT_ASSERTeq(set, NULL);

	pmemset_config_delete(&cfg);

	return 0;
}

/*
 * test_delete_null_set - test pmemset_delete on NULL set
 */
static int
test_delete_null_set(const struct test_case *tc, int argc,
		char *argv[])
{
	struct pmemset *set = NULL;
	/* should not crash */
	int ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(set, NULL);

	return 0;
}

/*
 * test_granularity_not_set - test pmemset_new without granularity
 */
static int
test_granularity_not_set(const struct test_case *tc, int argc,
		char *argv[])
{
	struct pmemset_config *cfg;
	struct pmemset *set;
	pmemset_config_new(&cfg);

	int ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_GRANULARITY_NOT_SET);

	pmemset_config_delete(&cfg);

	return 0;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_new_create_and_delete_valid),
	TEST_CASE(test_alloc_new_enomem),
	TEST_CASE(test_alloc_new_tree_enomem),
	TEST_CASE(test_delete_null_set),
	TEST_CASE(test_granularity_not_set),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmemset_new");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
