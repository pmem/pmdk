// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2022, Intel Corporation */

/*
 * pmemset_persist.c -- pmemset_part unittests
 */

#include "out.h"
#include "unittest.h"
#include "ut_pmemset_utils.h"

static int pmem2_df_count;

FUNC_MOCK(pmem2_deep_flush, int, struct pmem2_map *map, void *ptr, size_t size)
	FUNC_MOCK_RUN_DEFAULT {
		pmem2_df_count++;
		return _FUNC_REAL(pmem2_deep_flush)(map, ptr, size);
	}
FUNC_MOCK_END

/*
 * test_deep_flush_single - test pmemset_deep_flush combinations with single map
 */
static int
test_deep_flush_single(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_deep_flush_single <path>");

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

	ut_create_map_config(&map_cfg, 0, 64 * 1024);
	UT_ASSERTne(map_cfg, NULL);

	struct pmemset_part_descriptor desc;
	ret = pmemset_map(set, src, map_cfg, &desc);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* flush single part map */
	pmemset_deep_flush(set, desc.addr, desc.size);

	UT_ASSERTeq(pmem2_df_count, 1);
	pmem2_df_count = 0;

	/* flush half of a single part map */
	pmemset_deep_flush(set, desc.addr, desc.size / 2);

	UT_ASSERTeq(pmem2_df_count, 1);
	pmem2_df_count = 0;

	/* flush half of a single part map and out of map */
	pmemset_deep_flush(set, (char *)desc.addr + desc.size / 2, desc.size);

	UT_ASSERTeq(pmem2_df_count, 1);
	pmem2_df_count = 0;

	/* flush out of map */
	pmemset_deep_flush(set, (char *)desc.addr - desc.size, desc.size / 2);

	UT_ASSERTeq(pmem2_df_count, 0);

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
 * test_deep_flush_multiple_coal - test pmemset_deep_flush
 * combinations on multiple part maps with coalescing
 */
static int
test_deep_flush_multiple_coal(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_deep_flush_multiple_coal <path>");

	const char *file1 = argv[0];
	struct pmemset_source *src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_map_config *map_cfg;
	size_t part_size = 64 * 1024;
	const size_t num_of_parts = 8;

	struct pmemset_part_descriptor desc;

	int ret = pmemset_source_from_file(&src, file1);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_create_set_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_set_contiguous_part_coalescing(set,
			PMEMSET_COALESCING_FULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_create_map_config(&map_cfg, 0, part_size);
	UT_ASSERTne(map_cfg, NULL);

	for (int i = 0; i < num_of_parts; i++) {
		ret = pmemset_map(set, src, map_cfg, &desc);
		if (ret == PMEMSET_E_CANNOT_COALESCE_PARTS)
			goto end;
		UT_PMEMSET_EXPECT_RETURN(ret, 0);
	}

	/* flush all maps  */
	pmemset_deep_flush(set, desc.addr, desc.size);
	UT_ASSERTeq(pmem2_df_count, num_of_parts);
	pmem2_df_count = 0;

	/* flush a half of all parts */
	pmemset_deep_flush(set, (char *)desc.addr + (part_size *
		(num_of_parts / 2)), desc.size);
	UT_ASSERTeq(pmem2_df_count, num_of_parts / 2);
	pmem2_df_count = 0;

	/* flush tree maps, but start and finish at the middle of them */
	pmemset_deep_flush(set, (char *)desc.addr + (part_size / 2),
		part_size * 2);
	UT_ASSERTeq(pmem2_df_count, 3);
	pmem2_df_count = 0;

	/*
	 * flush one (not first) map, but start and finish are in the middle,
	 * it means flush range is smaller than map
	 */
	pmemset_deep_flush(set, (char *)desc.addr +
			((part_size * 2) + (part_size / 4)), part_size / 4);
	UT_ASSERTeq(pmem2_df_count, 1);
	pmem2_df_count = 0;

	/*
	 * flush tree maps, but start and finish at the middle of them,
	 * and start is not in the first map
	 */
	pmemset_deep_flush(set, (char *)desc.addr + part_size + (part_size / 2),
		part_size * 2);
	UT_ASSERTeq(pmem2_df_count, 3);
	pmem2_df_count = 0;

	/* flush one map, use whole map size as a range */
	pmemset_deep_flush(set, (char *)desc.addr + (part_size * 5), part_size);
	UT_ASSERTeq(pmem2_df_count, 1);
	pmem2_df_count = 0;

	/*
	 * flush two maps, but start at the beginning of fifth and
	 * finish at the middle of the next one
	 */
	pmemset_deep_flush(set, (char *)desc.addr + (part_size * 5),
		part_size + (part_size / 2));
	UT_ASSERTeq(pmem2_df_count, 2);
	pmem2_df_count = 0;

	/* flush all, but range out of maps */
	pmemset_deep_flush(set, (char *)desc.addr, desc.size + part_size);
	UT_ASSERTeq(pmem2_df_count, num_of_parts);

end:
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
 * test_deep_flush_multiple - test pmemset_deep_flush combinations on multiple
 * part maps
 */
static int
test_deep_flush_multiple(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_deep_flush_multiple <path>");

	const char *file1 = argv[0];
	struct pmemset_source *src;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_map_config *map_cfg;
	struct pmemset_part_map *first_pmap = NULL;
	struct pmemset_part_map *second_pmap = NULL;
	struct pmemset_part_descriptor first_desc;
	struct pmemset_part_descriptor second_desc;
	size_t part_size = 64 * 1024;

	int ret = pmemset_source_from_file(&src, file1);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_create_set_config(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_create_map_config(&map_cfg, 0, part_size);
	UT_ASSERTne(map_cfg, NULL);

	ret = pmemset_map(set, src, map_cfg, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_map(set, src, map_cfg, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_first_part_map(set, &first_pmap);
	UT_ASSERTne(first_pmap, NULL);

	pmemset_next_part_map(set, first_pmap, &second_pmap);
	UT_ASSERTne(second_pmap, NULL);

	first_desc = pmemset_descriptor_part_map(first_pmap);
	second_desc = pmemset_descriptor_part_map(second_pmap);

	size_t len = (size_t)((char *)second_desc.addr -
			(char *)first_desc.addr);
	size_t range_size = second_desc.size + len;

	/* flush both maps at once */
	pmemset_deep_flush(set, first_desc.addr, range_size);
	UT_ASSERTeq(pmem2_df_count, 2);

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
	TEST_CASE(test_deep_flush_single),
	TEST_CASE(test_deep_flush_multiple),
	TEST_CASE(test_deep_flush_multiple_coal),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmemset_deep_flush");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}

#ifdef _MSC_VER
MSVC_CONSTR(libpmemset_init)
MSVC_DESTR(libpmemset_fini)
#endif
