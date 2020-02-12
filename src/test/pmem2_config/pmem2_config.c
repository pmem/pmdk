// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * pmem_config.c -- pmem2_config unittests
 */
#include "fault_injection.h"
#include "unittest.h"
#include "ut_pmem2_utils.h"
#include "ut_pmem2_config.h"
#include "config.h"
#include "out.h"

/*
 * verify_fd -- verify value fd or handle in config
 */
static void
verify_fd(struct pmem2_config *cfg, int fd)
{
#ifdef WIN32
	UT_ASSERTeq(cfg->handle, fd != INVALID_FD ?
		(HANDLE)_get_osfhandle(fd) : INVALID_HANDLE_VALUE);
#else
	UT_ASSERTeq(cfg->fd, fd);
#endif
}

/*
 * test_cfg_create_and_delete_valid - test pmem2_config allocation
 */
static int
test_cfg_create_and_delete_valid(const struct test_case *tc, int argc,
		char *argv[])
{
	struct pmem2_config *cfg;

	int ret = pmem2_config_new(&cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);
	verify_fd(cfg, INVALID_FD);

	ret = pmem2_config_delete(&cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(cfg, NULL);

	return 0;
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
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	int fd = OPEN(file, O_RDWR);

	int ret = pmem2_config_set_fd(&cfg, fd);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	verify_fd(&cfg, fd);

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
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	int fd = OPEN(file, O_RDONLY);

	int ret = pmem2_config_set_fd(&cfg, fd);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	verify_fd(&cfg, fd);

	CLOSE(fd);

	return 1;
}

/*
 * test_set_negative - test setting negative fd
 */
static int
test_set_negative_fd(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	/* randomly picked negative number */
	int ret = pmem2_config_set_fd(&cfg, -42);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	verify_fd(&cfg, INVALID_FD);

	return 0;
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
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	/* open and close the file to get invalid fd */
	int fd = OPEN(file, O_WRONLY);
	CLOSE(fd);
	ut_suppress_crt_assert();
	int ret = pmem2_config_set_fd(&cfg, fd);
	ut_unsuppress_crt_assert();
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_HANDLE);
	verify_fd(&cfg, INVALID_FD);

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
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	int fd = OPEN(file, O_WRONLY);

	int ret = pmem2_config_set_fd(&cfg, fd);
#ifdef _WIN32
	/* windows doesn't validate open flags */
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	verify_fd(&cfg, fd);
#else
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_HANDLE);
	verify_fd(&cfg, INVALID_FD);
#endif
	CLOSE(fd);

	return 1;
}

/*
 * test_cfg_alloc_enomem - test pmem2_config allocation with error injection
 */
static int
test_alloc_cfg_enomem(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_config *cfg;
	if (!core_fault_injection_enabled()) {
		return 0;
	}
	core_inject_fault_at(PMEM_MALLOC, 1, "pmem2_malloc");

	int ret = pmem2_config_new(&cfg);
	UT_PMEM2_EXPECT_RETURN(ret, -ENOMEM);

	UT_ASSERTeq(cfg, NULL);

	return 0;
}

/*
 * test_delete_null_config - test pmem2_delete on NULL config
 */
static int
test_delete_null_config(const struct test_case *tc, int argc,
		char *argv[])
{
	struct pmem2_config *cfg = NULL;
	/* should not crash */
	int ret = pmem2_config_delete(&cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(cfg, NULL);

	return 0;
}

/*
 * test_config_set_granularity_valid - check valid granularity values
 */
static int
test_config_set_granularity_valid(const struct test_case *tc, int argc,
		char *argv[])
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);

	/* check default granularity */
	enum pmem2_granularity g =
		(enum pmem2_granularity)PMEM2_GRANULARITY_INVALID;
	UT_ASSERTeq(cfg.requested_max_granularity, g);

	/* change default granularity */
	int ret = -1;
	g = PMEM2_GRANULARITY_BYTE;
	ret = pmem2_config_set_required_store_granularity(&cfg, g);
	UT_ASSERTeq(cfg.requested_max_granularity, g);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	/* set granularity once more */
	ret = -1;
	g = PMEM2_GRANULARITY_PAGE;
	ret = pmem2_config_set_required_store_granularity(&cfg, g);
	UT_ASSERTeq(cfg.requested_max_granularity, g);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	return 0;
}

/*
 * test_config_set_granularity_invalid - check invalid granularity values
 */
