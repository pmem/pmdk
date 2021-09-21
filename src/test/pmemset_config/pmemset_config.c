// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * pmemset_config.c -- pmemset_config unittests
 */
#include "fault_injection.h"
#include "libpmemset.h"
#include "unittest.h"
#include "ut_pmemset_utils.h"
#include "out.h"
#include "config.h"

/*
 * test_cfg_create_and_delete_valid - test pmemset_config allocation
 */
static int
test_cfg_create_and_delete_valid(const struct test_case *tc, int argc,
		char *argv[])
{
	struct pmemset_config *cfg;

	int ret = pmemset_config_new(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);

	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(cfg, NULL);

	return 0;
}

/*
 * test_alloc_cfg_enomem - test pmemset_config allocation with error injection
 */
static int
test_alloc_cfg_enomem(const struct test_case *tc, int argc, char *argv[])
{
	struct pmemset_config *cfg;
	if (!core_fault_injection_enabled()) {
		return 0;
	}
	core_inject_fault_at(PMEM_MALLOC, 1, "pmemset_malloc");

	int ret = pmemset_config_new(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, -ENOMEM);

	UT_ASSERTeq(cfg, NULL);

	return 0;
}

/*
 * test_delete_null_config - test pmemset_delete on NULL config
 */
static int
test_delete_null_config(const struct test_case *tc, int argc,
		char *argv[])
{
	struct pmemset_config *cfg = NULL;
	/* should not crash */
	int ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(cfg, NULL);

	return 0;
}

/*
 * test_duplicate_cfg_enomem - test pmemset_duplicate with error injection
 */
static int
test_duplicate_cfg_enomem(const struct test_case *tc, int argc, char *argv[])
{
	struct pmemset_config *src_cfg;
	struct pmemset_config *dst_cfg = NULL;

	if (!core_fault_injection_enabled())
		return 0;

	int ret = pmemset_config_new(&src_cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src_cfg, NULL);

	core_inject_fault_at(PMEM_MALLOC, 1, "pmemset_malloc");

	ret = pmemset_config_duplicate(&dst_cfg, src_cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, -ENOMEM);

	ret = pmemset_config_delete(&src_cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src_cfg, NULL);

	return 0;
}

/*
 * test_set_invalid_granularity - test set inval granularity in the config
 */
static int
test_set_invalid_granularity(const struct test_case *tc, int argc,
		char *argv[])
{
	struct pmemset_config *cfg;

	int ret = pmemset_config_new(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);

	ret = pmemset_config_set_required_store_granularity(cfg, 999);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_GRANULARITY_NOT_SUPPORTED);
	UT_ASSERTne(cfg, NULL);

	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(cfg, NULL);

	return 0;
}

#define SET_PTR ((struct pmemset *)0xFFBADFF)
#define CTX_PTR ((struct pmemset_event_context *)0xFFBAD)
#define ARG_PTR ((void *)0xBADBADBAD)

static int
callback(struct pmemset *set, struct pmemset_event_context *ctx, void *arg)
{

	static int Counter;
	UT_ASSERTeq(arg, ARG_PTR);
	UT_ASSERTeq(set, SET_PTR);
	UT_ASSERTeq(ctx, CTX_PTR);
	return ++Counter;

}

/*
 * test_config_set_event - test setting events
 */
static int
test_config_set_event(const struct test_case *tc, int argc,
	char *argv[]) {

	COMPILE_ERROR_ON(sizeof(struct pmemset_event_copy)
		> PMEMSET_EVENT_CONTEXT_SIZE);
	COMPILE_ERROR_ON(sizeof(struct pmemset_event_flush)
		> PMEMSET_EVENT_CONTEXT_SIZE);
	COMPILE_ERROR_ON(sizeof(struct pmemset_event_persist)
		> PMEMSET_EVENT_CONTEXT_SIZE);
	COMPILE_ERROR_ON(sizeof(struct pmemset_event_part_remove)
		> PMEMSET_EVENT_CONTEXT_SIZE);
	COMPILE_ERROR_ON(sizeof(struct pmemset_event_part_add)
		> PMEMSET_EVENT_CONTEXT_SIZE);
	COMPILE_ERROR_ON(sizeof(struct pmemset_event_sds_update)
		> PMEMSET_EVENT_CONTEXT_SIZE);
	COMPILE_ERROR_ON(sizeof(struct pmemset_event_badblock)
		> PMEMSET_EVENT_CONTEXT_SIZE);

	struct pmemset_config *cfg;

	int ret = pmemset_config_new(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);

	ret = pmemset_config_event_callback(cfg, SET_PTR, CTX_PTR);
	UT_ASSERTeq(ret, 0);

	pmemset_config_set_event_callback(cfg, &callback, ARG_PTR);

	ret = pmemset_config_event_callback(cfg, SET_PTR, CTX_PTR);
	UT_ASSERTeq(ret, 1);
	return 0;

}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_cfg_create_and_delete_valid),
	TEST_CASE(test_alloc_cfg_enomem),
	TEST_CASE(test_delete_null_config),
	TEST_CASE(test_duplicate_cfg_enomem),
	TEST_CASE(test_set_invalid_granularity),
	TEST_CASE(test_config_set_event),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmemset_config");

	util_init();
	out_init("pmemset_config", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}
