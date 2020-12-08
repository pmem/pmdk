// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2020, Intel Corporation */

/*
 * pmem2_source_numa.c -- unit test for getting numa node from source
 */

#include <fcntl.h>
#include <ndctl/libndctl.h>

#include "libpmem2.h"
#include "unittest.h"
#include "ut_pmem2.h"
#include "out.h"

static int given_numa_node;

static int
test_get_numa_node(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2) {
		UT_FATAL("usage: test_get_numa_node "
				"file numa_node");
	}

	char *file = argv[0];
	given_numa_node = atoi(argv[1]);

	int fd = OPEN(file, O_CREAT | O_RDWR, 0666);
	struct pmem2_source *src;
	PMEM2_SOURCE_FROM_FD(&src, fd);

	int numa_node;
	int ret = pmem2_source_numa_node(src, &numa_node);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(numa_node, given_numa_node);

	PMEM2_SOURCE_DELETE(&src);
	CLOSE(fd);

	return 2;
}

FUNC_MOCK(pmem2_region_namespace, int, struct ndctl_ctx *ctx,
			const struct pmem2_source *src,
			struct ndctl_region **pregion,
			struct ndctl_namespace **pndns)
FUNC_MOCK_RUN_DEFAULT {
	*pregion = (void *)0x1;
	return 0;
} FUNC_MOCK_END

FUNC_MOCK(ndctl_region_get_numa_node, int, const struct ndctl_region *pregion)
FUNC_MOCK_RUN_DEFAULT {
	if (pregion != NULL) {
		return given_numa_node;
	}
	UT_FATAL("region is null");
}
FUNC_MOCK_END

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_get_numa_node),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_source_numa");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
