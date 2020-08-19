// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmem2_deep_flush.c -- unit test for pmem_deep_flush()
 *
 * usage: pmem2_deep_flush file deep_persist_size offset
 *
 * pmem2_deep_flush depending on the mapping granularity is performed using one
 * of the following paths:
 * - page: NOP
 * - cache: pmem2_deep_flush_dax
 * - byte: pmem2_persist_cpu_cache + pmem2_deep_flush_dax
 *
 * Where pmem2_deep_flush_dax:
 * - pmem2_get_type_from_stat is used to determine a file type
 * - for regular files performs pmem2_flush_file_buffers_os OR
 * - for Device DAX:
 *     - is looking for Device DAX region (pmem2_get_region_id)
 *     - is constructing the region deep flush file paths
 *     - opens deep_flush file (os_open)
 *     - reads deep_flush file (read)
 *     - performs a write to it (write)
 *
 * Where pmem2_persist_cpu_cache performs:
 * - flush (replaced by mock_flush) AND
 * - drain (replaced by mock_drain)
 *
 * Additionally, for the sake of this test, the following functions are
 * replaced:
 * - pmem2_get_type_from_stat (to control perceived file type)
 * - pmem2_flush_file_buffers_os (for counting calls)
 * - pmem2_get_region_id (to prevent reading sysfs in search for non
 * existing Device DAXes)
 * or mocked:
 * - os_open (to prevent opening non existing
 * /sys/bus/nd/devices/region[0-9]+/deep_flush files)
 * - write (for counting writes to non-existing
 * /sys/bus/nd/devices/region[0-9]+/deep_flush files)
 *
 * NOTE: In normal usage the persist function precedes any call to
 * pmem2_deep_flush. This test aims to validate the pmem2_deep_flush
 * function and so the persist function is omitted.
 */

#include "source.h"
#ifndef _WIN32
#include <sys/sysmacros.h>
#endif

#include "mmap.h"
#include "persist.h"
#include "pmem2_arch.h"
#include "pmem2_utils.h"
#include "ut_pmem2_utils.h"
#include "region_namespace.h"
#include "unittest.h"

static int n_file_buffs_flushes = 0;
static int n_fences = 0;
static int n_flushes = 0;
static int n_writes = 0;
static int n_reads = 0;
static enum pmem2_file_type *ftype_value;
static int read_invalid = 0;
static int deep_flush_not_needed = 0;

#ifndef _WIN32
#define MOCK_FD 999
#define MOCK_REG_ID 888
#define MOCK_BUS_DEVICE_PATH "/sys/bus/nd/devices/region888/deep_flush"
#define MOCK_DEV_ID 777UL

/*
 * pmem2_get_region_id -- redefine libpmem2 function
 */
int
pmem2_get_region_id(const struct pmem2_source *src,
	unsigned *region_id)
{
	*region_id = MOCK_REG_ID;

	return 0;
}

/*
 * os_open -- os_open mock
 */
FUNC_MOCK(os_open, int, const char *path, int flags, ...)
FUNC_MOCK_RUN_DEFAULT {
	if (strcmp(path, MOCK_BUS_DEVICE_PATH) == 0)
		return MOCK_FD;

	va_list ap;
	va_start(ap, flags);
	int mode = va_arg(ap, int);
	va_end(ap);

	return _FUNC_REAL(os_open)(path, flags, mode);
}
FUNC_MOCK_END

/*
 * write -- write mock
 */
FUNC_MOCK(write, int, int fd, const void *buffer, size_t count)
FUNC_MOCK_RUN_DEFAULT {
	UT_ASSERTeq(*(char *)buffer, '1');
	UT_ASSERTeq(count, 1);
	UT_ASSERTeq(fd, MOCK_FD);
	++n_writes;

	return 1;
}
FUNC_MOCK_END

/*
 * read -- read mock
 */
FUNC_MOCK(read, int, int fd, void *buffer, size_t nbytes)
FUNC_MOCK_RUN_DEFAULT {
	UT_ASSERTeq(nbytes, 2);
	UT_ASSERTeq(fd, MOCK_FD);

	UT_OUT("mocked read, fd %d", fd);
	char pattern[2] = {'1', '\n'};
	int ret = sizeof(pattern);

	if (deep_flush_not_needed)
		pattern[0] = '0';

	if (read_invalid) {
		ret = 0;
		goto end;
	}

	memcpy(buffer, pattern, sizeof(pattern));

end:
	++n_reads;
	return ret;
}
FUNC_MOCK_END
#endif /* not _WIN32 */

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
 * pmem2_arch_init -- attach flush and drain functions replacements
 */
void
pmem2_arch_init(struct pmem2_arch_info *info)
{
	info->flush = mock_flush;
	info->fence = mock_drain;
}

/*
 * pmem2_map_find -- redefine libpmem2 function, redefinition is needed
 * for a proper compilation of the test. NOTE: this function is not used
 * in the test.
 */
struct pmem2_map *
pmem2_map_find(const void *addr, size_t len)
{
	UT_ASSERT(0);
	return NULL;
}

/*
 * pmem2_flush_file_buffers_os -- redefine libpmem2 function
 */
