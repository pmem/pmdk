/*
 * Copyright 2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * pmem2_config_get_file_size.c -- pmem2_config_get_file_size unittests
 */

/* for O_TMPFILE */
#define _GNU_SOURCE

#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "fault_injection.h"
#include "unittest.h"
#include "ut_pmem2_utils.h"
#include "ut_pmem2_config.h"
#include "config.h"
#include "out.h"

typedef void (*test_fun)(const char *path, os_off_t size);

/*
 * test_notset_fd - tests what happens when file descriptor was not set
 */
static int
test_notset_fd(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_notset_fd <file>",
				argv[0]);

	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	size_t size;
	int ret = pmem2_config_get_file_size(&cfg, &size);

	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_FILE_HANDLE_NOT_SET);

	return 1;
}

static void
init_cfg(struct pmem2_config *cfg, int fd)
{
	pmem2_config_init(cfg);
#ifdef _WIN32
	cfg->handle = (HANDLE)_get_osfhandle(fd);
#else
	cfg->fd = fd;
#endif
}

/*
 * test_normal_file - tests normal file
 */
static int
test_normal_file(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 2)
		UT_FATAL("usage: %s test_normal_file <file> <expected_size>",
				argv[0]);

	char *path = argv[0];
	os_off_t expected_size = ATOLL(argv[1]);
	int fd = OPEN(path, O_RDWR);

	struct pmem2_config cfg;
	init_cfg(&cfg, fd);

	size_t size = SIZE_MAX;
	int ret = pmem2_config_get_file_size(&cfg, &size);

	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(size, expected_size);

	CLOSE(fd);

	return 3;
}

#ifdef O_TMPFILE
/*
 * test_tmpfile - tests temporary file
 */
static int
test_tmpfile(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 2)
		UT_FATAL("usage: %s test_tmpfile <file> <requested_size>",
				argv[0]);

	char *dir = argv[0];
	os_off_t requested_size = ATOLL(argv[1]);
	int fd = OPEN(dir, O_RDWR | O_TMPFILE);
	FTRUNCATE(fd, requested_size);

	struct pmem2_config cfg;
	init_cfg(&cfg, fd);

	size_t size = SIZE_MAX;
	int ret = pmem2_config_get_file_size(&cfg, &size);

	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(size, requested_size);

	CLOSE(fd);

	return 2;
}
#endif

/*
 * test_directory - tests directory path
 */
static int
test_directory(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_tmpfile <file>", argv[0]);

	char *dir = argv[0];
	int fd = OPEN(dir, O_RDONLY);

	struct pmem2_config cfg;
	init_cfg(&cfg, fd);

	size_t size;
	int ret = pmem2_config_get_file_size(&cfg, &size);

	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_TYPE);

	CLOSE(fd);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_notset_fd),
	TEST_CASE(test_normal_file),
#ifdef O_TMPFILE
	TEST_CASE(test_tmpfile),
#endif
	TEST_CASE(test_directory),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_config_get_file_size");

	util_init();
	out_init("pmem2_config_get_file_size", "TEST_LOG_LEVEL",
			"TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}
