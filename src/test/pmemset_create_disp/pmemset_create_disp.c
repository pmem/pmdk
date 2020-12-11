// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * pmemset_create_disp.c -- pmemset_file_create_disposition unittests
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
 * test_file_create_dispostion_valid - test valid
 * pmemset_config_file_create_disposition configuration values.
 */
static int
test_config_file_create_dispostion_valid(const struct test_case *tc, int argc,
						char *argv[])
{
	struct pmemset_config *cfg;
	enum pmemset_config_file_create_disposition set_disposition;
	enum pmemset_config_file_create_disposition get_disposition;

	int ret = pmemset_config_new(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);

	set_disposition = PMEMSET_CONFIG_FILE_CREATE_ALWAYS;
	ret = pmemset_config_set_file_create_disposition(cfg, set_disposition);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	get_disposition = pmemset_config_get_file_create_disposition(cfg);
	UT_ASSERTeq(set_disposition, get_disposition);

	set_disposition = PMEMSET_CONFIG_FILE_CREATE_IF_NEEDED;
	ret = pmemset_config_set_file_create_disposition(cfg, set_disposition);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	get_disposition = pmemset_config_get_file_create_disposition(cfg);
	UT_ASSERTeq(set_disposition, get_disposition);

	set_disposition = PMEMSET_CONFIG_FILE_OPEN;
	ret = pmemset_config_set_file_create_disposition(cfg, set_disposition);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	get_disposition = pmemset_config_get_file_create_disposition(cfg);
	UT_ASSERTeq(set_disposition, get_disposition);

	pmemset_config_delete(&cfg);
	UT_ASSERTeq(cfg, NULL);

	return 1;
}

/*
 * test_file_create_dispostion_invalid - test invalid
 * pmemset_config_file_create_disposition configuration value.
 */
static int
test_config_file_create_dispostion_invalid(const struct test_case *tc, int argc,
						char *argv[])
{
	struct pmemset_config *cfg;
	enum pmemset_config_file_create_disposition set_disposition;

	int ret = pmemset_config_new(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);

	set_disposition = (PMEMSET_CONFIG_FILE_OPEN + 1);
	ret = pmemset_config_set_file_create_disposition(cfg, set_disposition);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_INVALID_CFG_FILE_CREATE_DISP);

	pmemset_config_delete(&cfg);
	UT_ASSERTeq(cfg, NULL);

	return 1;
}

/*
 * test_file_create_disp_file_exists - test
 * pmemset_config_file_create_disposition configuration values when file exists.
 */
static int
test_file_create_disp_file_exists(const struct test_case *tc, int argc,
					char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_file_create_disp_file_exists <file>");

	char *file_path = argv[0];
	struct pmemset_config *cfg;
	struct pmemset_file *file;
	enum pmemset_config_file_create_disposition set_disposition;

	int ret = pmemset_config_new(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);

	set_disposition = PMEMSET_CONFIG_FILE_CREATE_ALWAYS;
	ret = pmemset_config_set_file_create_disposition(cfg, set_disposition);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_file_from_file(&file, file_path, cfg);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(file, NULL);
	pmemset_file_delete(&file);
	UT_ASSERTeq(file, NULL);

	set_disposition = PMEMSET_CONFIG_FILE_CREATE_IF_NEEDED;
	ret = pmemset_config_set_file_create_disposition(cfg, set_disposition);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_file_from_file(&file, file_path, cfg);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(file, NULL);
	pmemset_file_delete(&file);
	UT_ASSERTeq(file, NULL);

	set_disposition = PMEMSET_CONFIG_FILE_OPEN;
	ret = pmemset_config_set_file_create_disposition(cfg, set_disposition);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_file_from_file(&file, file_path, cfg);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(file, NULL);
	pmemset_file_delete(&file);
	UT_ASSERTeq(file, NULL);

	pmemset_config_delete(&cfg);
	UT_ASSERTeq(cfg, NULL);

	return 1;
}

/*
 * test_file_create_disp_no_file_always - test
 * pmemset_config_file_create_disposition configuration values when file does
 * not exist.
 */
static int
test_file_create_disp_no_file_always(const struct test_case *tc, int argc,
					char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_file_create_disp_file_not_exists <file>");

	char *file_path = argv[0];
	struct pmemset_config *cfg;
	struct pmemset_file *file;
	enum pmemset_config_file_create_disposition set_disposition;

	int ret = pmemset_config_new(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);

	set_disposition = PMEMSET_CONFIG_FILE_CREATE_ALWAYS;
	ret = pmemset_config_set_file_create_disposition(cfg, set_disposition);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_file_from_file(&file, file_path, cfg);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(file, NULL);
	pmemset_file_delete(&file);
	UT_ASSERTeq(file, NULL);

	pmemset_config_delete(&cfg);
	UT_ASSERTeq(cfg, NULL);

	return 1;
}

/*
 * test_file_create_disp_no_file_needed - test
 * pmemset_config_file_create_disposition configuration values when file does
 * not exist.
 */
static int
test_file_create_disp_no_file_needed(const struct test_case *tc, int argc,
					char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_file_create_disp_file_not_exists <file>");

	char *file_path = argv[0];
	struct pmemset_config *cfg;
	struct pmemset_file *file;
	enum pmemset_config_file_create_disposition set_disposition;

	int ret = pmemset_config_new(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);

	set_disposition = PMEMSET_CONFIG_FILE_CREATE_IF_NEEDED;
	ret = pmemset_config_set_file_create_disposition(cfg, set_disposition);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_file_from_file(&file, file_path, cfg);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(file, NULL);
	pmemset_file_delete(&file);
	UT_ASSERTeq(file, NULL);

	pmemset_config_delete(&cfg);
	UT_ASSERTeq(cfg, NULL);

	return 1;
}

/*
 * test_file_create_disp_no_file_open - test
 * pmemset_config_file_create_disposition configuration values when file does
 * not exist.
 */
static int
test_file_create_disp_no_file_open(const struct test_case *tc, int argc,
					char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_file_create_disp_file_not_exists <file>");

	char *file_path = argv[0];
	struct pmemset_config *cfg;
	struct pmemset_file *file;
	enum pmemset_config_file_create_disposition set_disposition;

	int ret = pmemset_config_new(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);

	set_disposition = PMEMSET_CONFIG_FILE_OPEN;
	ret = pmemset_config_set_file_create_disposition(cfg, set_disposition);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_file_from_file(&file, file_path, cfg);
	UT_ASSERTeq(ret, -2);
	UT_ASSERTeq(file, NULL);

	pmemset_config_delete(&cfg);
	UT_ASSERTeq(cfg, NULL);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_config_file_create_dispostion_valid),
	TEST_CASE(test_config_file_create_dispostion_invalid),
	TEST_CASE(test_file_create_disp_file_exists),
	TEST_CASE(test_file_create_disp_no_file_always),
	TEST_CASE(test_file_create_disp_no_file_needed),
	TEST_CASE(test_file_create_disp_no_file_open),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmemset_create_disp");

	util_init();
	out_init("pmemset_create_disp", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0,
			0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}
