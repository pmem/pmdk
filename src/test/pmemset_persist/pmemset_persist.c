// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * pmemset_persist.c -- pmemset_part unittests
 */

#include "out.h"
#include "unittest.h"
#include "ut_pmemset_utils.h"

/*
 * test_persist_single_part - test pmemset_persist on single part
 */
static int
test_persist_single_part(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_persist_single_part <path>");

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

	ut_create_map_config(&map_cfg, set, 0, 64 * 1024);
	UT_ASSERTne(map_cfg, NULL);

	struct pmemset_part_descriptor desc;
	ret = pmemset_map(src, map_cfg, &desc);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	memset(desc.addr, 1, desc.size);
	pmemset_persist(set, desc.addr, desc.size);

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
 * test_persist_multiple_parts - test pmemset_persist on multiple
 * parts
 */
static int
test_persist_multiple_parts(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_persist_multiple_parts <path1>");

	const char *file1 = argv[0];
	struct pmemset_source *src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	size_t first_part_size = 64 * 1024;
	size_t second_part_size = 128 * 1024;
	struct pmemset_part_descriptor first_desc;
	struct pmemset_part_descriptor second_desc;
	struct pmemset_map_config *first_map_cfg;
	struct pmemset_map_config *second_map_cfg;

	int ret = pmemset_source_from_file(&src, file1);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_create_set_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_create_map_config(&first_map_cfg, set, 0, first_part_size);
	UT_ASSERTne(first_map_cfg, NULL);

	ret = pmemset_map(src, first_map_cfg, &first_desc);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_create_map_config(&second_map_cfg, set, 0, second_part_size);
	UT_ASSERTne(second_map_cfg, NULL);

	ret = pmemset_map(src, second_map_cfg, &second_desc);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	memset(first_desc.addr, 1, first_desc.size);
	memset(second_desc.addr, 2, second_desc.size);
	pmemset_persist(set, first_desc.addr, first_desc.size);
	pmemset_persist(set, second_desc.addr, second_desc.size);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_map_config_delete(&first_map_cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_map_config_delete(&second_map_cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_persist_incomplete - test pmemset_persist on part of the mapping,
 * this test should fail under pmemcheck
 */
static int
test_persist_incomplete(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_persist_incomplete <path>");

	const char *file = argv[0];
	struct pmemset_source *src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	size_t part_size = 64 * 1024;
	struct pmemset_part_descriptor desc;
	struct pmemset_map_config *map_cfg;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_create_set_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_create_map_config(&map_cfg, set, 0, part_size);
	UT_ASSERTne(map_cfg, NULL);

	ret = pmemset_map(src, map_cfg, &desc);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	memset(desc.addr, 1, desc.size);
	pmemset_persist(set, desc.addr, desc.size / 2);

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
 * test_persist_flush_drain - test pmemset_flush on 2 maps
 * and drain, do not flush 1/2 of second map - should fail
 */
static int
test_set_flush_drain(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_set_flush_drain <path>");

	const char *file = argv[0];
	struct pmemset_source *src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	size_t first_part_size = 64 * 1024;
	size_t second_part_size = 256 * 1024;
	struct pmemset_part_descriptor first_desc;
	struct pmemset_part_descriptor second_desc;
	struct pmemset_map_config *first_map_cfg;
	struct pmemset_map_config *second_map_cfg;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_create_set_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_create_map_config(&first_map_cfg, set, 0, first_part_size);
	UT_ASSERTne(first_map_cfg, NULL);

	ret = pmemset_map(src, first_map_cfg, &first_desc);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_create_map_config(&second_map_cfg, set, 0, second_part_size);
	UT_ASSERTne(second_map_cfg, NULL);

	ret = pmemset_map(src, second_map_cfg, &second_desc);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	memset(first_desc.addr, 1, first_desc.size);
	memset(second_desc.addr, 2, second_desc.size);
	pmemset_flush(set, first_desc.addr, first_desc.size);
	pmemset_flush(set, second_desc.addr, second_desc.size / 2);
	pmemset_drain(set);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_map_config_delete(&first_map_cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_map_config_delete(&second_map_cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_persist_single_part),
	TEST_CASE(test_persist_multiple_parts),
	TEST_CASE(test_persist_incomplete),
	TEST_CASE(test_set_flush_drain),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmemset_persist");

	util_init();
	out_init("pmemset_persist", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}

#ifdef _MSC_VER
MSVC_CONSTR(libpmemset_init)
MSVC_DESTR(libpmemset_fini)
#endif
