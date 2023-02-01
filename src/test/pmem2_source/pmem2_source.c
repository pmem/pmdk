// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2023, Intel Corporation */

/*
 * pmem2_source.c -- pmem2_source unittests
 */
#include "fault_injection.h"
#include "file.h"
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
	UT_ASSERTeq(src->type, PMEM2_SOURCE_FD);
	UT_ASSERTeq(src->value.fd, fd);
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
	UT_ASSERTeq(src, NULL);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_HANDLE);
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

/*
 * test_pmem2_src_mcsafe_read -- test mcsafe read operation
 */
static int
test_pmem2_src_mcsafe_read(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_pmem2_src_mcsafe_read <file>");

	char *file = argv[0];
	int fd;
	struct pmem2_config *cfg;
	struct pmem2_map *map;
	struct pmem2_source *src;

	fd = OPEN(file, O_RDWR);
	UT_ASSERTne(fd, -1);

	int ret = pmem2_source_from_fd(&src, fd);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	/* set file content */
	ret = pmem2_config_new(&cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	ret = pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	ret = pmem2_map_new(&map, cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	void *addr = pmem2_map_get_address(map);
	pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(map);

	char *writebuf = "Write content";
	size_t bufsize = strlen(writebuf);
	memcpy_fn(addr, writebuf, bufsize, 0);

	ret = pmem2_map_delete(&map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	ret = pmem2_config_delete(&cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	/* verify read content */
	char *readbuf = MALLOC(bufsize);
	ret = pmem2_source_pread_mcsafe(src, readbuf, bufsize, 0);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	ret = strncmp(writebuf, readbuf, bufsize);
	ASSERTeq(ret, 0);

	FREE(readbuf);
	pmem2_source_delete(&src);
	CLOSE(fd);

	return 1;
}

/*
 * test_pmem2_src_mcsafe_write -- test mcsafe write operation
 */
static int
test_pmem2_src_mcsafe_write(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_pmem2_src_mcsafe_write <file>");

	char *file = argv[0];
	int fd;
	struct pmem2_config *cfg;
	struct pmem2_map *map;
	struct pmem2_source *src;

	fd = OPEN(file, O_RDWR);
	UT_ASSERTne(fd, -1);

	int ret = pmem2_source_from_fd(&src, fd);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	/* set file content */
	char *writebuf = "Write content";
	size_t bufsize = strlen(writebuf);
	ret = pmem2_source_pwrite_mcsafe(src, writebuf, bufsize, 0);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	/* verify read content */
	ret = pmem2_config_new(&cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	ret = pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	ret = pmem2_map_new(&map, cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	void *addr = pmem2_map_get_address(map);
	pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(map);

	char *readbuf = MALLOC(bufsize);
	memcpy_fn(readbuf, addr, bufsize, 0);
	ret = strncmp(writebuf, readbuf, bufsize);
	ASSERTeq(ret, 0);

	ret = pmem2_map_delete(&map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	ret = pmem2_config_delete(&cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	FREE(readbuf);
	pmem2_source_delete(&src);
	CLOSE(fd);

	return 1;
}

/*
 * test_pmem2_src_mcsafe_read_write_len_out_of_range -- test mcsafe read and
 *                                                      write operations with
 * length bigger than source size.
 */
static int
test_pmem2_src_mcsafe_read_write_len_out_of_range(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_pmem2_src_mcsafe_read_write_len_out_of_range <file>");

	char *file = argv[0];
	int fd;
	struct pmem2_source *src;

	fd = OPEN(file, O_RDWR);
	UT_ASSERTne(fd, -1);

	int ret = pmem2_source_from_fd(&src, fd);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	size_t src_size;
	ret = pmem2_source_size(src, &src_size);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	size_t op_size = src_size + 1;

	/* write to file */
	char *writebuf = MALLOC(op_size);
	memset(writebuf, '7', op_size);
	ret = pmem2_source_pwrite_mcsafe(src, writebuf, op_size, 0);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_LENGTH_OUT_OF_RANGE);

	/* read from file */
	char *readbuf = MALLOC(op_size);
	ret = pmem2_source_pread_mcsafe(src, readbuf, op_size, 0);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_LENGTH_OUT_OF_RANGE);

	FREE(writebuf);
	FREE(readbuf);
	pmem2_source_delete(&src);
	CLOSE(fd);

	return 1;
}

/*
 * test_pmem2_src_mcsafe_read_write_invalid_ftype -- test mcsafe read and write
 *                                                   operations on source with
 * invalid type.
 */
static int
test_pmem2_src_mcsafe_read_write_invalid_ftype(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_pmem2_src_mcsafe_read_write_invalid_ftype <file>");

	char *file = argv[0];
	int fd;
	struct pmem2_source *src;

	fd = OPEN(file, O_RDWR);
	UT_ASSERTne(fd, -1);

	/* write to file */
	char *writebuf = "Write content";
	size_t bufsize = strlen(writebuf);

	int ret = pmem2_source_from_anon(&src, bufsize);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	ret = pmem2_source_pwrite_mcsafe(src, writebuf, bufsize, 0);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_SOURCE_TYPE_NOT_SUPPORTED);

	/* read from file */
	char *readbuf = MALLOC(bufsize);
	ret = pmem2_source_pread_mcsafe(src, readbuf, bufsize, 0);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_SOURCE_TYPE_NOT_SUPPORTED);

	FREE(readbuf);
	pmem2_source_delete(&src);
	CLOSE(fd);

	return 1;
}

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

/*
 * test_get_fd - test getting file descriptor value
 */
static int
test_get_fd(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_get_fd <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDONLY);
	UT_ASSERTne(fd, -1);

	struct pmem2_source *src;
	int ret = pmem2_source_from_fd(&src, fd);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	int fd_from_pmem2;
	ret = pmem2_source_get_fd(src, &fd_from_pmem2);
	UT_ASSERTeq(fd_from_pmem2, fd);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	CLOSE(fd);
	pmem2_source_delete(&src);

	return 1;
}

/*
 * test_get_fd_inval_type - test getting fd value from invalid type
 */
static int
test_get_fd_inval_type(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_source *src;
	int ret = pmem2_source_from_anon(&src, 0);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	int fd_from_pmem2;
	ret = pmem2_source_get_fd(src, &fd_from_pmem2);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_FILE_DESCRIPTOR_NOT_SET);

	pmem2_source_delete(&src);

	return 0;
}

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
	TEST_CASE(test_pmem2_src_mcsafe_read),
	TEST_CASE(test_pmem2_src_mcsafe_write),
	TEST_CASE(test_pmem2_src_mcsafe_read_write_invalid_ftype),
	TEST_CASE(test_pmem2_src_mcsafe_read_write_len_out_of_range),
	TEST_CASE(test_set_directory_fd),
	TEST_CASE(test_get_fd),
	TEST_CASE(test_get_fd_inval_type),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_source");

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);

	DONE(NULL);
}
