// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmem2_deep_sync.c -- unit test for pmem_deep_sync()
 *
 * usage: pmem2_deep_sync file deep_persist_size offset
 *
 */

#include "map.h"
#include "mmap.h"
#include "persist.h"
#include "pmem2_deep_sync.h"
#include "pmem2_utils.h"
#include "unittest.h"

int n_msynces = 0;
int n_persists = 0;
int is_devdax = 0;

/*
 * pmem2_persist_mock -- count pmem2_persist calls in the test
 */
void
pmem2_persist_mock(const void *addr, size_t len)
{
	n_persists++;
}

/*
 * map_init -- fill pmem2_map in minimal scope
 */
static void
map_init(struct pmem2_map *map)
{
	const size_t length = 20 * MEGABYTE + 5 * KILOBYTE;
	map->content_length = length;
	map->addr = MALLOC(length);
#ifndef _WIN32
	map->map_st = MALLOC(sizeof(os_stat_t));
	/*
	 * predefined 'st_rdev' value is needed to hardcode the mocked path
	 * which is required to emulate DAX devices
	 */
	map->map_st->st_rdev = 60041;
#endif
}

/*
* map_cleanup -- cleanup pmem2_map
*/
static void
map_cleanup(struct pmem2_map *map)
{
#ifndef _WIN32
	FREE(map->map_st);
#endif
	FREE(map->addr);
}

/*
 * counters_check_n_reset -- check values of counts of calls of persist
 * functions in the test and reset them
 */
static void
counters_check_n_reset(int msynces, int persists)
{
	UT_ASSERTeq(n_msynces, msynces);
	UT_ASSERTeq(n_persists, persists);

	n_msynces = 0;
	n_persists = 0;
}

/*
 * test_get_persist_func -- test runs pmem2_deep_sync for all granularity
 * options
 */
static int
test_deep_sync_func(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_map map;
	map_init(&map);

	void *addr = map.addr;
	size_t len = map.content_length;

	map.effective_granularity = PMEM2_GRANULARITY_PAGE;
	int ret = pmem2_deep_sync(&map, addr, len);
	UT_ASSERTeq(ret, 0);
	counters_check_n_reset(0, 0);

	map.effective_granularity = PMEM2_GRANULARITY_CACHE_LINE;
	ret = pmem2_deep_sync(&map, addr, len);
	UT_ASSERTeq(ret, 0);
	counters_check_n_reset(1, 0);

	map.effective_granularity = PMEM2_GRANULARITY_BYTE;
	pmem2_set_flush_fns(&map);
	ret = pmem2_deep_sync(&map, addr, len);
	UT_ASSERTeq(ret, 0);
	counters_check_n_reset(1, 1);

	map_cleanup(&map);

	return 0;
}

/*
 * test_get_persist_func -- test runs pmem2_deep_sync with mocked DAX devices
 */
static int
test_deep_sync_func_devdax(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_map map;
	map_init(&map);

	void *addr = map.addr;
	size_t len = map.content_length;

	is_devdax = 1;
	map.effective_granularity = PMEM2_GRANULARITY_CACHE_LINE;
	int ret = pmem2_deep_sync(&map, addr, len);
	UT_ASSERTeq(ret, 0);
	counters_check_n_reset(0, 0);

	map.effective_granularity = PMEM2_GRANULARITY_BYTE;
	pmem2_set_flush_fns(&map);
	ret = pmem2_deep_sync(&map, addr, len);
	UT_ASSERTeq(ret, 0);
	counters_check_n_reset(0, 1);

	map_cleanup(&map);

	return 0;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_deep_sync_func),
	TEST_CASE(test_deep_sync_func_devdax),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_deep_sync");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
