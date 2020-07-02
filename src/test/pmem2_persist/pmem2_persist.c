// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * pmem2_persist.c -- pmem2_get_[flush|drain|persist]_fn unittests
 */

#include "mmap.h"
#include "persist.h"
#include "pmem2_arch.h"
#include "out.h"
#include "unittest.h"

static int n_flushes = 0;
static int n_fences = 0;
static int n_msynces = 0;

/*
 * mock_flush -- count flush calls in the test
 */
static void
mock_flush(const void *addr, size_t len)
{
	++n_flushes;
}

/*
 * mock_drain -- count drain calls in the test
 */
static void
mock_drain(void)
{
	++n_fences;
}

/*
 * pmem2_arch_init -- redefine libpmem2 function
 */
void
pmem2_arch_init(struct pmem2_arch_info *info)
{
	info->flush = mock_flush;
	info->fence = mock_drain;
}

/*
 * pmem2_map_find -- redefine libpmem2 function
 */
struct pmem2_map *
pmem2_map_find(const void *addr, size_t len)
{
	static struct pmem2_map cur;

	cur.addr = (void *)ALIGN_DOWN((size_t)addr, Pagesize);
	if ((size_t)cur.addr < (size_t)addr)
		len += (size_t)addr - (size_t)cur.addr;

	cur.reserved_length = ALIGN_UP(len, Pagesize);

	return &cur;
}

/*
 * pmem2_flush_file_buffers_os -- redefine libpmem2 function
 */
int
pmem2_flush_file_buffers_os(struct pmem2_map *map, const void *addr, size_t len,
		int autorestart)
{
	UT_ASSERTeq((uintptr_t)addr % Pagesize, 0);

	++n_msynces;

	return 0;
}

/*
 * prepare_map -- fill pmem2_map in minimal scope
 */
static void
prepare_map(struct pmem2_map *map)
{
	const size_t length = 20 * MEGABYTE + 5 * KILOBYTE;
	map->content_length = length;
	map->addr = MALLOC(length);
}

/*
 * counters_check_n_reset -- check values of counts of calls of persist
 * functions in the test and reset them
 */
static void
counters_check_n_reset(int msynces, int flushes, int fences)
{
	UT_ASSERTeq(n_msynces, msynces);
	UT_ASSERTeq(n_flushes, flushes);
	UT_ASSERTeq(n_fences, fences);

	n_msynces = 0;
	n_flushes = 0;
	n_fences = 0;
}

/*
 * do_persist -- call persist function according to a granularity
 */
static void
do_persist(struct pmem2_map *map, enum pmem2_granularity granularity)
{
	map->effective_granularity = granularity;
	pmem2_set_flush_fns(map);
	pmem2_persist_fn func = pmem2_get_persist_fn(map);
	UT_ASSERTne(func, NULL);
	func(map->addr, map->content_length);
}

/*
 * do_flush -- call flush function according to a granularity
 */
static void
do_flush(struct pmem2_map *map, enum pmem2_granularity granularity)
{
	map->effective_granularity = granularity;
	pmem2_set_flush_fns(map);
	pmem2_flush_fn func = pmem2_get_flush_fn(map);
	UT_ASSERTne(func, NULL);
	func(map->addr, map->content_length);
}

/*
 * do_drain -- call drain function according to a granularity
 */
static void
do_drain(struct pmem2_map *map, enum pmem2_granularity granularity)
{
	map->effective_granularity = granularity;
	pmem2_set_flush_fns(map);
	pmem2_drain_fn func = pmem2_get_drain_fn(map);
	UT_ASSERTne(func, NULL);
	func();
}

/*
 * test_get_persist_funcs -- test getting pmem2 persist functions
 */
static int
test_get_persist_funcs(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_map map;
	prepare_map(&map);

	do_persist(&map, PMEM2_GRANULARITY_PAGE);
	counters_check_n_reset(1, 0, 0);

	do_persist(&map, PMEM2_GRANULARITY_CACHE_LINE);
	counters_check_n_reset(0, 1, 1);

	do_persist(&map, PMEM2_GRANULARITY_BYTE);
	counters_check_n_reset(0, 0, 1);

	FREE(map.addr);

	return 0;
}

/*
 * test_get_flush_funcs -- test getting pmem2 flush functions
 */
static int
test_get_flush_funcs(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_map map;
	prepare_map(&map);

	do_flush(&map, PMEM2_GRANULARITY_PAGE);
	counters_check_n_reset(1, 0, 0);

	do_flush(&map, PMEM2_GRANULARITY_CACHE_LINE);
	counters_check_n_reset(0, 1, 0);

	do_flush(&map, PMEM2_GRANULARITY_BYTE);
	counters_check_n_reset(0, 0, 0);

	FREE(map.addr);

	return 0;
}

/*
 * test_get_drain_funcs -- test getting pmem2 drain functions
 */
static int
test_get_drain_funcs(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_map map;
	prepare_map(&map);

	do_drain(&map, PMEM2_GRANULARITY_PAGE);
	counters_check_n_reset(0, 0, 0);

	do_drain(&map, PMEM2_GRANULARITY_CACHE_LINE);
	counters_check_n_reset(0, 0, 1);

	do_drain(&map, PMEM2_GRANULARITY_BYTE);
	counters_check_n_reset(0, 0, 1);

	FREE(map.addr);

	return 0;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_get_persist_funcs),
	TEST_CASE(test_get_flush_funcs),
	TEST_CASE(test_get_drain_funcs),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_persist");
	pmem2_persist_init();
	util_init();
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
