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
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_INVALID_SOURCE_PATH);
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
	UT_ASSERTeq(ret, 0);
	size_before = st.st_size;

	flags = PMEMSET_SOURCE_FILE_CREATE_ALWAYS;
	ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = os_access(file, F_OK);
	UT_ASSERTeq(ret, 0);

	ret = os_stat(file, &st);
	UT_ASSERTeq(ret, 0);
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
	UT_ASSERTeq(ret, 0);

	ret = os_stat(file, &st);
	UT_ASSERTeq(ret, 0);
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
	UT_ASSERTeq(ret, 0);
	size_before = st.st_size;

	flags = PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED;
	ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = os_access(file, F_OK);
	UT_ASSERTeq(ret, 0);

	ret = os_stat(file, &st);
	UT_ASSERTeq(ret, 0);
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
	UT_ASSERTeq(ret, 0);

	ret = os_stat(file, &st);
	UT_ASSERTeq(ret, 0);
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
	uint64_t flags = 0;

	flags = PMEMSET_SOURCE_FILE_CREATE_VALID_FLAGS + 1;
	int ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret,
		PMEMSET_E_INVALID_SOURCE_FILE_CREATE_FLAGS);
	UT_ASSERTeq(src, NULL);

	return 1;
}

/*
 * test_src_from_temporary_valid - test source from temporary created
 * in the provided dir
 */
static int
test_src_from_temporary_valid(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_src_from_temporary_valid <dir>");

	char *dir = argv[0];

	struct pmemset_source *src_set;

	int ret = pmemset_source_from_temporary(&src_set, dir);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src_set, NULL);

	ret = pmemset_source_delete(&src_set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src_set, NULL);

	return 1;
}

/*
 * test_src_from_temporary_inval_dir - test source from temporary created
 * in the provided invalid dir path
 */
static int
test_src_from_temporary_inval_dir(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc != 0)
		UT_FATAL("usage: test_src_from_temporary_inval_dir");

	char *dir = NULL;

	struct pmemset_source *src_set;

	int ret = pmemset_source_from_temporary(&src_set, dir);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_INVALID_SOURCE_PATH);
	UT_ASSERTeq(src_set, NULL);

	dir = "XYZ";

	ret = pmemset_source_from_temporary(&src_set, dir);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_INVALID_SOURCE_PATH);
	UT_ASSERTeq(src_set, NULL);

	return 0;
}

/*
 * test_src_from_temporary_no_del - test source from temporary created
 * in the provided dir but do not dlete source - tmp file sohuld not be deleted
 */
static int
test_src_from_temporary_no_del(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_src_from_temporary_no_del <dir>");

	char *dir = argv[0];

	struct pmemset_source *src_set;

	int ret = pmemset_source_from_temporary(&src_set, dir);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src_set, NULL);

	return 1;
}

/*
 * test_src_from_file_with_do_not_grow - test source creation with
 * PMEMSET_SOURCE_FILE_DO_NOT_GROW flag.
 */
static int
test_src_from_file_with_do_not_grow(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_src_from_file_with_do_not_grow "
			"<path>");

	const char *file = argv[0];
	struct pmemset_source *src;
	unsigned flags = 0;
	os_off_t size;
	os_stat_t st;

	flags = PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED | \
		PMEMSET_SOURCE_FILE_DO_NOT_GROW;
	int ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = os_access(file, F_OK);
	UT_ASSERTeq(ret, 0);

	ret = os_stat(file, &st);
	UT_ASSERTeq(ret, 0);
	size = st.st_size;
	UT_ASSERT(size == 0);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	return 1;
}

/*
 * test_src_from_file_with_rusr_mode - test source creation with
 * PMEMSET_SOURCE_FILE_CREATE_ALWAYS_MODE flags.
 */
static int
test_src_from_file_with_rusr_mode(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_src_from_file_with_rusr_mode <path>");

	const char *file = argv[0];
	struct pmemset_source *src;
	uint64_t flags = 0;
	int ret = 0;

	flags = PMEMSET_SOURCE_FILE_CREATE_ALWAYS | \
	PMEMSET_SOURCE_FILE_CREATE_ALWAYS_MODE(PMEMSET_SOURCE_FILE_RUSR_MODE);
	ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = os_access(file, F_OK | R_OK | W_OK | X_OK);
	UT_ASSERTeq(ret, -1);

	ret = os_access(file, F_OK | R_OK);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	return 1;
}

/*
 * test_src_from_file_with_rwxu_mode - test source creation with
 * PMEMSET_SOURCE_FILE_CREATE_ALWAYS_MODE flags.
 */
