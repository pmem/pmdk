// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmemset_part.c -- pmemset_part unittests
 */

#include <string.h>

#include "fault_injection.h"
#include "libpmemset.h"
#include "libpmem2.h"
#include "out.h"
#include "part.h"
#include "config.h"
#include "source.h"
#include "unittest.h"
#include "ut_pmemset_utils.h"

/*
 * test_part_new_enomem - test pmemset_part allocation with error injection
 */
static int
test_part_new_enomem(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_new_enomem <path>");

	const char *file = argv[0];
	struct pmemset_part *part;
	struct pmemset_source *src;

	if (!core_fault_injection_enabled())
		return 1;

	int ret = pmemset_source_from_file(&src, file);
	UT_ASSERTeq(ret, 0);

	core_inject_fault_at(PMEM_MALLOC, 1, "pmemset_malloc");

	ret = pmemset_part_new(&part, NULL, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, -ENOMEM);
	UT_ASSERTeq(part, NULL);

	return 1;
}

/*
 * test_part_new_invalid_source_path - create a new part from a source
 *                                     with invalid path assigned
 */
static int
test_part_new_invalid_source_path(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_new_invalid_source_path <path>");

	const char *file = argv[0];
	struct pmemset_part *part;
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_part_new(&part, NULL, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_INVALID_FILE_PATH);
	UT_ASSERTeq(part, NULL);

	return 1;
}

/*
 * test_part_new_valid_source_path - create a new part from a source
 *                                   with valid path assigned
 */
static int
test_part_new_valid_source_path(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_new_valid_source_path <path>");

	const char *file = argv[0];
	struct pmemset_part *part;
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_part_new(&part, NULL, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(part, NULL);

	return 1;
}

/*
 * test_part_new_valid_source_pmem2 - create a new part from a source
 *                                    with valid pmem2_source assigned
 */
static int
test_part_new_valid_source_pmem2(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_new_valid_source_pmem2 <path>");

	const char *file = argv[0];
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmem2_source *pmem2_src;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_part_new(&part, NULL, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(part, NULL);

	pmem2_source_delete(&pmem2_src);
	CLOSE(fd);

	return 1;
}

/*
 * test_part_map_valid_source_pmem2 - create a new part from a source
 *                                    with valid pmem2_source and map part
 */
static int
test_part_map_valid_source_pmem2(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_valid_source_pmem2 <path>");

	const char *file = argv[0];
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_config_new(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, 64 * 1024);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_config_delete(&cfg);
	pmem2_source_delete(&pmem2_src);
	CLOSE(fd);

	return 1;
}

/*
 * test_part_map_valid_source_file - create a new part from a source
 *                                    with valid file path and map part
 */
static int
test_part_map_valid_source_file(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_valid_source_file <path>");

	const char *file = argv[0];
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmemset *set;
	struct pmemset_config *cfg;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_config_new(&cfg);
	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, 64 * 1024);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_config_delete(&cfg);

	return 1;
}

/*
 * test_part_new_enomem_config - test pmemset_part allocation with
 *				error injection
 */
static int
test_part_map_enomem_config(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_enomem_config <path>");

	const char *file = argv[0];
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmemset_config *cfg;
	struct pmemset *set;

	if (!core_fault_injection_enabled())
		return 1;

	int ret = pmemset_source_from_file(&src, file);
	UT_ASSERTeq(ret, 0);

	pmemset_config_new(&cfg);
	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	core_inject_fault_at(PMEM_MALLOC, 1, "pmemset_config_new");

	ret = pmemset_part_new(&part, NULL, src, 0, 0);
	UT_PMEMSET_EXPECT_RETURN(ret,
		PMEMSET_E_CANNOT_ALLOCATE_INTERNAL_STRUCTURE);
	UT_ASSERTeq(part, NULL);

	return 1;
}

/*
 * test_part_map_invalid_offset - create a new part from a source
 *                                    with invalid offset value
 */
static int
test_part_map_invalid_offset(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_invalid_offset <path>");

	const char *file = argv[0];
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmemset *set;
	struct pmemset_config *cfg;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_config_new(&cfg);
	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src,
			(size_t)(INT64_MAX) + 1, 64 * 1024);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_INVALID_OFFSET_VALUE);

	pmemset_config_delete(&cfg);

	return 1;
}

/*
 * test_part_map_twice - try to map part twice
 */
static int
test_part_map_twice(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_part_map_twice <path>");

	const char *file = argv[0];
	struct pmemset_part *part;
	struct pmemset_source *src;
	struct pmem2_source *pmem2_src;
	struct pmemset *set;
	struct pmemset_config *cfg;

	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_config_new(&cfg);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_new(&part, set, src, 0, 64 * 1024);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_config_delete(&cfg);
	pmem2_source_delete(&pmem2_src);
	CLOSE(fd);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_part_new_enomem),
	TEST_CASE(test_part_new_invalid_source_path),
	TEST_CASE(test_part_new_valid_source_path),
	TEST_CASE(test_part_new_valid_source_pmem2),
	TEST_CASE(test_part_map_valid_source_pmem2),
	TEST_CASE(test_part_map_valid_source_file),
	TEST_CASE(test_part_map_enomem_config),
	TEST_CASE(test_part_map_invalid_offset),
	TEST_CASE(test_part_map_twice),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmemset_part");

	util_init();
	out_init("pmemset_part", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}
