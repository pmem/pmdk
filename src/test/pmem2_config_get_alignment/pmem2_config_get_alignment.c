// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * pmem2_config_get_alignment.c -- pmem2_config_get_alignment unittests
 */

#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "unittest.h"
#include "ut_pmem2_utils.h"
#include "ut_pmem2_config.h"
#include "config.h"
#include "out.h"

/*
 * test_notset_fd - tests what happens when file descriptor was not set
 */
static int
test_notset_fd(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_config cfg;
	pmem2_config_init(&cfg);
	size_t alignment;
	int ret = pmem2_config_get_alignment(&cfg, &alignment);

	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_FILE_HANDLE_NOT_SET);

	return 0;
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
 * test_get_alignment_success - simply checks returned value
 */
static int
test_get_alignment_success(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_get_alignment_success"
				" <file> [alignment]");

	int ret = 1;

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_config cfg;
	init_cfg(&cfg, fd);

	size_t alignment;
	int ret2 = pmem2_config_get_alignment(&cfg, &alignment);
	UT_PMEM2_EXPECT_RETURN(ret2, 0);

	size_t ref_alignment = Ut_mmap_align;

	/* let's check if it is DEVDAX test */
	if (argc >= 2) {
		ref_alignment = ATOUL(argv[1]);
		ret = 2;
	}

	UT_ASSERTeq(ref_alignment, alignment);

	CLOSE(fd);

	return ret;
}

/*
 * test_directory - tests directory path
 */
static int
test_directory(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_directory <file>");

	char *dir = argv[0];
	int fd = OPEN(dir, O_RDONLY);

	struct pmem2_config cfg;
	init_cfg(&cfg, fd);

	size_t alignment;
	int ret = pmem2_config_get_alignment(&cfg, &alignment);

	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_TYPE);

	CLOSE(fd);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_notset_fd),
	TEST_CASE(test_get_alignment_success),
	TEST_CASE(test_directory),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_config_get_alignment");

	util_init();
	out_init("pmem2_config_get_alignment", "TEST_LOG_LEVEL",
			"TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}
