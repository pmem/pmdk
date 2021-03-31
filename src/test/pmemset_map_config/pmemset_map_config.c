// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * pmemset_map_config.c -- pmemset_map_config unittests
 */

#include <string.h>

#include "config.h"
#include "fault_injection.h"
#include "libpmemset.h"
#include "libpmem2.h"
#include "out.h"
#include "part.h"
#include "source.h"
#include "unittest.h"
#include "ut_pmemset_utils.h"

/*
 * test_map_config_new_enomem - test map_config allocation with error injection
 */
static int
test_map_config_new_enomem(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_map_config_new_enomem <path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_source *src;
	struct pmemset_config *cfg;
	struct pmemset_map_config *map_cfg;

	if (!core_fault_injection_enabled())
		return 1;

	ut_create_set_config(&cfg);

	int ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	core_inject_fault_at(PMEM_MALLOC, 1, "pmemset_malloc");

	ret = pmemset_map_config_new(&map_cfg, set);
	UT_PMEMSET_EXPECT_RETURN(ret, -ENOMEM);
	UT_ASSERTeq(map_cfg, NULL);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_map_config_new_valid_source_file - create a new map_config with a source
 *                                   with valid path assigned
 */
static int
test_map_config_new_valid_source_file(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_map_config_new_valid_source_file <path>");

	const char *file = argv[0];
	struct pmemset *set;
	struct pmemset_source *src;
	struct pmemset_config *cfg;
	struct pmemset_map_config *map_cfg;

	ut_create_set_config(&cfg);

	int ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_file(&src, file);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_map_config_new(&map_cfg, set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(map_cfg, NULL);

	ret = pmemset_map(src, map_cfg, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_map_config_delete(&map_cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_map_config_new_valid_source_pmem2 - create a new map_config with
 *			a source with valid pmem2_source assigned
 */
static int
test_map_config_new_valid_source_pmem2(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_map_config_new_valid_source_pmem2 <path>");

	const char *file = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_source *src;
	struct pmemset_config *cfg;
	struct pmemset_map_config *map_cfg;

	ut_create_set_config(&cfg);

	int ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	int fd = OPEN(file, O_RDWR);

	ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = pmemset_map_config_new(&map_cfg, set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(map_cfg, NULL);

	ret = pmemset_map(src, map_cfg, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	CLOSE(fd);
	ret = pmemset_map_config_delete(&map_cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_map_coinfig_new_invalid_source_ - create a new map_config with
 *					an invalid source
 */
static int
test_map_config_new_invalid_source(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_map_config_new_invalid_source");

	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_map_config *map_cfg;

	ut_create_set_config(&cfg);

	int ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_map_config_new(&map_cfg, set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(map_cfg, NULL);

	ret = pmemset_map(NULL, map_cfg, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_INVALID_SOURCE_TYPE);

	ret = pmemset_map_config_delete(&map_cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_delete_null_config - test pmemset_map_config_delete on NULL config
 */
static int
test_delete_null_config(const struct test_case *tc, int argc,
		char *argv[])
{
	struct pmemset_map_config *cfg = NULL;
	/* should not crash */
	int ret = pmemset_map_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(cfg, NULL);

	return 1;
}

/*
 * test_map_config_invalid_offset - create a new map config
 *                                    with invalid offset value
 */
static int
test_map_config_invalid_offset(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_map_config_invalid_offset <path>");

	const char *file = argv[0];
	struct pmemset_source *src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_map_config *map_cfg;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_create_set_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_map_config_new(&map_cfg, set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(map_cfg, NULL);

	ret = pmemset_map_config_set_offset(map_cfg, (size_t)(INT64_MAX) + 1);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_OFFSET_OUT_OF_RANGE);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_map_config_delete(&map_cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_map_config_new_enomem),
	TEST_CASE(test_map_config_new_valid_source_file),
	TEST_CASE(test_map_config_new_valid_source_pmem2),
	TEST_CASE(test_delete_null_config),
	TEST_CASE(test_map_config_new_invalid_source),
	TEST_CASE(test_map_config_invalid_offset),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmemset_map_config");

	util_init();
	out_init("pmemset_map_config", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}
