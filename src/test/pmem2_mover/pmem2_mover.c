// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2021, Intel Corporation */

/*
 * pmem2_mover.c -- pmem2 mover tests tests
 */

#include "libpmem2.h"
#include <libminiasync.h>
#include "unittest.h"
#include "ut_pmem2.h"
#include "ut_pmem2_setup_integration.h"

/*
 * map_valid -- return valid mapped pmem2_map
 */
static struct pmem2_map *
map_valid(struct pmem2_config *cfg, struct pmem2_source *src)
{
	struct pmem2_map *map = NULL;
	int ret = pmem2_map_new(&map, cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(map, NULL);

	return map;
}

/*
 * test_mover_basic -- test basic functionality of pmem2 default mover
 */
static int
test_mover_basic(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_mover_basic <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_source *src;
	struct pmem2_config *cfg;
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
		PMEM2_GRANULARITY_PAGE);

	struct pmem2_map *map = map_valid(cfg, src);
	char *data = pmem2_map_get_address(map);
	pmem2_memset_fn memset_fn = pmem2_get_memset_fn(map);
	memset_fn(data, 0xBA, 4096, 0);
	memset_fn(data + 4096, 0xAB, 4096, 0);
	struct vdm_memcpy_future cpy =
		pmem2_memcpy_async(map, data, data + 4096, 4096, 0);

	FUTURE_BUSY_POLL(&cpy);

	if (memcmp(data, data + 4096, 4096))
		UT_FATAL("data should be equal");

	pmem2_map_delete(&map);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	CLOSE(fd);
	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_mover_basic)
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_integration");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
