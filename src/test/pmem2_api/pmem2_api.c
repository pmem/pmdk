// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmem2_api.c -- PMEM2_API_[START|END] unittests
 */

#include "unittest.h"
#include "ut_pmem2.h"
#include "ut_pmem2_setup_integration.h"

/*
 * map_valid -- return valid mapped pmem2_map and validate mapped memory length
 */
static struct pmem2_map *
map_valid(struct pmem2_config *cfg, struct pmem2_source *src, size_t size)
{
	struct pmem2_map *map = NULL;
	PMEM2_MAP(cfg, src, &map);
	UT_ASSERTeq(pmem2_map_get_size(map), size);

	return map;
}

/*
 * test_pmem2_api_logs -- map O_RDWR file and do pmem2_[cpy|set|move]_fns
 */
static int
test_pmem2_api_logs(const struct test_case *tc, int argc,
					char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_mem_move_cpy_set_with_map_private <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);
	const char *word1 = "Persistent memory...";
	const char *word2 = "Nonpersistent memory";
	const char *word3 = "XXXXXXXXXXXXXXXXXXXX";

	struct pmem2_config *cfg;
	struct pmem2_source *src;
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
						PMEM2_GRANULARITY_PAGE);

	size_t size = 0;
	PMEM2_SOURCE_SIZE(src, &size);
	struct pmem2_map *map = map_valid(cfg, src, size);

	char *addr = pmem2_map_get_address(map);

	pmem2_memmove_fn memmove_fn = pmem2_get_memmove_fn(map);
	pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(map);
	pmem2_memset_fn memset_fn = pmem2_get_memset_fn(map);

	memcpy_fn(addr, word1, strlen(word1), 0);
	UT_ASSERTeq(strcmp(addr, word1), 0);
	memmove_fn(addr, word2, strlen(word2), 0);
	UT_ASSERTeq(strcmp(addr, word2), 0);
	memset_fn(addr, 'X', strlen(word3), 0);
	UT_ASSERTeq(strcmp(addr, word3), 0);

	/* cleanup after the test */
	pmem2_unmap(&map);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	CLOSE(fd);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_pmem2_api_logs),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_api");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
