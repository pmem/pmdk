// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmem2_deep_sync.c -- unit test for pmem_deep_sync()
 *
 * usage: pmem2_deep_sync file deep_persist_size offset
 *
 * pmem2_deep_sync depending on the mapping granularity is performed using one
 * of the following paths:
 * - page: NOP
 * - cache: pmem2_deep_sync_dax
 * - byte: pmem2_persist_cpu_cache + pmem2_deep_sync_dax
 *
 * Where pmem2_deep_sync_dax:
 * - pmem2_get_type_from_stat is used to determine a file type
 * - for regular files performs pmem2_flush_file_buffers_os OR
 * - for Device DAX:
 *     - is looking for Device DAX region (pmem2_device_dax_region_find)
 *     - is constructing the region deep flush file paths
 *     - opens deep_flush file (os_open)
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
 * - pmem2_device_dax_region_find (to prevent reading sysfs in search for non
 * existing Device DAXes) or mocked:
 * - os_open (to prevent opening non existing /sys// *region* /deep_flush files)
 * - write (for counting writes to non-existing /sys/ *region* /deep_flush
 * files)
 *
 * NOTE: In normal usage for page and cache line mapping granularity, before
 * pmem2_deep_sync there should be performed persist function. This test aims to
 * validate the pmem2_deep_sync function and persist function is skipped.
 */

#ifndef _WIN32
#include <sys/sysmacros.h>
#endif

#include "mmap.h"
#include "persist.h"
#include "pmem2_arch.h"
#include "pmem2_utils.h"
#include "unittest.h"

static int n_msynces = 0;
static int n_fences = 0;
static int n_flushes = 0;
static int n_writes = 0;
static enum pmem2_file_type ftype_value = PMEM2_FTYPE_REG;

#ifndef _WIN32
#define FD_MOCK 999
#define REG_ID_MOCK 1
#define DEV_ID_MOCK 777UL

/*
 * pmem2_device_dax_region_find -- redefine libpmem2 function
 */
int
pmem2_device_dax_region_find(const os_stat_t *st)
{
	dev_t dev_id = st->st_rdev;
	UT_ASSERTeq(dev_id, DEV_ID_MOCK);

	return REG_ID_MOCK;
}

#define BUS_DEVICE_PATH "/sys/bus/nd/devices/region1/deep_flush"

/*
 * os_open -- os_open mock
 */
FUNC_MOCK(os_open, int, const char *path, int flags, ...)
FUNC_MOCK_RUN_DEFAULT {
	if (strcmp(path, BUS_DEVICE_PATH) == 0) {
		UT_OUT("mocked open, path %s", path);
		if (os_access(path, R_OK))
			return FD_MOCK;
	}

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
	if (fd == FD_MOCK) {
		UT_OUT("mocked write, path %d", fd);
		++n_writes;
		return 1;
	}
	return _FUNC_REAL(write)(fd, buffer, count);
}
FUNC_MOCK_END
#endif

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
 * pmem2_get_type_from_stat -- redefine libpmem2 function
 */
int
pmem2_get_type_from_stat(const os_stat_t *st, enum pmem2_file_type *type)
{
	*type = ftype_value;
	return 0;
}

/*
 * pmem2_map_find -- redefine libpmem2 function, redefinition is needed
 * for a proper compilation of the test. NOTE: this function is not used
 * in the test.
 */
struct pmem2_map *
pmem2_map_find(const void *addr, size_t len)
{
	return NULL;
}

/*
 * pmem2_flush_file_buffers_os -- redefine libpmem2 function
 */
int
pmem2_flush_file_buffers_os(struct pmem2_map *map, const void *addr, size_t len,
		int autorestart)
{
	++n_msynces;
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
	/* mocked device ID for device DAX */
	map->src_fd_st.st_rdev = DEV_ID_MOCK;
#endif
}

/*
 * counters_check_n_reset -- check values of counts of calls of persist
 * functions in the test and reset them
 */
static void
counters_check_n_reset(int msynces, int flushes, int fences, int writes)
{
	UT_ASSERTeq(n_msynces, msynces);
	UT_ASSERTeq(n_flushes, flushes);
	UT_ASSERTeq(n_fences, fences);
	UT_ASSERTeq(n_writes, writes);

	n_msynces = 0;
	n_flushes = 0;
	n_fences = 0;
	n_writes = 0;
}

/*
 * test_deep_sync_func -- test pmem2_deep_sync for all granularity options
 */
static int
test_deep_sync_func(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_map map;
	map_init(&map);

	void *addr = map.addr;
	size_t len = map.content_length;

	map.effective_granularity = PMEM2_GRANULARITY_PAGE;
	pmem2_set_flush_fns(&map);
	int ret = pmem2_deep_sync(&map, addr, len);
	UT_ASSERTeq(ret, 0);
	counters_check_n_reset(0, 0, 0, 0);

	map.effective_granularity = PMEM2_GRANULARITY_CACHE_LINE;
	pmem2_set_flush_fns(&map);
	ret = pmem2_deep_sync(&map, addr, len);
	UT_ASSERTeq(ret, 0);
	counters_check_n_reset(1, 0, 0, 0);

	map.effective_granularity = PMEM2_GRANULARITY_BYTE;
	pmem2_set_flush_fns(&map);
	ret = pmem2_deep_sync(&map, addr, len);
	UT_ASSERTeq(ret, 0);
	counters_check_n_reset(1, 1, 1, 0);

	FREE(map.addr);

	return 0;
}

/*
 * test_deep_sync_func_devdax -- test pmem2_deep_sync with mocked DAX devices
 */
static int
test_deep_sync_func_devdax(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_map map;
	map_init(&map);

	void *addr = map.addr;
	size_t len = map.content_length;
	ftype_value = PMEM2_FTYPE_DEVDAX;

	map.effective_granularity = PMEM2_GRANULARITY_CACHE_LINE;
	pmem2_set_flush_fns(&map);
	int ret = pmem2_deep_sync(&map, addr, len);
	UT_ASSERTeq(ret, 0);
	counters_check_n_reset(0, 0, 0, 1);

	map.effective_granularity = PMEM2_GRANULARITY_BYTE;
	pmem2_set_flush_fns(&map);
	ret = pmem2_deep_sync(&map, addr, len);
	UT_ASSERTeq(ret, 0);
	counters_check_n_reset(0, 1, 1, 1);

	FREE(map.addr);

	return 0;
}

/*
 * test_deep_sync_addr_beyond_mapping -- test pmem2_deep_sync with the address
 * that goes beyond mapping
 */
static int
test_deep_sync_addr_beyond_mapping(const struct test_case *tc, int argc,
					char *argv[])
{
	struct pmem2_map map;

	map_init(&map);

	void *addr = (void *)((uintptr_t)map.addr + map.content_length + 1);
	size_t len = map.content_length;

	int ret = pmem2_deep_sync(&map, addr, len);
	UT_ASSERTeq(ret, PMEM2_E_SYNC_RANGE);

	FREE(map.addr);

	return 0;
}

/*
 * test_deep_sync_range_beyond_mapping -- test pmem2_deep_sync with a range
 * that is partially outside the mapping
 */
static int
test_deep_sync_range_beyond_mapping(const struct test_case *tc, int argc,
					char *argv[])
{
	struct pmem2_map map;
	map_init(&map);

	void *addr = (void *)((uintptr_t)map.addr + map.content_length / 2);
	size_t len = map.content_length;

	int ret = pmem2_deep_sync(&map, addr, len);
	UT_ASSERTeq(ret, PMEM2_E_SYNC_RANGE);

	FREE(map.addr);

	return 0;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_deep_sync_func),
	TEST_CASE(test_deep_sync_func_devdax),
	TEST_CASE(test_deep_sync_addr_beyond_mapping),
	TEST_CASE(test_deep_sync_range_beyond_mapping),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_deep_sync");
	pmem2_persist_init();
	util_init();
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
