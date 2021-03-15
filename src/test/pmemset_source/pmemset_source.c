// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

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
 * test_src_from_file_exists_always_disp - test source creation with
 * PMEMSET_SOURCE_FILE_CREATE_ALWAYS file disposition.
 */
static int
test_src_from_file_exists_always_disp(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_src_from_file_exists_always_disp <path>");

	const char *file = argv[0];
	struct pmemset_source *src;
	unsigned flags = 0;
	os_off_t size_before, size_after;
	os_stat_t st;

	int ret = os_stat(file, &st);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	size_before = st.st_size;

	flags = PMEMSET_SOURCE_FILE_CREATE_ALWAYS;
	ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = os_access(file, F_OK);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = os_stat(file, &st);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	size_after = st.st_size;
	UT_ASSERT(size_before >= size_after);
	UT_ASSERT(size_after == 0);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	return 1;
}

/*
 * test_src_from_file_not_exists_always_disp - test source creation with
 * PMEMSET_SOURCE_FILE_CREATE_ALWAYS file disposition when file does not exist.
 */
static int
test_src_from_file_not_exists_always_disp(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_src_from_file_not_exists_always_disp "
			"<path>");

	const char *file = argv[0];
	struct pmemset_source *src;
	unsigned flags = 0;
	os_off_t size;
	os_stat_t st;

	flags = PMEMSET_SOURCE_FILE_CREATE_ALWAYS;
	int ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = os_access(file, F_OK);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = os_stat(file, &st);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	size = st.st_size;
	UT_ASSERT(size == 0);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	return 1;
}

/*
 * test_src_from_file_exists_needed_disp - test source creation with
 * PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED file disposition.
 */
static int
test_src_from_file_exists_needed_disp(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_src_from_file_exists_needed_disp <path>");

	const char *file = argv[0];
	struct pmemset_source *src;
	unsigned flags = 0;
	os_off_t size_before, size_after;
	os_stat_t st;

	int ret = os_stat(file, &st);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	size_before = st.st_size;

	flags = PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED;
	ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = os_access(file, F_OK);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = os_stat(file, &st);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	size_after = st.st_size;
	UT_ASSERT(size_before == size_after);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	return 1;
}

/*
 * test_src_from_file_not_exists_needed_disp - test source creation with
 * PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED file disposition when file does not
 * exist.
 */
static int
test_src_from_file_not_exists_needed_disp(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_src_from_file_not_exists_needed_disp "
			"<path>");

	const char *file = argv[0];
	struct pmemset_source *src;
	unsigned flags = 0;
	os_off_t size;
	os_stat_t st;

	flags = PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED;
	int ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = os_access(file, F_OK);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = os_stat(file, &st);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	size = st.st_size;
	UT_ASSERT(size == 0);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	return 1;
}

/*
 * test_src_from_file_invalid_flags - test source creation with
 * invalid flags.
 */
static int
test_src_from_file_invalid_flags(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_src_from_file_invalid_flags "
			"<path>");

	const char *file = argv[0];
	struct pmemset_source *src;
	unsigned flags = 0;

	flags = PMEMSET_SOURCE_FILE_CREATE_VALID_FLAGS + 1;
	int ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret,
		PMEMSET_E_INVALID_SOURCE_FILE_CREATE_FLAGS);
	UT_ASSERTeq(src, NULL);

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
	TEST_CASE(test_src_from_file_exists_always_disp),
	TEST_CASE(test_src_from_file_not_exists_always_disp),
	TEST_CASE(test_src_from_file_exists_needed_disp),
	TEST_CASE(test_src_from_file_not_exists_needed_disp),
	TEST_CASE(test_src_from_file_invalid_flags),
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
