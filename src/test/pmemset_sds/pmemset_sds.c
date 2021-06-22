// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * pmemset_sds.c -- pmemset_sds unittests
 */

#include <string.h>

#include "config.h"
#include "fault_injection.h"
#include "libpmemset.h"
#include "out.h"
#include "part.h"
#include "sds.h"
#include "source.h"
#include "unittest.h"
#include "ut_pmemset_utils.h"

static void create_config(struct pmemset_config **cfg) {
	int ret = pmemset_config_new(cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);

	ret = pmemset_config_set_required_store_granularity(*cfg,
		PMEM2_GRANULARITY_PAGE);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);
}

/*
 * test_sds_new_enomem - test pmemset_sds allocation with error injection
 */
static int
test_sds_new_enomem(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_sds_new_enomem <path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_sds *sds;
	struct pmemset_source *src;
	struct pmemset_config *cfg;

	if (!core_fault_injection_enabled())
		return 1;

	create_config(&cfg);

	int ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	core_inject_fault_at(PMEM_MALLOC, 1, "pmemset_malloc");

	ret = pmemset_sds_new(&sds, src);
	UT_PMEMSET_EXPECT_RETURN(ret, -ENOMEM);
	UT_ASSERTeq(sds, NULL);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_sds_pool_in_use_wrong_usc - create new sds and map a part, then modify
 *                                  the usc in SDS and map a part again
 */
static int
test_sds_pool_in_use_wrong_usc(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_sds_pool_in_use_wrong_usc <path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_extras extras;
	struct pmemset_part *part;
	enum pmemset_part_state state;
	struct pmemset_sds *sds;
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_sds_new(&sds, src);
	if (ret == PMEMSET_E_NOSUPP)
		goto err_cleanup;

	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(sds, NULL);

	uint64_t acceptable_states = PMEMSET_PART_STATE_OK;
	ret = pmemset_sds_set_acceptable_states(sds, acceptable_states);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	extras.in_sds = sds;
	extras.in_bb = NULL;
	extras.out_state = &state;
	pmemset_source_set_extras(src, &extras);

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(part, NULL);

	/* no error, correct SDS values */
	ret = pmemset_part_map(&part, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	sds->usc += 1;

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(part, NULL);

	/* new SDS unsafe shutdown count doesn't match the old one */
	ret = pmemset_part_map(&part, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_UNDESIRABLE_PART_STATE);
	UT_ASSERTeq(state, PMEMSET_PART_STATE_CORRUPTED);

	ret = pmemset_part_delete(&part);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

err_cleanup:
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_sds_pool_not_in_use_wrong_usc - create new sds modify the usc in SDS and
 *                                      map a part
 */
static int
test_sds_pool_not_in_use_wrong_usc(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_sds_pool_not_in_use_wrong_usc <path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_extras extras;
	struct pmemset_part *part;
	enum pmemset_part_state state;
	struct pmemset_sds *sds;
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_sds_new(&sds, src);
	if (ret == PMEMSET_E_NOSUPP)
		goto err_cleanup;

	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(sds, NULL);

	uint64_t acceptable_states = PMEMSET_PART_STATE_OK;
	ret = pmemset_sds_set_acceptable_states(sds, acceptable_states);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	extras.in_sds = sds;
	extras.in_bb = NULL;
	extras.out_state = &state;
	pmemset_source_set_extras(src, &extras);

	sds->usc += 1;
	uint64_t old_usc = sds->usc;

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(part, NULL);

	/* new SDS unsafe shutdown count doesn't match the old one */
	ret = pmemset_part_map(&part, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(state, PMEMSET_PART_STATE_OK);
	/* if pool wasn't in use then usc should be reinitialized */
	UT_ASSERTne(sds->usc, old_usc);

	ret = pmemset_part_delete(&part);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

err_cleanup:
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_sds_pool_in_use_wrong_device_id - create new sds and map a part, then
 * the device ID in SDS and map a part again
 */
static int
test_sds_pool_in_use_wrong_device_id(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_sds_pool_in_use_wrong_device_id <path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_extras extras;
	struct pmemset_part *part;
	enum pmemset_part_state state;
	struct pmemset_sds *sds;
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_sds_new(&sds, src);
	if (ret == PMEMSET_E_NOSUPP)
		goto err_cleanup;

	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(sds, NULL);

	uint64_t acceptable_states = PMEMSET_PART_STATE_OK;
	ret = pmemset_sds_set_acceptable_states(sds, acceptable_states);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	extras.in_sds = sds;
	extras.in_bb = NULL;
	extras.out_state = &state;
	pmemset_source_set_extras(src, &extras);

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(part, NULL);

	/* no error, correct SDS values */
	ret = pmemset_part_map(&part, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	sds->id[0] += 1;

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(part, NULL);

	/* new SDS unsafe shutdown count doesn't match the old one */
	ret = pmemset_part_map(&part, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_UNDESIRABLE_PART_STATE);
	UT_ASSERTeq(state, PMEMSET_PART_STATE_INDETERMINATE);

	ret = pmemset_part_delete(&part);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

err_cleanup:
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_sds_pool_not_in_use_wrong_device_id - create new sds modify the device
 *                                            id in SDS and map a part
 */
static int
test_sds_pool_not_in_use_wrong_device_id(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_sds_pool_not_in_use_wrong_device_id " \
				"<path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_extras extras;
	struct pmemset_part *part;
	enum pmemset_part_state state;
	struct pmemset_sds *sds;
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_sds_new(&sds, src);
	if (ret == PMEMSET_E_NOSUPP)
		goto err_cleanup;

	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(sds, NULL);

	uint64_t acceptable_states = PMEMSET_PART_STATE_OK;
	ret = pmemset_sds_set_acceptable_states(sds, acceptable_states);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	extras.in_sds = sds;
	extras.in_bb = NULL;
	extras.out_state = &state;
	pmemset_source_set_extras(src, &extras);

	sds->id[0] += 1;
	char old_device_id[PMEMSET_SDS_DEVICE_ID_LEN];
	strncpy(old_device_id, sds->id, PMEMSET_SDS_DEVICE_ID_LEN);

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(part, NULL);

	/* new SDS unsafe shutdown count doesn't match the old one */
	ret = pmemset_part_map(&part, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(state, PMEMSET_PART_STATE_OK);
	/* if pool wasn't in use then device id should be reinitialized */
	UT_ASSERT(strncmp(sds->id, old_device_id,
			PMEMSET_SDS_DEVICE_ID_LEN) != 0);

	ret = pmemset_part_delete(&part);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

err_cleanup:
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_sds_pool_multiple_mappings - create new sds and map three parts one by
 *                                   one
 */
static int
test_sds_pool_multiple_mappings(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_sds_pool_normal_mappings " \
				"<path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_extras extras;
	struct pmemset_part *part;
	enum pmemset_part_state state;
	struct pmemset_sds *sds;
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	create_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_sds_new(&sds, src);
	if (ret == PMEMSET_E_NOSUPP)
		goto err_cleanup;

	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(sds, NULL);

	uint64_t acceptable_states = (PMEMSET_PART_STATE_OK |
			PMEMSET_PART_STATE_OK_BUT_INTERRUPTED);
	ret = pmemset_sds_set_acceptable_states(sds, acceptable_states);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	extras.in_sds = sds;
	extras.in_bb = NULL;
	extras.out_state = &state;
	pmemset_source_set_extras(src, &extras);

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(part, NULL);

	ret = pmemset_part_map(&part, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(state, PMEMSET_PART_STATE_OK);

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(part, NULL);

	ret = pmemset_part_map(&part, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(state, PMEMSET_PART_STATE_OK_BUT_INTERRUPTED);

	ret = pmemset_part_new(&part, set, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(part, NULL);

	ret = pmemset_part_map(&part, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(state, PMEMSET_PART_STATE_OK_BUT_INTERRUPTED);

	ret = pmemset_part_delete(&part);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

err_cleanup:
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_sds_new_enomem),
	TEST_CASE(test_sds_pool_in_use_wrong_usc),
	TEST_CASE(test_sds_pool_not_in_use_wrong_usc),
	TEST_CASE(test_sds_pool_in_use_wrong_device_id),
	TEST_CASE(test_sds_pool_not_in_use_wrong_device_id),
	TEST_CASE(test_sds_pool_multiple_mappings),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmemset_sds");

	util_init();
	out_init("pmemset_sds", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}

#ifdef _MSC_VER
MSVC_CONSTR(libpmemset_init)
MSVC_DESTR(libpmemset_fini)
#endif
