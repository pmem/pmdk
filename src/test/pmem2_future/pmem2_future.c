// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/*
 * pmem2_future.c -- pmem2_future unittests
 */
#define PMEM2_USE_MINIASYNC 1

#include "libpmem2.h"
#include "out.h"
#include "unittest.h"
#include "ut_pmem2.h"
#include "ut_pmem2_setup_integration.h"
#include <libminiasync.h>

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
 * test_pmem2_future_mover - test if pmem2_*_async operations are properly
 * moved into a persistent domain by the default libpmem2 mover
 */
static int
test_pmem2_future_mover(const struct test_case *tc, int argc,
			char *argv[]) {
	if (argc < 2)
		UT_FATAL("usage: test_pmem2_future_mover <path> <size>");

	char *file = argv[0];
	size_t test_len = ATOUL(argv[1]);
	int fd = OPEN(file, O_RDWR);

	struct pmem2_source *src;
	struct pmem2_config *cfg;
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
		PMEM2_GRANULARITY_PAGE);

	struct pmem2_map *map = map_valid(cfg, src);
	char *data = pmem2_map_get_address(map);

	/*
	 * We only test memcpy operation here, because all operations
	 * use the same mechanism for assuring data persistence
	 */
	struct pmem2_future cpy =
		pmem2_memcpy_async(map, data, data + test_len, test_len, 0);

	enum pmem2_granularity gran = pmem2_map_get_store_granularity(map);

	UT_ASSERTeq(cpy.data.op.fut.data.operation.data.memcpy.flags,
			gran == PMEM2_GRANULARITY_CACHE_LINE ?
				VDM_F_MEM_DURABLE : 0);

	FUTURE_BUSY_POLL(&cpy);

	if (memcmp(data, data + test_len, test_len))
		UT_FATAL("data should be equal");

	pmem2_map_delete(&map);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	CLOSE(fd);
	return 2;
}

/*
 * test_pmem2_future_vdm - test if pmem2_*_async operations perform a call to
 * pmem2_persist_fn if libpmem2 is using vdm specified by the user
 */
static int
test_pmem2_future_vdm(const struct test_case *tc, int argc,
	char *argv[]) {
	if (argc < 2)
		UT_FATAL("usage: test_pmem2_future_vdm <path> <size>");

	char *file = argv[0];
	size_t test_len = ATOUL(argv[1]);
	int fd = OPEN(file, O_RDWR);

	struct pmem2_source *src;
	struct pmem2_config *cfg;
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
		PMEM2_GRANULARITY_PAGE);

	struct data_mover_sync *dms = data_mover_sync_new();
	UT_ASSERTne(dms, NULL);
	struct vdm *vdm = data_mover_sync_get_vdm(dms);
	UT_ASSERTne(vdm, NULL);
	pmem2_config_set_vdm(cfg, vdm);

	struct pmem2_map *map = map_valid(cfg, src);
	char *data = pmem2_map_get_address(map);

	struct pmem2_future cpy =
		pmem2_memcpy_async(map, data, data + test_len, test_len, 0);

	FUTURE_BUSY_POLL(&cpy);

	if (memcmp(data, data + test_len, test_len))
		UT_FATAL("data should be equal");

	pmem2_map_delete(&map);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	CLOSE(fd);
	return 2;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_pmem2_future_mover),
	TEST_CASE(test_pmem2_future_vdm),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_future");

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);

	DONE(NULL);
}
