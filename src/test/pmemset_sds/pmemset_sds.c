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

struct pmemset_sds *pmemset_source_get_sds(struct pmemset_source *src);

/*
 * test_source_set_sds_duplicate_enomem - test pmemset_sds allocation with error
 *                                        injection
 */
static int
test_source_set_sds_duplicate_enomem(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_source_set_sds_duplicate_enomem <path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_sds sds = PMEMSET_SDS_INITIALIZE();
	struct pmemset_source *src;
	struct pmemset_config *cfg;

	if (!core_fault_injection_enabled())
		return 1;

	ut_create_set_config(&cfg);

	int ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	core_inject_fault_at(PMEM_MALLOC, 1, "pmemset_malloc");

	ret = pmemset_source_set_sds(src, &sds, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, -ENOMEM);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

#define PMEMSET_SDS_INITIALIZE() { \
	.id = {0}, \
	.usc = 0, \
	.refcount = 0 \
}

/*
 * test_sds_part_in_use_wrong_usc - create new sds and map a part, then modify
 *                                  the usc in SDS and map a part again
 */
static int
test_sds_part_in_use_wrong_usc(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_sds_part_in_use_wrong_usc <path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_config *cfg;
	enum pmemset_part_state state;
	struct pmemset_sds sds = PMEMSET_SDS_INITIALIZE();
	struct pmemset_sds *sds_copy;
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ut_create_set_config(&cfg);

	uint64_t acceptable_states = PMEMSET_PART_STATE_OK;
	ret = pmemset_config_set_acceptable_states(cfg, acceptable_states);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_set_sds(src, &sds, &state);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_setup_source(&src, 0, 0);

	ret = pmemset_map(set, src, NULL);
	if (ret == PMEMSET_E_SDS_ENOSUPP)
		goto err_cleanup;
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* get duplicated SDS (internal function) */
	sds_copy = pmemset_source_get_sds(src);
	/* spoil usc */
	sds_copy->usc += 1;

	/* new SDS unsafe shutdown count doesn't match the old one */
	ret = pmemset_map(set, src, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_UNDESIRABLE_PART_STATE);
	UT_ASSERTeq(state, PMEMSET_PART_STATE_CORRUPTED);

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
 * test_sds_part_not_in_use_wrong_usc - create new sds modify the usc in SDS and
 *                                      map a part
 */
static int
test_sds_part_not_in_use_wrong_usc(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_sds_part_not_in_use_wrong_usc <path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_config *cfg;
	enum pmemset_part_state state;
	struct pmemset_sds sds = PMEMSET_SDS_INITIALIZE();
	struct pmemset_sds *sds_copy;
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ut_create_set_config(&cfg);

	uint64_t acceptable_states = PMEMSET_PART_STATE_OK;
	ret = pmemset_config_set_acceptable_states(cfg, acceptable_states);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* SDS contents were duplicated */
	pmemset_source_set_sds(src, &sds, &state);

	/* get duplicated SDS (internal function) */
	sds_copy = pmemset_source_get_sds(src);
	/* spoil usc */
	sds_copy->usc += 1;
	uint64_t old_usc = sds_copy->usc;

	ut_setup_source(&src, 0, 0);

	/* new SDS unsafe shutdown count doesn't match the old one */
	ret = pmemset_map(set, src, NULL);
	if (ret == PMEMSET_E_SDS_ENOSUPP)
		goto err_cleanup;
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(state, PMEMSET_PART_STATE_OK);
	/* if part wasn't in use then usc should be reinitialized */
	UT_ASSERTne(sds_copy->usc, old_usc);

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
 * test_sds_part_in_use_wrong_device_id - create new sds and map a part, then
 * the device ID in SDS and map a part again
 */
static int
test_sds_part_in_use_wrong_device_id(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_sds_part_in_use_wrong_device_id <path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_config *cfg;
	enum pmemset_part_state state;
	struct pmemset_sds sds = PMEMSET_SDS_INITIALIZE();
	struct pmemset_sds *sds_copy;
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ut_create_set_config(&cfg);

	uint64_t acceptable_states = PMEMSET_PART_STATE_OK;
	ret = pmemset_config_set_acceptable_states(cfg, acceptable_states);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_source_set_sds(src, &sds, &state);

	ut_setup_source(&src, 0, 0);

	/* no error, correct SDS values */
	ret = pmemset_map(set, src, NULL);
	if (ret == PMEMSET_E_SDS_ENOSUPP)
		goto err_cleanup;
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* get duplicated SDS (internal function) */
	sds_copy = pmemset_source_get_sds(src);
	/* spoil device id */
	sds_copy->id[0] += 1;

	/* new SDS unsafe shutdown count doesn't match the old one */
	ret = pmemset_map(set, src, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_UNDESIRABLE_PART_STATE);
	UT_ASSERTeq(state, PMEMSET_PART_STATE_INDETERMINATE);

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
 * test_sds_part_not_in_use_wrong_device_id - create new sds modify the device
 *                                            id in SDS and map a part
 */
static int
test_sds_part_not_in_use_wrong_device_id(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_sds_part_not_in_use_wrong_device_id " \
				"<path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_config *cfg;
	enum pmemset_part_state state;
	struct pmemset_sds sds = PMEMSET_SDS_INITIALIZE();
	struct pmemset_sds *sds_copy;
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ut_create_set_config(&cfg);

	uint64_t acceptable_states = PMEMSET_PART_STATE_OK;
	ret = pmemset_config_set_acceptable_states(cfg, acceptable_states);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_source_set_sds(src, &sds, &state);

	/* get duplicated SDS (internal function) */
	sds_copy = pmemset_source_get_sds(src);
	/* spoil device id */
	sds_copy->id[0] += 1;

	char old_device_id[PMEMSET_SDS_DEVICE_ID_LEN];
	strncpy(old_device_id, sds_copy->id, PMEMSET_SDS_DEVICE_ID_LEN);

	ut_setup_source(&src, 0, 0);

	/* new SDS unsafe shutdown count doesn't match the old one */
	ret = pmemset_map(set, src, NULL);
	if (ret == PMEMSET_E_SDS_ENOSUPP)
		goto err_cleanup;
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(state, PMEMSET_PART_STATE_OK);
	/* if part wasn't in use then device id should be reinitialized */
	UT_ASSERT(strncmp(sds_copy->id, old_device_id,
			PMEMSET_SDS_DEVICE_ID_LEN) != 0);

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
 * test_sds_part_multiple_mappings - create new sds and map three parts one by
 *                                   one
 */
static int
test_sds_part_multiple_mappings(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_sds_part_normal_mappings " \
				"<path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_config *cfg;
	enum pmemset_part_state state;
	struct pmemset_sds sds = PMEMSET_SDS_INITIALIZE();
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ut_create_set_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_source_set_sds(src, &sds, &state);

	ut_setup_source(&src, 0, 0);

	ret = pmemset_map(set, src, NULL);
	if (ret == PMEMSET_E_SDS_ENOSUPP)
		goto err_cleanup;
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(state, PMEMSET_PART_STATE_OK);

	ret = pmemset_map(set, src, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	/*
	 * SDS changes after first mapping, its in use, therefore every
	 * next mapping from this SDS should return an already open state
	 */
	UT_ASSERTeq(state, PMEMSET_PART_STATE_OK_BUT_ALREADY_OPEN);

	ret = pmemset_map(set, src, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(state, PMEMSET_PART_STATE_OK_BUT_ALREADY_OPEN);

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
	TEST_CASE(test_source_set_sds_duplicate_enomem),
	TEST_CASE(test_sds_part_in_use_wrong_usc),
	TEST_CASE(test_sds_part_not_in_use_wrong_usc),
	TEST_CASE(test_sds_part_in_use_wrong_device_id),
	TEST_CASE(test_sds_part_not_in_use_wrong_device_id),
	TEST_CASE(test_sds_part_multiple_mappings),
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