static int
test_src_from_file_with_rwxu_mode(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_src_from_file_with_rwxu_mode <path>");

	const char *file = argv[0];
	struct pmemset_source *src;
	uint64_t flags = 0;
	int ret = 0;

	flags = PMEMSET_SOURCE_FILE_CREATE_ALWAYS | \
	PMEMSET_SOURCE_FILE_CREATE_ALWAYS_MODE(PMEMSET_SOURCE_FILE_RWXU_MODE);
	ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = os_access(file, F_OK | R_OK | W_OK | X_OK);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	return 1;
}

/*
 * test_src_from_file_with_num_mode - test source creation with
 * number mode value in PMEMSET_SOURCE_FILE_CREATE_ALWAYS_MODE.
 */
static int
test_src_from_file_with_num_mode(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_src_from_file_with_num_mode <path>");

	const char *file = argv[0];
	struct pmemset_source *src;
	uint64_t flags = 0;
	int ret = 0;

	flags = PMEMSET_SOURCE_FILE_CREATE_ALWAYS | \
	PMEMSET_SOURCE_FILE_CREATE_ALWAYS_MODE(00700);
	ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = os_access(file, F_OK | R_OK | W_OK | X_OK);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	return 1;
}

/*
 * test_src_from_file_with_inval_mode - test source creation with
 * invalid mode value in PMEMSET_SOURCE_FILE_CREATE_ALWAYS_MODE.
 */
static int
test_src_from_file_with_inval_mode(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_src_from_file_with_inval_mode <path>");

	const char *file = argv[0];
	struct pmemset_source *src;
	uint64_t flags = 0;
	int ret = 0;

	flags = PMEMSET_SOURCE_FILE_CREATE_ALWAYS | \
		PMEMSET_SOURCE_FILE_CREATE_MODE(90180);
	ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret,
		PMEMSET_E_INVALID_SOURCE_FILE_CREATE_FLAGS);
	UT_ASSERTeq(src, NULL);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	return 1;
}

/*
 * test_src_from_file_only_mode - test source creation with
 * only mode value in flag param.
 */
static int
test_src_from_file_only_mode(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_src_from_file_only_mode <path>");

	const char *file = argv[0];
	struct pmemset_source *src;
	uint64_t flags = 0;
	int ret = 0;
/* just to compile test on Windows */
#ifndef S_IXUSR
#define S_IXUSR 00100
#endif
	flags = PMEMSET_SOURCE_FILE_CREATE_ALWAYS_MODE(S_IXUSR);
	ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = os_access(file, F_OK | R_OK | X_OK);
	UT_ASSERTeq(ret, -1);

	ret = os_access(file, F_OK | X_OK);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	return 1;
}

/*
 * test_src_from_file_with_inval_win_mode - test source creation with
 * invalid mode value on Windows in PMEMSET_SOURCE_FILE_CREATE_ALWAYS_MODE.
 */
static int
test_src_from_file_with_inval_win_mode(const struct test_case *tc, int argc,
	char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_src_from_file_with_inval_win_mode <path>");

	const char *file = argv[0];
	struct pmemset_source *src;
	uint64_t flags = 0;
	int ret = 0;

	/* "random" flag does not work */
	flags = PMEMSET_SOURCE_FILE_CREATE_ALWAYS_MODE(00100);
	ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret,
		PMEMSET_E_INVALID_SOURCE_FILE_CREATE_FLAGS);
	UT_ASSERTeq(src, NULL);

	/* PMEMSET flag works but do nothing internally */
	flags = PMEMSET_SOURCE_FILE_CREATE_ALWAYS_MODE(
			PMEMSET_SOURCE_FILE_RWXU_MODE);
	ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	return 1;
}

/*
 * test_src_from_file_with_rusr_mode_if_needed - test source creation with
 * PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED flag.
 */
static int
test_src_from_file_with_rusr_mode_if_needed(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_src_from_file_with_rusr_mode_if_needed <path>");

	const char *file = argv[0];
	struct pmemset_source *src;
	uint64_t flags = 0;
	int ret = 0;

	flags = PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED | \
	PMEMSET_SOURCE_FILE_CREATE_MODE(PMEMSET_SOURCE_FILE_RUSR_MODE);
	ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	ret = os_access(file, F_OK | R_OK | W_OK | X_OK);
	UT_ASSERTeq(ret, -1);

	ret = os_access(file, F_OK | R_OK);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	return 1;
}

/*
 * test_src_from_file_with_rwxu_mode_if_needed_created - test source creation
 * with PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED flag with already creaded file.
 */
