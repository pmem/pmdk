// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmemset_file.c -- pmemset_file unittests
 */

#include <libpmem2.h>

#include "config.h"
#include "fault_injection.h"
#include "file.h"
#include "libpmemset.h"
#include "out.h"
#include "source.h"
#include "unittest.h"
#include "ut_pmemset_utils.h"

/*
 * test_alloc_file_enomem -- test pmemset_file allocation with error injection
 */
static int
test_alloc_file_enomem(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_alloc_file_enomem <file>");

	char *file_path = argv[0];
	struct pmemset_config *cfg;
	struct pmemset_file *file;

	if (!core_fault_injection_enabled()) {
		return 1;
	}

	int ret = pmemset_config_new(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);

	core_inject_fault_at(PMEM_MALLOC, 1, "pmemset_malloc");

	ret = pmemset_file_from_file(&file, file_path, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, -ENOMEM);
	UT_ASSERTeq(file, NULL);

	return 1;
}

/*
 * test_file_from_file_valid - test valid pmemset_file allocation from file
 */
static int
test_file_from_file_valid(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_file_from_file_valid <file>");

	char *file_path = argv[0];
	struct pmemset_config *cfg;
	struct pmemset_file *file;

	pmemset_config_new(&cfg);

	int ret = pmemset_file_from_file(&file, file_path, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(file, NULL);

	pmemset_file_delete(&file);
	UT_ASSERTeq(file, NULL);

	return 1;
}

/*
 * test_file_from_file_invalid - test pmemset_file allocation from invalid path
 */
static int
test_file_from_file_invalid(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_file_from_file_invalid <file>");

	char *file_path = argv[0];
	struct pmemset_config *cfg;
	struct pmemset_file *file;

	pmemset_config_new(&cfg);

	int ret = pmemset_file_from_file(&file, file_path, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, -ENOENT);
	UT_ASSERTeq(file, NULL);

	return 1;
}

/*
 * test_file_from_pmem2_valid - test valid pmemset_file allocation
 *                              from pmem2_source
 */
static int
test_file_from_pmem2_valid(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_file_from_pmem2_valid <file>");

	char *file_path = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmemset_config *cfg;
	struct pmemset_file *file;

	pmemset_config_new(&cfg);

	int fd = OPEN(file_path, O_RDWR);
	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_file_from_pmem2(&file, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(file, NULL);

	pmemset_file_delete(&file);
	UT_ASSERTeq(file, NULL);

	CLOSE(fd);

	return 1;
}

/*
 * test_file_from_pmem2_invalid - test pmemset_file allocation from invalid
 *                                pmem2_source
 */
static int
test_file_from_pmem2_invalid(const struct test_case *tc, int argc, char *argv[])
{
	struct pmemset_file *file;

	int ret = pmemset_file_from_pmem2(&file, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_INVALID_PMEM2_SOURCE);
	UT_ASSERTeq(file, NULL);

	return 0;
}

/*
 * test_file_from_file_get_pmem2_src - test retrieving pmem2_src stored in the
 *                                     pmemset_file created from file
 */
static int
test_file_from_file_get_pmem2_src(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_file_from_file_get_pmem2_src <file>");

	char *file_path = argv[0];
	struct pmem2_source *retrieved_pmem2_src = NULL;
	struct pmemset_config *cfg;
	struct pmemset_file *file;

	pmemset_config_new(&cfg);

	int ret = pmemset_file_from_file(&file, file_path, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(file, NULL);

	retrieved_pmem2_src = pmemset_file_get_pmem2_source(file);
	UT_ASSERTne(retrieved_pmem2_src, NULL);

	pmemset_file_delete(&file);
	UT_ASSERTeq(file, NULL);

	return 1;
}

/*
 * test_file_from_pmem2_get_pmem2_src - test retrieving pmem2_source stored
 * in the pmemset_file created from pmem2_source
 */
static int
test_file_from_pmem2_get_pmem2_src(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_file_from_pmem2_get_pmem2_src <file>");

	char *file_path = argv[0];
	struct pmem2_source *pmem2_src;
	struct pmem2_source *retrieved_pmem2_src = NULL;
	struct pmemset_config *cfg;
	struct pmemset_file *file;

	pmemset_config_new(&cfg);

	int fd = OPEN(file_path, O_RDWR);
	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_file_from_pmem2(&file, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(file, NULL);

	retrieved_pmem2_src = pmemset_file_get_pmem2_source(file);
	UT_ASSERTeq(retrieved_pmem2_src, pmem2_src);

	pmemset_file_delete(&file);
	UT_ASSERTeq(file, NULL);

	CLOSE(fd);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_alloc_file_enomem),
	TEST_CASE(test_file_from_file_valid),
	TEST_CASE(test_file_from_file_invalid),
	TEST_CASE(test_file_from_pmem2_valid),
	TEST_CASE(test_file_from_pmem2_invalid),
	TEST_CASE(test_file_from_file_get_pmem2_src),
	TEST_CASE(test_file_from_pmem2_get_pmem2_src),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmemset_file");

	util_init();
	out_init("pmemset_file", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}