int
pmem2_flush_file_buffers_os(struct pmem2_map *map, const void *addr, size_t len,
		int autorestart)
{
	++n_file_buffs_flushes;
	return 0;
}

/*
 * map_init -- fill pmem2_map in minimal scope
 */
static void
map_init(struct pmem2_map *map)
{
	const size_t length = 8 * MEGABYTE;
	map->content_length = length;
	/*
	 * The test needs to allocate more memory because some test cases
	 * validate behavior with address beyond mapping.
	 */
	map->addr = MALLOC(2 * length);
#ifndef _WIN32
	map->source.type = PMEM2_SOURCE_FD;
	/* mocked device ID for device DAX */
	map->source.value.st_rdev = MOCK_DEV_ID;
#else
	map->source.type = PMEM2_SOURCE_HANDLE;
#endif
	ftype_value = &map->source.value.ftype;
}

/*
 * counters_check_n_reset -- check numbers of uses of deep-flushing elements
 * and reset them
 */
static void
counters_check_n_reset(int msynces, int flushes, int fences,
	int writes, int reads)
{
	UT_ASSERTeq(n_file_buffs_flushes, msynces);
	UT_ASSERTeq(n_flushes, flushes);
	UT_ASSERTeq(n_fences, fences);
	UT_ASSERTeq(n_writes, writes);
	UT_ASSERTeq(n_reads, reads);

	n_file_buffs_flushes = 0;
	n_flushes = 0;
	n_fences = 0;
	n_writes = 0;
	n_reads = 0;

	read_invalid = 0;
	deep_flush_not_needed = 0;
}

/*
 * test_deep_flush_func -- test pmem2_deep_flush for all granularity options
 */
static int
test_deep_flush_func(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_map map;
	map_init(&map);
	*ftype_value = PMEM2_FTYPE_REG;

	void *addr = map.addr;
	size_t len = map.content_length;

	map.effective_granularity = PMEM2_GRANULARITY_PAGE;
	pmem2_set_flush_fns(&map);
	int ret = pmem2_deep_flush(&map, addr, len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	counters_check_n_reset(0, 0, 0, 0, 0);

	map.effective_granularity = PMEM2_GRANULARITY_CACHE_LINE;
	pmem2_set_flush_fns(&map);
	ret = pmem2_deep_flush(&map, addr, len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	counters_check_n_reset(1, 0, 0, 0, 0);

	map.effective_granularity = PMEM2_GRANULARITY_BYTE;
	pmem2_set_flush_fns(&map);
	ret = pmem2_deep_flush(&map, addr, len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	counters_check_n_reset(1, 0, 0, 0, 0);

	FREE(map.addr);

	return 0;
}

/*
 * test_deep_flush_func_devdax -- test pmem2_deep_flush with mocked DAX devices
 */
static int
test_deep_flush_func_devdax(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_map map;
	map_init(&map);

	void *addr = map.addr;
	size_t len = map.content_length;
	*ftype_value = PMEM2_FTYPE_DEVDAX;

	map.effective_granularity = PMEM2_GRANULARITY_CACHE_LINE;
	pmem2_set_flush_fns(&map);
	int ret = pmem2_deep_flush(&map, addr, len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	counters_check_n_reset(0, 1, 1, 1, 1);

	deep_flush_not_needed = 1;
	ret = pmem2_deep_flush(&map, addr, len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	counters_check_n_reset(0, 1, 1, 0, 1);

	read_invalid = 1;
	ret = pmem2_deep_flush(&map, addr, len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	counters_check_n_reset(0, 1, 1, 0, 1);

	map.effective_granularity = PMEM2_GRANULARITY_BYTE;
	pmem2_set_flush_fns(&map);
	ret = pmem2_deep_flush(&map, addr, len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	counters_check_n_reset(0, 1, 1, 1, 1);

	deep_flush_not_needed = 1;
	ret = pmem2_deep_flush(&map, addr, len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	counters_check_n_reset(0, 1, 1, 0, 1);

	read_invalid = 1;
	ret = pmem2_deep_flush(&map, addr, len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	counters_check_n_reset(0, 1, 1, 0, 1);

	FREE(map.addr);

	return 0;
}

/*
 * test_deep_flush_range_beyond_mapping -- test pmem2_deep_flush with
 * the address that goes beyond mapping
 */
static int
test_deep_flush_range_beyond_mapping(const struct test_case *tc, int argc,
					char *argv[])
{
	struct pmem2_map map;
	map_init(&map);

	/* set address completely beyond mapping */
	void *addr = (void *)((uintptr_t)map.addr + map.content_length);
	size_t len = map.content_length;

	int ret = pmem2_deep_flush(&map, addr, len);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_DEEP_FLUSH_RANGE);

	/*
	 * set address in the middle of mapping, which makes range partially
	 * beyond mapping
	 */
	addr = (void *)((uintptr_t)map.addr + map.content_length / 2);

	ret = pmem2_deep_flush(&map, addr, len);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_DEEP_FLUSH_RANGE);

	FREE(map.addr);

	return 0;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_deep_flush_func),
	TEST_CASE(test_deep_flush_func_devdax),
	TEST_CASE(test_deep_flush_range_beyond_mapping),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_deep_flush");
	pmem2_persist_init();
	util_init();
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
