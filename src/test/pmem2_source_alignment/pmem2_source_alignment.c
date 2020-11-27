// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * pmem2_source_alignment.c -- pmem2_source_alignment unittests
 */

#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "libpmem2.h"
#include "unittest.h"
#include "ut_pmem2.h"
#include "config.h"
#include "out.h"

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

	struct pmem2_source *src;
	PMEM2_SOURCE_FROM_FD(&src, fd);

	size_t alignment;
	PMEM2_SOURCE_ALIGNMENT(src, &alignment);

	size_t ref_alignment = Ut_mmap_align;

	/* let's check if it is DEVDAX test */
	if (argc >= 2) {
		ref_alignment = ATOUL(argv[1]);
		ret = 2;
	}

	UT_ASSERTeq(ref_alignment, alignment);

	PMEM2_SOURCE_DELETE(&src);
	CLOSE(fd);

	return ret;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_get_alignment_success),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_source_alignment");

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);

	DONE(NULL);
}
