// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * pmem2_source.c -- pmem2_source unittests
 */
#include "fault_injection.h"
#include "libpmem2.h"
#include "unittest.h"
#include "ut_pmem2_utils.h"
#include "ut_pmem2_config.h"
#include "source.h"
#include "out.h"

/*
 * verify_fd -- verify value fd or handle in source
 */
static void
verify_fd(struct pmem2_source *src, int fd)
{
#ifdef WIN32
	UT_ASSERTeq(src->type, PMEM2_SOURCE_HANDLE);
	UT_ASSERTeq(src->value.handle, fd != INVALID_FD ?
		(HANDLE)_get_osfhandle(fd) : INVALID_HANDLE_VALUE);
#else
	UT_ASSERTeq(src->type, PMEM2_SOURCE_FD);
	UT_ASSERTeq(src->value.fd, fd);
#endif
}

/*
 * test_set_rw_fd - test setting O_RDWR fd
 */
static int
test_set_rw_fd(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_set_rw_fd <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_source *src;

	int ret = pmem2_source_from_fd(&src, fd);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);
	verify_fd(src, fd);

	ret = pmem2_source_delete(&src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	CLOSE(fd);

	return 1;
}

/*
 * test_set_ro_fd - test setting O_RDONLY fd
 */
static int
test_set_ro_fd(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_set_ro_fd <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDONLY);

	struct pmem2_source *src;

	int ret = pmem2_source_from_fd(&src, fd);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(src, NULL);
	verify_fd(src, fd);

	ret = pmem2_source_delete(&src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	CLOSE(fd);

	return 1;
}

/*
 * test_set_invalid_fd - test setting invalid fd
 */
static int
test_set_invalid_fd(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_set_invalid_fd <file>");

	char *file = argv[0];
	/* open and close the file to get invalid fd */
	int fd = OPEN(file, O_WRONLY);
	CLOSE(fd);
	ut_suppress_crt_assert();
	struct pmem2_source *src;

	int ret = pmem2_source_from_fd(&src, fd);
	ut_unsuppress_crt_assert();
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_HANDLE);
	UT_ASSERTeq(src, NULL);

	return 1;
}

/*
 * test_set_wronly_fd - test setting wronly fd
 */
static int
test_set_wronly_fd(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_set_wronly_fd <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_WRONLY);

	struct pmem2_source *src;

	int ret = pmem2_source_from_fd(&src, fd);
#ifdef _WIN32
	/* windows doesn't validate open flags */
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	verify_fd(src, fd);
	ret = pmem2_source_delete(&src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);
#else
	UT_ASSERTeq(src, NULL);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_HANDLE);
#endif
	CLOSE(fd);

	return 1;
}

/*
 * test_alloc_src_enomem - test pmem2_source allocation with error injection
 */
static int
test_alloc_src_enomem(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_alloc_src_enomem <file>");

	char *file = argv[0];

	struct pmem2_source *src;
	if (!core_fault_injection_enabled()) {
		return 1;
	}
	int fd = OPEN(file, O_RDWR);

	core_inject_fault_at(PMEM_MALLOC, 1, "pmem2_malloc");

	int ret = pmem2_source_from_fd(&src, fd);
	UT_PMEM2_EXPECT_RETURN(ret, -ENOMEM);

	UT_ASSERTeq(src, NULL);
	CLOSE(fd);

	return 1;
}

/*
 * test_delete_null_config - test pmem2_source_delete on NULL config
 */
static int
test_delete_null_config(const struct test_case *tc, int argc,
		char *argv[])
{
	struct pmem2_source *src = NULL;
	/* should not crash */
	int ret = pmem2_source_delete(&src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	return 0;
}

#ifdef WIN32
/*
 * test_set_handle - test setting valid handle
 */
static int
test_set_handle(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_set_handle <file>");

	char *file = argv[0];
	HANDLE h = CreateFile(file, GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_ALWAYS, 0, NULL);
	UT_ASSERTne(h, INVALID_HANDLE_VALUE);

	struct pmem2_source *src;

	int ret = pmem2_source_from_handle(&src, h);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src->value.handle, h);

	CloseHandle(h);
	pmem2_source_delete(&src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(src, NULL);

	return 1;
}

/*
 * test_set_null_handle - test resetting handle
 */
static int
test_set_null_handle(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_source *src;

	int ret = pmem2_source_from_handle(&src, INVALID_HANDLE_VALUE);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_HANDLE);
	UT_ASSERTeq(src, NULL);

	return 0;
}

/*
 * test_set_invalid_handle - test setting invalid handle
 */
static int
test_set_invalid_handle(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_set_invalid_handle <file>");

	char *file = argv[0];
	struct pmem2_source *src;
	HANDLE h = CreateFile(file, GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_ALWAYS, 0, NULL);
	UT_ASSERTne(h, INVALID_HANDLE_VALUE);

	CloseHandle(h);

	int ret = pmem2_source_from_handle(&src, h);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_HANDLE);

	return 1;
}

/*
 * test_set_directory_handle - test setting a directory handle
 */
static int
test_set_directory_handle(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_set_directory_handle <file>");

	char *file = argv[0];
	struct pmem2_source *src;

	HANDLE h = CreateFile(file, GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_ALWAYS, FILE_FLAG_BACKUP_SEMANTICS, NULL);

	UT_ASSERTne(h, INVALID_HANDLE_VALUE);

	int ret = pmem2_source_from_handle(&src, h);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_TYPE);
	UT_ASSERTeq(src, NULL);
	CloseHandle(h);

	return 1;
}

/*
 * test_set_directory_handle - test setting a mutex handle
 */
static int
test_set_mutex_handle(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_source *src;

	HANDLE h = CreateMutex(NULL, FALSE, NULL);
	UT_ASSERTne(h, INVALID_HANDLE_VALUE);

	int ret = pmem2_source_from_handle(&src, h);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_HANDLE);
	UT_ASSERTeq(src, NULL);
	CloseHandle(h);

	return 0;
}
#else
/*
 * test_set_directory_handle - test setting directory's fd
 */
static int
test_set_directory_fd(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_set_directory_fd <file>");

	char *file = argv[0];
	struct pmem2_source *src;

	int fd = OPEN(file, O_RDONLY);

	int ret = pmem2_source_from_fd(&src, fd);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_TYPE);

	CLOSE(fd);

	return 1;
}
#endif

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_set_rw_fd),
	TEST_CASE(test_set_ro_fd),
	TEST_CASE(test_set_invalid_fd),
	TEST_CASE(test_set_wronly_fd),
	TEST_CASE(test_alloc_src_enomem),
	TEST_CASE(test_delete_null_config),
#ifdef _WIN32
	TEST_CASE(test_set_handle),
	TEST_CASE(test_set_null_handle),
	TEST_CASE(test_set_invalid_handle),
	TEST_CASE(test_set_directory_handle),
	TEST_CASE(test_set_mutex_handle),
#else
	TEST_CASE(test_set_directory_fd),
#endif
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_source");

	util_init();
	out_init("pmem2_source", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}