static int
test_src_from_file_with_rwxu_mode_if_needed_created(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_src_from_file_with_rwxu_mode_if_needed_created <path>");

	const char *file = argv[0];
	struct pmemset_source *src;
	uint64_t flags = 0;
	int ret = 0;

	flags = PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED | \
	PMEMSET_SOURCE_FILE_CREATE_MODE(PMEMSET_SOURCE_FILE_RWXU_MODE);
	ret = pmemset_xsource_from_file(&src, file, flags);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	/* file is already created - default mode has not changed */
	ret = os_access(file, F_OK | R_OK | W_OK | X_OK);
	UT_ASSERTeq(ret, -1);

	ret = os_access(file, F_OK | R_OK);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	return 1;
}

enum test_src_op_type {
	TEST_SRC_OP_READ,
	TEST_SRC_OP_WRITE
};

/*
 * test_src_mcsafe_op -- test machine safe operation
 */
static void
test_src_mcsafe_op(char *file, enum test_src_op_type op_type)
{
	UT_ASSERT(op_type >= TEST_SRC_OP_READ && op_type <= TEST_SRC_OP_WRITE);

	struct pmem2_source *p2src;
	struct pmemset_source *src;

	size_t bufsize = 4096;
	void *buf = MALLOC(bufsize);

	/* source from file */
	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	if (op_type == TEST_SRC_OP_READ)
		ret = pmemset_source_pread_mcsafe(src, buf, bufsize, 0);
	else
		ret = pmemset_source_pwrite_mcsafe(src, buf, bufsize, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_source_delete(&src);

	/* source from pmem2 */
	int fd = OPEN(file, O_RDWR);
	ret = pmem2_source_from_fd(&p2src, fd);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_source_from_pmem2(&src, p2src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	if (op_type == TEST_SRC_OP_READ)
		ret = pmemset_source_pread_mcsafe(src, buf, bufsize, 0);
	else
		ret = pmemset_source_pwrite_mcsafe(src, buf, bufsize, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_source_delete(&src);
	CLOSE(fd);

	/* source from temporary */
	for (int i = (int)strlen(file) - 1; i > 0; i--) {
		/* change file path into directory path */
		if (file[i] == '/' || file[i] == '\\') {
			file[i + 1] = '\0';
			break;
		}
	}

	ret = pmemset_source_from_temporary(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	if (op_type == TEST_SRC_OP_READ)
		ret = pmemset_source_pread_mcsafe(src, buf, bufsize, 0);
	else
		ret = pmemset_source_pwrite_mcsafe(src, buf, bufsize, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_LENGTH_OUT_OF_RANGE);

	FREE(buf);

	pmemset_source_delete(&src);
}

/*
 * test_src_mcsafe_read -- test mcsafe read operation
 */
static int
test_src_mcsafe_read(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_src_mcsafe_read <file>");

	char *file = argv[0];
	test_src_mcsafe_op(file, TEST_SRC_OP_READ);

	return 1;
}

/*
 * test_src_mcsafe_write -- test mcsafe write operation
 */
static int
test_src_mcsafe_write(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_src_mcsafe_write <file>");

	char *file = argv[0];
	test_src_mcsafe_op(file, TEST_SRC_OP_WRITE);

	return 1;
}

/*
 * test_src_alignment -- test read alignment operation
 */
static int
test_src_alignment(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_src_alignment <file>");

	const char *file = argv[0];
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);

	size_t alignment = 0;
	ret = pmemset_source_alignment(src, &alignment);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(alignment, 0);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
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
	TEST_CASE(test_src_from_temporary_valid),
	TEST_CASE(test_src_from_temporary_inval_dir),
	TEST_CASE(test_src_from_temporary_no_del),
	TEST_CASE(test_src_from_file_with_do_not_grow),
	TEST_CASE(test_src_from_file_with_rusr_mode),
	TEST_CASE(test_src_from_file_with_rwxu_mode),
	TEST_CASE(test_src_from_file_with_num_mode),
	TEST_CASE(test_src_from_file_with_inval_mode),
	TEST_CASE(test_src_from_file_with_inval_win_mode),
	TEST_CASE(test_src_from_file_only_mode),
	TEST_CASE(test_src_from_file_with_rusr_mode_if_needed),
	TEST_CASE(test_src_from_file_with_rwxu_mode_if_needed_created),
	TEST_CASE(test_src_mcsafe_read),
	TEST_CASE(test_src_mcsafe_write),
	TEST_CASE(test_src_alignment),
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