static int
test_config_set_granularity_invalid(const struct test_case *tc, int argc,
		char *argv[])
{
	/* pass invalid granularity */
	int ret = 0;
	enum pmem2_granularity g_inval = 999;
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	ret = pmem2_config_set_required_store_granularity(&cfg, g_inval);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_ARG);

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
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	HANDLE h = CreateFile(file, GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_ALWAYS, 0, NULL);
	UT_ASSERTne(h, INVALID_HANDLE_VALUE);

	int ret = pmem2_config_set_handle(&cfg, h);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(cfg.handle, h);

	CloseHandle(h);

	return 1;
}

/*
 * test_set_null_handle - test resetting handle
 */
static int
test_set_null_handle(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);

	/* set the handle to something different than INVALID_HANDLE_VALUE */
	cfg.handle = NULL;

	int ret = pmem2_config_set_handle(&cfg, INVALID_HANDLE_VALUE);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(cfg.handle, INVALID_HANDLE_VALUE);

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
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	HANDLE h = CreateFile(file, GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_ALWAYS, 0, NULL);
	UT_ASSERTne(h, INVALID_HANDLE_VALUE);

	CloseHandle(h);

	int ret = pmem2_config_set_handle(&cfg, h);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_HANDLE);
	UT_ASSERTeq(cfg.handle, INVALID_HANDLE_VALUE);

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
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);

	HANDLE h = CreateFile(file, GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_ALWAYS, FILE_FLAG_BACKUP_SEMANTICS, NULL);

	UT_ASSERTne(h, INVALID_HANDLE_VALUE);

	int ret = pmem2_config_set_handle(&cfg, h);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_TYPE);
	UT_ASSERTeq(cfg.handle, INVALID_HANDLE_VALUE);
	CloseHandle(h);

	return 1;
}

/*
 * test_set_directory_handle - test setting a mutex handle
 */
static int
test_set_mutex_handle(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);

	HANDLE h = CreateMutex(NULL, FALSE, NULL);
	UT_ASSERTne(h, INVALID_HANDLE_VALUE);

	int ret = pmem2_config_set_handle(&cfg, h);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_HANDLE);
	UT_ASSERTeq(cfg.handle, INVALID_HANDLE_VALUE);
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
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);

	int fd = OPEN(file, O_RDONLY);

	int ret = pmem2_config_set_fd(&cfg, fd);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_TYPE);

	CLOSE(fd);

	return 1;
}
#endif

/*
 * test_set_offset_too_large - setting offset which is too large
 */
static int
test_set_offset_too_large(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_config cfg;

	/* let's try to set the offset which is too large */
	size_t offset = (size_t)INT64_MAX + 1;
	int ret = pmem2_config_set_offset(&cfg, offset);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OFFSET_OUT_OF_RANGE);

	return 0;
}

/*
 * test_set_offset_success - setting a valid offset
 */
static int
test_set_offset_success(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_config cfg;

	/* let's try to successfully set the offset */
	size_t offset = Ut_mmap_align;
	int ret = pmem2_config_set_offset(&cfg, offset);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(cfg.offset, offset);

	return 0;
}

/*
 * test_set_length_success - setting a valid length
 */
static int
test_set_length_success(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_config cfg;

	/* let's try to successfully set the length, can be any length */
	size_t length = Ut_mmap_align;
	int ret = pmem2_config_set_length(&cfg, length);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(cfg.length, length);

	return 0;
}

/*
 * test_set_offset_max - setting maximum possible offset
 */
static int
test_set_offset_max(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_config cfg;

	/* let's try to successfully set maximum possible offset */
	size_t offset = (INT64_MAX / Ut_mmap_align) * Ut_mmap_align;
	int ret = pmem2_config_set_offset(&cfg, offset);
	UT_ASSERTeq(ret, 0);

	return 0;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_cfg_create_and_delete_valid),
	TEST_CASE(test_set_rw_fd),
	TEST_CASE(test_set_ro_fd),
	TEST_CASE(test_set_negative_fd),
	TEST_CASE(test_set_invalid_fd),
	TEST_CASE(test_set_wronly_fd),
	TEST_CASE(test_alloc_cfg_enomem),
	TEST_CASE(test_delete_null_config),
	TEST_CASE(test_config_set_granularity_valid),
	TEST_CASE(test_config_set_granularity_invalid),
#ifdef _WIN32
	TEST_CASE(test_set_handle),
	TEST_CASE(test_set_null_handle),
	TEST_CASE(test_set_invalid_handle),
	TEST_CASE(test_set_directory_handle),
	TEST_CASE(test_set_mutex_handle),
#else
	TEST_CASE(test_set_directory_fd),
#endif
	TEST_CASE(test_set_offset_too_large),
	TEST_CASE(test_set_offset_success),
	TEST_CASE(test_set_length_success),
	TEST_CASE(test_set_offset_max),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_config");

	util_init();
	out_init("pmem2_config", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}
