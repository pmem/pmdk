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
 * pmem2_map.c -- pmem2_get_[flush|drain|persist]_fn unittests
 */

#include "persist.h"
#include "pmem2_arch.h"
#include "out.h"
#include "unittest.h"

#define MEGABYTE (1 << 20)

int n_flushes = 0;
int n_fences = 0;
int n_msynces = 0;

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
 * pmem2_arch_init -- initialize list of pmem operations in basic scope,
 * redefine original implementation of this function
 */
void
pmem2_arch_init(struct pmem2_arch_info *info)
{
	info->flush = mock_flush;
	info->fence = mock_drain;
}

/*
 * pmem2_get_mapping -- finds the earliest mapping overlapping with
 * [addr, addr+size) range, redefine original implementation of this function
 */
struct pmem2_map *
pmem2_get_mapping(const void *addr, size_t len)
{
	static struct pmem2_map cur;

	cur.addr = (void *)ALIGN_DOWN((size_t)addr, Pagesize);
	if ((size_t)cur.addr < (size_t)addr)
		len = len + (size_t)addr - (size_t)cur.addr;

	if ((len % (size_t)Pagesize))
		cur.reserved_length = (size_t)ALIGN_UP(len, (size_t)Pagesize);
	else
		cur.reserved_length = len;

	return &cur;
}

/*
 * os_flush_file_buffers -- count msync calls in the test, redefine original
 * implementation of this function
 */
int
os_flush_file_buffers(struct pmem2_map *map, const void *addr, size_t len,
		int autorestart)
{
	++n_msynces;

	return 0;
}

/*
 * prepare_map -- fill pmem2_map in minimal scope
 */
static void
prepare_map(struct pmem2_map *map)
{
	map->content_length = 20 * MEGABYTE;
	map->addr = malloc(20 * MEGABYTE);
}

/*
 * check_counters -- check values of counts of calls of persist functions
 * in the test
 */
static void
check_counters(int msynces, int flushes, int fences)
{
	UT_ASSERTeq(n_msynces, msynces);
	UT_ASSERTeq(n_flushes, flushes);
	UT_ASSERTeq(n_fences, fences);
}

/*
 * do_persist -- call persist function depends on the set granularity
 */
static void
do_persist(struct pmem2_map *map, enum pmem2_granularity granularity)
{
	map->effective_granularity = granularity;
	pmem2_persist_fn func = pmem2_get_persist_fn(map);
	func(map->addr, map->content_length);
}

/*
 * do_flush -- call flush function depends on the set granularity
 */
static void
do_flush(struct pmem2_map *map, enum pmem2_granularity granularity)
{
	map->effective_granularity = granularity;
	pmem2_flush_fn func = pmem2_get_flush_fn(map);
	func(map->addr, map->content_length);
}

/*
 * do_drain -- call drain function depends on the set granularity
 */
static void
do_drain(struct pmem2_map *map, enum pmem2_granularity granularity)
{
	map->effective_granularity = granularity;
	pmem2_drain_fn func = pmem2_get_drain_fn(map);
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
	check_counters(1, 0, 0);

	do_persist(&map, PMEM2_GRANULARITY_CACHE_LINE);
	check_counters(1, 1, 1);

	do_persist(&map, PMEM2_GRANULARITY_BYTE);
	check_counters(1, 1, 2);

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
	check_counters(1, 0, 0);

	do_flush(&map, PMEM2_GRANULARITY_CACHE_LINE);
	check_counters(1, 1, 0);

	do_flush(&map, PMEM2_GRANULARITY_BYTE);
	check_counters(1, 1, 0);

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
	check_counters(0, 0, 0);

	do_drain(&map, PMEM2_GRANULARITY_CACHE_LINE);
	check_counters(0, 0, 1);

	do_drain(&map, PMEM2_GRANULARITY_BYTE);
	check_counters(0, 0, 2);

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
	/* XXX to be done */
	// pmem2_persist_fini();
	DONE(NULL);
}
