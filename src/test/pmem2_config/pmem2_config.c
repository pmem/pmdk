// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * pmem_config.c -- pmem2_config unittests
 */
#include "fault_injection.h"
#include "unittest.h"
#include "ut_pmem2_utils.h"
#include "ut_pmem2_config.h"
#include "config.h"
#include "out.h"

/*
 * test_cfg_create_and_delete_valid - test pmem2_config allocation
 */
static int
test_cfg_create_and_delete_valid(const struct test_case *tc, int argc,
		char *argv[])
{
	struct pmem2_config *cfg;

	int ret = pmem2_config_new(&cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);

	ret = pmem2_config_delete(&cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(cfg, NULL);

	return 0;
}

/*
 * test_cfg_alloc_enomem - test pmem2_config allocation with error injection
 */
static int
test_alloc_cfg_enomem(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_config *cfg;
	if (!core_fault_injection_enabled()) {
		return 0;
	}
	core_inject_fault_at(PMEM_MALLOC, 1, "pmem2_malloc");

	int ret = pmem2_config_new(&cfg);
	UT_PMEM2_EXPECT_RETURN(ret, -ENOMEM);

	UT_ASSERTeq(cfg, NULL);

	return 0;
}

/*
 * test_delete_null_config - test pmem2_delete on NULL config
 */
static int
test_delete_null_config(const struct test_case *tc, int argc,
		char *argv[])
{
	struct pmem2_config *cfg = NULL;
	/* should not crash */
	int ret = pmem2_config_delete(&cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(cfg, NULL);

	return 0;
}

/*
 * test_config_set_granularity_valid - check valid granularity values
 */
static int
test_config_set_granularity_valid(const struct test_case *tc, int argc,
		char *argv[])
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);

	/* check default granularity */
	enum pmem2_granularity g =
		(enum pmem2_granularity)PMEM2_GRANULARITY_INVALID;
	UT_ASSERTeq(cfg.requested_max_granularity, g);

	/* change default granularity */
	int ret = -1;
	g = PMEM2_GRANULARITY_BYTE;
	ret = pmem2_config_set_required_store_granularity(&cfg, g);
	UT_ASSERTeq(cfg.requested_max_granularity, g);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	/* set granularity once more */
	ret = -1;
	g = PMEM2_GRANULARITY_PAGE;
	ret = pmem2_config_set_required_store_granularity(&cfg, g);
	UT_ASSERTeq(cfg.requested_max_granularity, g);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	return 0;
}

/*
 * test_config_set_granularity_invalid - check invalid granularity values
 */
static int
test_config_set_granularity_invalid(const struct test_case *tc, int argc,
		char *argv[])
{
	/* pass invalid granularity */
	int ret = 0;
	enum pmem2_granularity g_inval = 999;
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	ret = pmem2_config_set_required_store_granularity(&cfg, g_inval);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_GRANULARITY_NOT_SUPPORTED);

	return 0;
}

/*
 * test_set_offset_too_large - setting offset which is too large
 */
static int
test_set_offset_too_large(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_config cfg;

	/* let's try to set the offset which is too large */
	size_t offset = (size_t)INT64_MAX + 1;
	int ret = pmem2_config_set_offset(&cfg, offset);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OFFSET_OUT_OF_RANGE);

	return 0;
}

/*
 * test_set_offset_success - setting a valid offset
 */
static int
test_set_offset_success(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_config cfg;

	/* let's try to successfully set the offset */
	size_t offset = Ut_mmap_align;
	int ret = pmem2_config_set_offset(&cfg, offset);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(cfg.offset, offset);

	return 0;
}

/*
 * test_set_length_success - setting a valid length
 */
static int
test_set_length_success(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_config cfg;

	/* let's try to successfully set the length, can be any length */
	size_t length = Ut_mmap_align;
	int ret = pmem2_config_set_length(&cfg, length);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(cfg.length, length);

	return 0;
}

/*
 * test_set_offset_max - setting maximum possible offset
 */
static int
test_set_offset_max(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_config cfg;

	/* let's try to successfully set maximum possible offset */
	size_t offset = (INT64_MAX / Ut_mmap_align) * Ut_mmap_align;
	int ret = pmem2_config_set_offset(&cfg, offset);
	UT_ASSERTeq(ret, 0);

	return 0;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_cfg_create_and_delete_valid),
	TEST_CASE(test_alloc_cfg_enomem),
	TEST_CASE(test_delete_null_config),
	TEST_CASE(test_config_set_granularity_valid),
	TEST_CASE(test_config_set_granularity_invalid),
	TEST_CASE(test_set_offset_too_large),
	TEST_CASE(test_set_offset_success),
	TEST_CASE(test_set_length_success),
	TEST_CASE(test_set_offset_max),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_config");

	util_init();
	out_init("pmem2_config", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}
