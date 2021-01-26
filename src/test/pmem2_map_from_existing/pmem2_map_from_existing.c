// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * pmem2_map_from_existing.c -- pmem2_map_from_existing unittests
 */

#include <stdbool.h>
#include "fault_injection.h"
#include "unittest.h"
#include "ut_pmem2_utils.h"

/*
 * test_two_same_mappings - try to create two the same mappings
 */
static int
test_two_same_mappings(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_map *map1 = NULL;
	struct pmem2_map *map2 = NULL;
	struct pmem2_source *src;

	int fd = OPEN(argv[0], O_RDWR);
	int ret = pmem2_source_from_fd(&src, fd);
	UT_ASSERTeq(ret, 0);

	ret = pmem2_map_from_existing(&map1, src, (void *)0xFFFF, 0xFF,
		PMEM2_GRANULARITY_PAGE);

	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(map1, NULL);

	ret = pmem2_map_from_existing(&map2, src, (void *)0xFFFF, 0xFF,
		PMEM2_GRANULARITY_PAGE);

	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAP_EXISTS);
	UT_ASSERTeq(map2, NULL);

	pmem2_map_delete(&map1);
	CLOSE(fd);
	return 1;
}

/*
 * test_mapping_overlap_bottom - try to map which overlap
 *		bottom part of existing mapping
 */
static int
test_mapping_overlap_bottom(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_map *map1 = NULL;
	struct pmem2_map *map2 = NULL;
	struct pmem2_source *src;

	int fd = OPEN(argv[0], O_RDWR);
	int ret = pmem2_source_from_fd(&src, fd);
	UT_ASSERTeq(ret, 0);

	ret = pmem2_map_from_existing(&map1, src, (void *)0xFFFF, 0xFF,
		PMEM2_GRANULARITY_PAGE);

	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(map1, NULL);

	ret = pmem2_map_from_existing(&map2, src, (void *)0xFFF0, 0xFF,
		PMEM2_GRANULARITY_PAGE);

	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAP_EXISTS);
	UT_ASSERTeq(map2, NULL);

	pmem2_map_delete(&map1);
	CLOSE(fd);
	return 1;
}

/*
 * test_mapping_overlap_upper - try to map which overlap
 *		upper part of existing mapping
 */
static int
test_mapping_overlap_upper(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_map *map1 = NULL;
	struct pmem2_map *map2 = NULL;
	struct pmem2_source *src;

	int fd = OPEN(argv[0], O_RDWR);
	int ret = pmem2_source_from_fd(&src, fd);
	UT_ASSERTeq(ret, 0);

	ret = pmem2_map_from_existing(&map1, src, (void *)0x0FFFF, 0xFF,
		PMEM2_GRANULARITY_PAGE);

	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(map1, NULL);

	ret = pmem2_map_from_existing(&map2, src, (void *)(0x0FFFF + 0x1),
		0xFFFF, PMEM2_GRANULARITY_PAGE);

	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAP_EXISTS);
	UT_ASSERTeq(map2, NULL);

	pmem2_map_delete(&map1);
	CLOSE(fd);
	return 1;
}

/*
 * test_map_allocation_enomem - inject enomem in to allocation of map object
 */
static int
test_map_allocation_enomem(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_map *map = NULL;
	struct pmem2_source *src;

	if (!core_fault_injection_enabled()) {
		return 1;
	}

	int fd = OPEN(argv[0], O_RDWR);
	int ret = pmem2_source_from_fd(&src, fd);
	UT_ASSERTeq(ret, 0);

	core_inject_fault_at(PMEM_MALLOC, 1, "pmem2_malloc");
	ret = pmem2_map_from_existing(&map, src, (void *)0x0FFFF, 0xFF,
		PMEM2_GRANULARITY_PAGE);

	UT_PMEM2_EXPECT_RETURN(ret, -ENOMEM);
	UT_ASSERTeq(map, NULL);

	CLOSE(fd);
	return 1;
}

/*
 * test_register_mapping_enomem - inject enomem during adding map to ravl
 */
static int
test_register_mapping_enomem(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_map *map = NULL;
	struct pmem2_source *src;

	if (!core_fault_injection_enabled()) {
		return 1;
	}

	int fd = OPEN(argv[0], O_RDWR);
	int ret = pmem2_source_from_fd(&src, fd);
	UT_ASSERTeq(ret, 0);

	core_inject_fault_at(PMEM_MALLOC, 1, "ravl_new_node");
	ret = pmem2_map_from_existing(&map, src, (void *)0x0FFFF, 0xFF,
		PMEM2_GRANULARITY_PAGE);

	UT_PMEM2_EXPECT_RETURN(ret, -ENOMEM);
	UT_ASSERTeq(map, NULL);

	CLOSE(fd);
	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_two_same_mappings),
	TEST_CASE(test_mapping_overlap_bottom),
	TEST_CASE(test_mapping_overlap_upper),
	TEST_CASE(test_map_allocation_enomem),
	TEST_CASE(test_register_mapping_enomem),

};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_map_from_existing");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}

#ifdef _MSC_VER
MSVC_CONSTR(libpmem2_init)
MSVC_DESTR(libpmem2_fini)
#endif
