// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * pmemset_source.c -- pmemset_source unittests
 */
#include "fault_injection.h"
#include "file.h"
#include "libpmemset.h"
#include "out.h"
#include "source.h"
#include "unittest.h"
#include "ut_pmemset_utils.h"
#include "config.h"

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

	ret = pmemset_source_delete(&src_set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src_set, NULL);

	ret = pmem2_source_delete(&src_pmem2);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

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

	ret = pmem2_source_delete(&src_pmem2);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	CLOSE(fd);
	return 1;
}

/*
 * test_src_from_file_null - test source creation from null file path
 */
static int
test_src_from_file_null(const struct test_case *tc, int argc,
		char *argv[])
{
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_INVALID_FILE_PATH);
	UT_ASSERTeq(src, NULL);

	return 0;
}

/*
 * test_src_from_file_valid - test source creation with valid file path
 */
static int
test_src_from_file_valid(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_src_from_file_valid <path>");

	const char *file = argv[0];
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	return 1;
}

/*
 * test_create_file_from_type_file - test pmemset_file creation with source
 *                                   type file
 */
static int
test_create_file_from_type_file(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_create_file_from_type_file <path>");

	const char *file_path = argv[0];
	struct pmemset_config *cfg;
	struct pmemset_file *file = NULL;
	struct pmemset_source *src;

	int ret = pmemset_config_new(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);

	ret = pmemset_source_from_file(&src, file_path);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = pmemset_source_create_pmemset_file(src, &file, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(file, NULL);

	pmemset_file_delete(&file);
	UT_ASSERTeq(file, NULL);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	return 1;
}

/*
 * test_create_file_from_type_pmem2 - test pmemset_file creation with source
 *                                    type pmem2
 */
static int
test_create_file_from_type_pmem2(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_create_file_from_type_pmem2 <path>");

	const char *file_path = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset_config *cfg;
	struct pmemset_file *file = NULL;
	struct pmemset_source *src;

	int ret = pmemset_config_new(&cfg);

	int fd = OPEN(file_path, O_RDWR);
	ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_ASSERTne(pmem2_src, NULL);

	ret = pmemset_source_from_pmem2(&src, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = pmemset_source_create_pmemset_file(src, &file, cfg);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(file, NULL);

	pmemset_file_delete(&file);
	UT_ASSERTeq(file, NULL);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

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
	TEST_CASE(test_src_from_file_null),
	TEST_CASE(test_src_from_file_valid),
	TEST_CASE(test_create_file_from_type_file),
	TEST_CASE(test_create_file_from_type_pmem2),
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
