// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * pmem2_vm_reservation.c -- pmem2_vm_reservation unittests
 */

#include <stdbool.h>
#ifndef _WIN32
#include <pthread.h>
#endif

#include "config.h"
#include "fault_injection.h"
#include "pmem2_utils.h"
#include "source.h"
#include "map.h"
#include "out.h"
#include "pmem2.h"
#include "unittest.h"
#include "ut_pmem2.h"
#include "ut_pmem2_setup.h"

/*
 * get_align_by_filename -- fetch map alignment for an unopened file
 */
static size_t
get_align_by_filename(const char *filename)
{
	struct pmem2_source *src;
	size_t align;
	int fd = OPEN(filename, O_RDONLY);
	PMEM2_SOURCE_FROM_FD(&src, fd);
	PMEM2_SOURCE_ALIGNMENT(src, &align);
	PMEM2_SOURCE_DELETE(&src);
	CLOSE(fd);

	return align;
}

/*
 * offset_align_to_devdax - align offset of the virtual memory reservation
 *                          to device DAX granularity
 */
static size_t
offset_align_to_devdax(void *rsv_addr, size_t alignment)
{
	/*
	 * Address of the vm_reservation, is always aligned to the OS allocation
	 * granularity. DevDax demands its own granularity, we need to calculate
	 * the offset, so that (reservation address + offset) is aligned to the
	 * closest address, contained in the vm reservation, compatible with
	 * DevDax granularity.
	 */
	size_t mod_align = (size_t)rsv_addr % alignment;
	if (mod_align)
		return (alignment - mod_align);

	return 0;
}

/*
 * test_vm_reserv_new_valid_addr - map a file to the desired addr with the
 *                                 help of virtual memory reservation
 */
static int
test_vm_reserv_new_valid_addr(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_new_valid_addr "
				"<file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	void *rsv_addr;
	size_t rsv_size;
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);

	int ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	rsv_addr = pmem2_map_get_address(map);

	/* unmap the mapping after getting the address */
	ret = pmem2_map_delete(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);

	/*
	 * there's no need for padding in case of DevDax since the address
	 * we get from the first mapping is already aligned
	 */
	rsv_size = size;

	ret = pmem2_vm_reservation_new(&rsv, rsv_addr, rsv_size);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(pmem2_vm_reservation_get_address(rsv), rsv_addr);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	pmem2_config_set_vm_reservation(&cfg, rsv, 0);

	ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(pmem2_map_get_address(map), rsv_addr);

	ret = pmem2_map_delete(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);
	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_vm_reserv_new_region_occupied_map - create a reservation
 * in the region overlapping whole existing mapping
 */
static int
test_vm_reserv_new_region_occupied_map(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_new_region_occupied_map "
				"<file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	void *addr;
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_source *src;
	struct pmem2_vm_reservation *rsv;

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);

	/* map a region of virtual address space */
	int ret = pmem2_map_new(&map, &cfg, src);
	UT_ASSERTeq(ret, 0);

	addr = pmem2_map_get_address(map);
	UT_ASSERTne(addr, NULL);

	/* create a reservation in the region occupied by existing mapping */
	ret = pmem2_vm_reservation_new(&rsv, addr, size);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAPPING_EXISTS);

	ret = pmem2_map_delete(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_vm_reserv_new_region_occupied_map_below - create a reservation
 * in the region overlapping lower half of the existing mapping
 */
static int
test_vm_reserv_new_region_occupied_map_below(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_new_region_occupied_map_below "
				"<file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	size_t alignment = get_align_by_filename(file);
	void *rsv_addr;
	size_t rsv_size;
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);

	int ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	/*
	 * address of the mapping is already aligned, we need to align
	 * the half of the size in case of DevDax
	 */
	rsv_addr = (char *)pmem2_map_get_address(map) -
			ALIGN_UP(size / 2, alignment);

	/*
	 * there's no need for padding in case of DevDax since the address
	 * we get from the first mapping is already aligned
	 */
	rsv_size = size;

	ret = pmem2_vm_reservation_new(&rsv, rsv_addr, rsv_size);
	UT_ASSERTeq(ret, PMEM2_E_MAPPING_EXISTS);
	UT_ASSERTeq(rsv, NULL);

	/* unmap the mapping after getting the address */
	ret = pmem2_map_delete(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);

	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_vm_reserv_new_region_occupied_map_above - create a reservation
 * in the region overlapping upper half of the existing mapping
 */
static int
test_vm_reserv_new_region_occupied_map_above(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_new_region_occupied_map_above "
				"<file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	size_t alignment = get_align_by_filename(file);
	void *rsv_addr;
	size_t rsv_size;
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);

	int ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	/*
	 * address of the mapping is already aligned, we need to align
	 * the half of the size in case of DevDax
	 */
	rsv_addr = (char *)pmem2_map_get_address(map) +
			ALIGN_DOWN(size / 2, alignment);

	/*
	 * there's no need for padding in case of DevDax since the address
	 * we get from the first mapping is already aligned
	 */
	rsv_size = size;

	ret = pmem2_vm_reservation_new(&rsv, rsv_addr, rsv_size);
	UT_ASSERTeq(ret, PMEM2_E_MAPPING_EXISTS);
	UT_ASSERTeq(rsv, NULL);

	/* unmap the mapping after getting the address */
	ret = pmem2_map_delete(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);

	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_vm_reserv_new_region_occupied_reserv - create a vm reservation
 * in the region belonging to other existing vm reservation
 */
static int
test_vm_reserv_new_region_occupied_reserv(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_new_region_occupied_reserv "
				"<file> <size>");

	size_t size = ATOUL(argv[1]);
	void *rsv_addr;
	struct pmem2_vm_reservation *rsv1;
	struct pmem2_vm_reservation *rsv2;

	/* reserve a region in the virtual address space */
	int ret = pmem2_vm_reservation_new(&rsv1, NULL, size);
	UT_ASSERTeq(ret, 0);

	rsv_addr = pmem2_vm_reservation_get_address(rsv1);
	UT_ASSERTne(rsv_addr, NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv1), size);

	/*
	 * Make a vm reservation of the region occupied by other
	 * existing reservation.
	 */
	ret = pmem2_vm_reservation_new(&rsv2, rsv_addr, size);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAPPING_EXISTS);

	ret = pmem2_vm_reservation_delete(&rsv1);
	UT_ASSERTeq(ret, 0);

	return 2;
}

/*
 * test_vm_reserv_new_unaligned_addr - create a vm reservation with
 *                                     unaligned address provided
 */
static int
test_vm_reserv_new_unaligned_addr(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_new_unaligned_addr "
				"<file> <size>");

	size_t size = ATOUL(argv[1]);
	void *rsv_addr = (char *)Mmap_align - 1; /* unaligned address */
	struct pmem2_vm_reservation *rsv;

	/* reserve a region in the virtual address space */
	int ret = pmem2_vm_reservation_new(&rsv, rsv_addr, size);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_ADDRESS_UNALIGNED);

	return 2;
}

/*
 * test_vm_reserv_new_unaligned_size - create a vm reservation with
 *                                     unaligned size provided
 */
static int
test_vm_reserv_new_unaligned_size(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_new_unaligned_size "
				"<file> <size>");

	size_t size = ATOUL(argv[1]) - 1; /* unaligned size */
	struct pmem2_vm_reservation *rsv;

	/* reserve a region in the virtual address space */
	int ret = pmem2_vm_reservation_new(&rsv, NULL, size);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_LENGTH_UNALIGNED);

	return 2;
}

/*
 * test_vm_reserv_new_alloc_enomem - create a vm reservation
 *                                   with error injection
 */
static int
test_vm_reserv_new_alloc_enomem(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_new_alloc_enomem "
				"<file> <size>");

	size_t size = ATOUL(argv[1]);
	struct pmem2_vm_reservation *rsv;

	if (!core_fault_injection_enabled()) {
		return 2;
	}
	core_inject_fault_at(PMEM_MALLOC, 1, "pmem2_malloc");

	/* reserve a region in the virtual address space */
	int ret = pmem2_vm_reservation_new(&rsv, NULL, size);
	UT_PMEM2_EXPECT_RETURN(ret, -ENOMEM);

	return 2;
}

/*
 * test_vm_reserv_map_file - map a file to a vm reservation
 */
static int
test_vm_reserv_map_file(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_map_file <file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	size_t alignment = get_align_by_filename(file);
	void *rsv_addr;
	size_t rsv_offset;
	size_t rsv_size;
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;

	rsv_size = size + alignment;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);

	rsv_addr = pmem2_vm_reservation_get_address(rsv);
	UT_ASSERTne(rsv_addr, 0);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	/* in case of DevDax */
	rsv_offset = offset_align_to_devdax(rsv_addr, alignment);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);
	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	UT_ASSERTne(map, NULL);
	UT_ASSERTeq(pmem2_map_get_address(map), (char *)rsv_addr + rsv_offset);

	ret = pmem2_map_delete(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);
	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_vm_reserv_map_part_file - map a part of the file to a vm reservation
 */
static int
test_vm_reserv_map_part_file(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_map_part_file <file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	size_t alignment = get_align_by_filename(file);
	size_t offset = 0;
	void *rsv_addr;
	size_t rsv_offset;
	size_t rsv_size;
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;

	/* map only part of the file */
	offset = ALIGN_DOWN(size / 2, alignment);

	/* reservation size is not big enough for the whole file */
	rsv_size = size - offset + alignment;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);

	rsv_addr = pmem2_vm_reservation_get_address(rsv);
	UT_ASSERTne(rsv_addr, 0);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	/* in case of DevDax */
	rsv_offset = offset_align_to_devdax(rsv_addr, alignment);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, offset,
			FH_RDWR);
	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	UT_ASSERTeq(pmem2_map_get_address(map), (char *)rsv_addr + rsv_offset);

	ret = pmem2_map_delete(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);
	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_vm_reserv_delete_contains_mapping - delete a vm reservation that
 *                                          contains mapping
 */
static int
test_vm_reserv_delete_contains_mapping(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_delete_contains_mapping "
				"<file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	size_t alignment = get_align_by_filename(file);
	void *rsv_addr;
	size_t rsv_size;
	size_t rsv_offset;
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_source *src;
	struct pmem2_vm_reservation *rsv;

	rsv_size = size + alignment;

	/* create a reservation in the virtual memory */
	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);

	rsv_addr = pmem2_vm_reservation_get_address(rsv);
	UT_ASSERTne(rsv_addr, 0);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	/* in case of DevDax */
	rsv_offset = offset_align_to_devdax(rsv_addr, alignment);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);
	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	/* create a mapping in the reserved region */
	ret = pmem2_map_new(&map, &cfg, src);
	UT_ASSERTeq(ret, 0);

	/* delete the reservation while it contains a mapping */
	ret = pmem2_vm_reservation_delete(&rsv);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_VM_RESERVATION_NOT_EMPTY);

	ret = pmem2_map_delete(&map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(map, NULL);
	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_vm_reserv_map_unmap_multiple_files - map multiple files to a
 * vm reservation, then unmap every 2nd mapping and map the mapping again
 */
static int
test_vm_reserv_map_unmap_multiple_files(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_map_unmap_multiple_files "
				"<file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	size_t alignment = get_align_by_filename(file);
	void *rsv_addr;
	size_t rsv_offset;
	size_t rsv_size;
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_map **map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;
	size_t NMAPPINGS = 10;

	map = MALLOC(sizeof(struct pmem2_map *) * NMAPPINGS);

	rsv_size = NMAPPINGS * size + alignment;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);

	rsv_addr = pmem2_vm_reservation_get_address(rsv);
	UT_ASSERTne(rsv_addr, NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	/* in case of DevDax */
	size_t align_offset = offset_align_to_devdax(rsv_addr, alignment);
	rsv_offset = align_offset;

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);

	for (size_t i = 0; i < NMAPPINGS; i++, rsv_offset += size) {
		pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

		ret = pmem2_map_new(&map[i], &cfg, src);
		UT_PMEM2_EXPECT_RETURN(ret, 0);

		UT_ASSERTeq((char *)rsv_addr + rsv_offset,
				pmem2_map_get_address(map[i]));
	}

	for (size_t i = 0; i < NMAPPINGS; i += 2) {
		ret = pmem2_map_delete(&map[i]);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(map[i], NULL);
	}

	rsv_offset = align_offset;
	for (size_t i = 0; i < NMAPPINGS; i += 2, rsv_offset += 2 * size) {
		pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

		ret = pmem2_map_new(&map[i], &cfg, src);
		UT_PMEM2_EXPECT_RETURN(ret, 0);

		UT_ASSERTeq((char *)rsv_addr + rsv_offset,
				pmem2_map_get_address(map[i]));
	}

	for (size_t i = 0; i < NMAPPINGS; i++) {
		ret = pmem2_map_delete(&map[i]);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(map[i], NULL);
	}

	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);
	FREE(map);

	return 2;
}

/*
 * test_vm_reserv_map_insufficient_space - map a file to a vm reservation
 *                                         with insufficient space
 */
static int
test_vm_reserv_map_insufficient_space(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_map_insufficient_space "
				"<file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	void *rsv_addr;
	size_t rsv_size;
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;

	rsv_size = size / 2;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	rsv_addr = pmem2_vm_reservation_get_address(rsv);
	UT_ASSERTne(rsv_addr, NULL);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);
	pmem2_config_set_vm_reservation(&cfg, rsv, 0);

	ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_LENGTH_OUT_OF_RANGE);

	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_vm_reserv_map_full_overlap - map a file to a vm reservation
 *                                   and overlap existing mapping
 */
static int
test_vm_reserv_map_full_overlap(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_map_full_overlap "
				"<file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	size_t alignment = get_align_by_filename(file);
	void *rsv_addr;
	size_t rsv_offset;
	size_t rsv_size;
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_map *overlap_map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;

	rsv_size = size + alignment;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);

	rsv_addr = pmem2_vm_reservation_get_address(rsv);
	UT_ASSERTne(rsv_addr, NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	/* in case of DevDax */
	rsv_offset = offset_align_to_devdax(rsv_addr, alignment);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);
	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	ret = pmem2_map_new(&overlap_map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAPPING_EXISTS);

	ret = pmem2_map_delete(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);
	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_vm_reserv_map_partial_overlap_below - map a file to a vm reservation
 * overlapping with the earlier half of the other existing mapping
 */
static int
test_vm_reserv_map_partial_overlap_below(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_map_partial_overlap_below "
				"<file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	size_t alignment = get_align_by_filename(file);
	void *rsv_addr;
	size_t rsv_size;
	size_t rsv_offset;
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_map *overlap_map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;

	rsv_size = size + size / 2 + alignment;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);

	rsv_addr = pmem2_vm_reservation_get_address(rsv);
	UT_ASSERTne(rsv_addr, NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	/* in case of DevDax */
	size_t offset_align = offset_align_to_devdax(rsv_addr, alignment);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);

	rsv_offset = ALIGN_DOWN(size / 2, alignment) + offset_align;
	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	ret = pmem2_map_new(&map, &cfg, src);
	UT_ASSERTeq(ret, 0);

	rsv_offset = offset_align;
	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	ret = pmem2_map_new(&overlap_map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAPPING_EXISTS);

	ret = pmem2_map_delete(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);
	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_vm_reserv_map_partial_overlap_above - map a file to a vm reservation
 * overlapping with the latter half of the other existing mapping
 */
static int
test_vm_reserv_map_partial_overlap_above(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_map_partial_overlap_above "
				"<file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	size_t alignment = get_align_by_filename(file);
	void *rsv_addr;
	size_t rsv_size;
	size_t rsv_offset;
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_map *overlap_map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;

	rsv_size = size + size / 2 + alignment;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);

	rsv_addr = pmem2_vm_reservation_get_address(rsv);
	UT_ASSERTne(rsv_addr, NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	/* in case of DevDax */
	size_t offset_align = offset_align_to_devdax(rsv_addr, alignment);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);

	rsv_offset = offset_align;
	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	ret = pmem2_map_new(&map, &cfg, src);
	UT_ASSERTeq(ret, 0);

	rsv_offset = ALIGN_DOWN(size / 2, alignment) + offset_align;
	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	ret = pmem2_map_new(&overlap_map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAPPING_EXISTS);

	ret = pmem2_map_delete(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);
	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_vm_reserv_map_invalid_granularity - map a file with invalid granularity
 * to a vm reservation in the middle of the vm reservation bigger than
 * the file, then map a file that covers whole vm reservation
 */
static int
test_vm_reserv_map_invalid_granularity(const struct test_case *tc,
	int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_map_invalid_granularity "
				"<file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	size_t offset = 0;
	size_t rsv_offset;
	size_t rsv_size;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;
	struct FHandle *fh;

	/* map only half of the file */
	offset = size / 2;

	rsv_size = size;
	/* map it to the middle of the vm reservation */
	rsv_offset = size / 4;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, offset,
			FH_RDWR);
	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	/* spoil requested granularity */
	enum pmem2_granularity gran = cfg.requested_max_granularity;
	cfg.requested_max_granularity = PMEM2_GRANULARITY_BYTE;

	ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_GRANULARITY_NOT_SUPPORTED);

	/* map whole file */
	offset = 0;
	rsv_offset = 0;

	/* restore correct granularity */
	cfg.requested_max_granularity = gran;
	cfg.offset = offset;

	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	ret = pmem2_map_new(&map, &cfg, src);
	UT_ASSERTeq(ret, 0);

	UT_ASSERTeq((char *)pmem2_vm_reservation_get_address(rsv) +
		rsv_offset, pmem2_map_get_address(map));

	ret = pmem2_map_delete(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);
	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

#define MAX_THREADS 32

struct worker_args {
	size_t n_ops;
	struct pmem2_vm_reservation *rsv;
	size_t rsv_offset;
	struct FHandle *fh;
};

static void *
map_unmap_worker(void *arg)
{
	struct worker_args *warg = arg;

	struct pmem2_vm_reservation *rsv = warg->rsv;
	struct FHandle *fh = warg->fh;

	void *rsv_addr;
	size_t rsv_offset;
	size_t n_ops = warg->n_ops;
	struct pmem2_config cfg;
	struct pmem2_source *src;
	struct pmem2_map *map = NULL;

	rsv_addr  = pmem2_vm_reservation_get_address(rsv);
	rsv_offset = warg->rsv_offset;

	pmem2_config_init(&cfg);
	pmem2_config_set_required_store_granularity(&cfg,
			PMEM2_GRANULARITY_PAGE);
	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);
	PMEM2_SOURCE_FROM_FH(&src, fh);

	int ret;
	for (size_t n = 0; n < n_ops; n++) {
		if (map == NULL) {
			ret = pmem2_map_new(&map, &cfg, src);
			UT_ASSERTeq(ret, 0);
			UT_ASSERTeq(pmem2_map_get_address(map),
					(char *)rsv_addr + rsv_offset);
		}

		if (map != NULL) {
			ret = pmem2_map_delete(&map);
			UT_ASSERTeq(ret, 0);
			UT_ASSERTeq(map, NULL);
		}
	}

	PMEM2_SOURCE_DELETE(&src);

	return NULL;
}

static void
run_worker(void *(worker_func)(void *arg), struct worker_args args[],
		size_t n_threads)
{
	os_thread_t threads[MAX_THREADS];

#ifdef _WIN32
	for (size_t n = 0; n < n_threads; n++)
		THREAD_CREATE(&threads[n], NULL, worker_func, &args[n]);
#else
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	/* thread stack size is set to 16MB */
	pthread_attr_setstacksize(&attr, (1 << 24));

	for (size_t n = 0; n < n_threads; n++)
		THREAD_CREATE(&threads[n], (os_thread_attr_t *)&attr,
				worker_func, &args[n]);
#endif

	for (size_t n = 0; n < n_threads; n++)
		THREAD_JOIN(&threads[n], NULL);
}

/*
 * test_vm_reserv_async_map_unmap_multiple_files - map and unmap
 * asynchronously multiple files to the vm reservation. Mappings
 * will occur to 3 different overlapping regions of the vm reservation.
 */
static int
test_vm_reserv_async_map_unmap_multiple_files(const struct test_case *tc,
	int argc, char *argv[])
{
	if (argc < 4)
		UT_FATAL("usage: test_vm_reserv_async_map_unmap_multiple_files "
				"<file> <size> <threads> <ops/thread>");

	size_t n_threads = ATOU(argv[2]);
	if (n_threads > MAX_THREADS)
		UT_FATAL("threads %zu > MAX_THREADS %u",
				n_threads, MAX_THREADS);

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	size_t ops_per_thread = ATOU(argv[3]);
	size_t alignment = get_align_by_filename(file);
	void *rsv_addr;
	size_t rsv_size;
	size_t rsv_offset;
	struct pmem2_vm_reservation *rsv;
	struct FHandle *fh;
	struct worker_args args[MAX_THREADS];

	/*
	 * reservation will fit as many files as there are threads + 1,
	 * it's expanded by the length of alignment, for the device DAX
	 */
	rsv_size = n_threads * size + alignment;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);

	rsv_addr = pmem2_vm_reservation_get_address(rsv);
	UT_ASSERTne(rsv_addr, NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	fh = UT_FH_OPEN(FH_FD, file, FH_RDWR);

	/* in case of DevDax */
	size_t offset_align = offset_align_to_devdax(rsv_addr, alignment);

	/*
	 * the offset increases by the size of the file.
	 */
	for (size_t n = 0; n < n_threads; n++) {
		/* calculate offset for each thread */
		rsv_offset = ALIGN_DOWN(n * size, alignment) + offset_align;

		args[n].n_ops = ops_per_thread;
		args[n].rsv = rsv;
		args[n].rsv_offset = rsv_offset;
		args[n].fh = fh;
	}

	run_worker(map_unmap_worker, args, n_threads);

	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);
	UT_FH_CLOSE(fh);

	return 4;
}

/*
 * test_vm_reserv_empty_extend - extend the empty vm reservation
 */
static int
test_vm_reserv_empty_extend(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_empty_extend "
				"<file> <size>");

	size_t size = ATOUL(argv[1]);
	size_t rsv_size;
	struct pmem2_vm_reservation *rsv;

	rsv_size = size;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(pmem2_vm_reservation_get_address(rsv), NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	/*
	 * Extend the reservation by another file size. Since vm reservation
	 * can't always be extended, proceed with the test only if it is
	 * extended.
	 */
	ret = pmem2_vm_reservation_extend(rsv, size);
	if (ret == PMEM2_E_MAPPING_EXISTS)
		goto err_cleanup;
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), 2 * size);

err_cleanup:
	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);

	return 2;
}

/*
 * test_vm_reserv_map_extend - map a file to a vm reservation, extend the
 *                             reservation and map again
 */
static int
test_vm_reserv_map_extend(const struct test_case *tc,
	int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_map_extend "
			"<file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	size_t rsv_size;
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_map *second_map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;

	rsv_size = size;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(pmem2_vm_reservation_get_address(rsv), NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);
	pmem2_config_set_vm_reservation(&cfg, rsv, 0);

	ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	/*
	 * Extend the reservation by another file size. Since vm reservation
	 * can't always be extended, proceed with the test only if it is
	 * extended.
	 */
	ret = pmem2_vm_reservation_extend(rsv, size);
	if (ret == PMEM2_E_MAPPING_EXISTS)
		goto err_cleanup;
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), 2 * size);

	/* try mapping the file after the first file */
	pmem2_config_set_vm_reservation(&cfg, rsv, size);
	ret = pmem2_map_new(&second_map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	ret = pmem2_map_delete(&second_map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(second_map, NULL);

err_cleanup:
	ret = pmem2_map_delete(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);

	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_vm_reserv_unaligned_extend - extend the empty vm reservation by
 *                                   unaligned size
 */
static int
test_vm_reserv_unaligned_extend(const struct test_case *tc,
	int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_unaligned_extend "
			"<file> <size>");

	size_t size = ATOUL(argv[1]);
	struct pmem2_vm_reservation *rsv;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, size);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(pmem2_vm_reservation_get_address(rsv), NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), size);

	ret = pmem2_vm_reservation_extend(rsv, size - 1);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_LENGTH_UNALIGNED);

	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);

	return 2;
}

/*
 * test_vm_reserv_empty_shrink - shrink the empty vm reservation from the start,
 *                               then from the end, lastly map a file to it
 */
static int
test_vm_reserv_empty_shrink(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_empty_shrink "
			"<file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	size_t rsv_size;
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;

	rsv_size = 3 * size;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(pmem2_vm_reservation_get_address(rsv), NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	/* shrink the reservation by 1x file size from the start */
	ret = pmem2_vm_reservation_shrink(rsv, 0, size);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), 2 * size);

	/* shrink the reservation by 1x file size from the end */
	ret = pmem2_vm_reservation_shrink(rsv, size, size);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), size);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);
	pmem2_config_set_vm_reservation(&cfg, rsv, 0);

	ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	ret = pmem2_map_delete(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);

	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_vm_reserv_map_shrink - map a file to the reservation, shrink the
 *                             reservation from the start and from the end
 */
static int
test_vm_reserv_map_shrink(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_map_shrink "
			"<file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	size_t rsv_size;
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;

	rsv_size = 3 * size;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(pmem2_vm_reservation_get_address(rsv), NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);
	/* map a file in the middle of the reservation */
	pmem2_config_set_vm_reservation(&cfg, rsv, size);

	ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	/* shrink the reservation by 1x file size from the start */
	ret = pmem2_vm_reservation_shrink(rsv, 0, size);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), 2 * size);

	/* shrink the reservation by 1x file size from the end */
	ret = pmem2_vm_reservation_shrink(rsv, size, size);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), size);

	ret = pmem2_map_delete(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);

	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_vm_reserv_unaligned_shrink - shrink the empty vm reservation with
 *                                   unaligned offset, then with unaligned size
 */
static int
test_vm_reserv_unaligned_shrink(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_unaligned_shrink "
			"<file> <size>");

	size_t size = ATOUL(argv[1]);
	size_t rsv_size;
	struct pmem2_vm_reservation *rsv;

	rsv_size = 2 * size;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(pmem2_vm_reservation_get_address(rsv), NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	/*
	 * shrink the reservation by 1x file size from the offset
	 * of 1x file size - 1
	 */
	ret = pmem2_vm_reservation_shrink(rsv, size - 1, size);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OFFSET_UNALIGNED);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), 2 * size);

	/*
	 * shrink the reservation by 1x file size - 1 from the offset
	 * of 1x file size
	 */
	ret = pmem2_vm_reservation_shrink(rsv, size, size - 1);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_LENGTH_UNALIGNED);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), 2 * size);

	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);

	return 2;
}

/*
 * test_vm_reserv_out_of_range_shrink - shrink the empty vm reservation by
 * interval (offset, offset + size) that is out of available range for the
 * reservation to be shrunk.
 */
static int
test_vm_reserv_out_of_range_shrink(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_out_of_range_shrink "
			"<file> <size>");

	size_t size = ATOUL(argv[1]);
	size_t rsv_size;
	struct pmem2_vm_reservation *rsv;

	rsv_size = 2 * size;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(pmem2_vm_reservation_get_address(rsv), NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	/*
	 * shrink the reservation by 1x file size from the offset 3x file size
	 */
	ret = pmem2_vm_reservation_shrink(rsv, 3 * size, size);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OFFSET_OUT_OF_RANGE);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), 2 * size);

	/* shrink the reservation by 3x file size from the offset 0 */
	ret = pmem2_vm_reservation_shrink(rsv, 0, 3 * size);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_LENGTH_OUT_OF_RANGE);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), 2 * size);

	/* shrink the reservation by 0 from the offset 0 */
	ret = pmem2_vm_reservation_shrink(rsv, 0, 0);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_LENGTH_OUT_OF_RANGE);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), 2 * size);

	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);

	return 2;
}

/*
 * test_vm_reserv_unsupported_shrink - shrink the empty vm reservation from the
 * middle, then try shrinking reservation by its whole range.
 */
static int
test_vm_reserv_unsupported_shrink(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_unsupported_shrink "
			"<file> <size>");

	size_t size = ATOUL(argv[1]);
	size_t rsv_size;
	struct pmem2_vm_reservation *rsv;

	rsv_size = 3 * size;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(pmem2_vm_reservation_get_address(rsv), NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	/*
	 * shrink the reservation by 1x file size from the offset 1x file size
	 */
	ret = pmem2_vm_reservation_shrink(rsv, size, size);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_NOSUPP);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), 3 * size);

	/* shrink the reservation by its whole range */
	ret = pmem2_vm_reservation_shrink(rsv, 0, 3 * size);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_NOSUPP);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), 3 * size);

	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);

	return 2;
}

/*
 * test_vm_reserv_occupied_region_shrink - shrink the vm reservation by
 *                                         the region that is occupied
 */
static int
test_vm_reserv_occupied_region_shrink(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_occupied_region_shrink "
			"<file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	size_t rsv_size;
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;

	rsv_size = 2 * size;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(pmem2_vm_reservation_get_address(rsv), NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);
	/* map a file in the middle of the reservation */
	pmem2_config_set_vm_reservation(&cfg, rsv, size);

	ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	/*
	 * shrink the reservation by 1x file size from the offset 1x file size
	 */
	ret = pmem2_vm_reservation_shrink(rsv, size, size);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_VM_RESERVATION_NOT_EMPTY);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), 2 * size);

	ret = pmem2_map_delete(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);

	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_vm_reserv_one_map_find - create a reservation with exactly the size of
 * a file and map a file to it, search for the mapping with the following
 * intervals (offset, size): 1. (reserv_start, reserv_middle),
 * 2. (reserv_middle, reserv_end), 3. (reserv_start, reserv_end)
 */
static int
test_vm_reserv_one_map_find(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_one_map_find "
			"<file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	size_t rsv_size;
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;

	rsv_size = size;
	size_t reserv_half = rsv_size / 2;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(pmem2_vm_reservation_get_address(rsv), NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);
	pmem2_config_set_vm_reservation(&cfg, rsv, 0);

	ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	void *map_addr = pmem2_map_get_address(map);

	struct pmem2_map *fmap;
	/* search for the mapping at interval (reserv_start, reserv_middle) */
	ret = pmem2_vm_reservation_map_find(rsv, 0, reserv_half, &fmap);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map_addr, pmem2_map_get_address(fmap));

	/* search for the mapping at interval (reserv_middle, reserv_end) */
	ret = pmem2_vm_reservation_map_find(rsv, reserv_half, reserv_half,
			&fmap);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map_addr, pmem2_map_get_address(fmap));

	/* search for the mapping at interval (reserv_start, reserv_end) */
	ret = pmem2_vm_reservation_map_find(rsv, 0, rsv_size, &fmap);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map_addr, pmem2_map_get_address(fmap));

	ret = pmem2_map_delete(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);

	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_vm_reserv_two_maps_find - create a reservation with exactly the size of
 * a 2x file size and map a file to it two times, occupying the whole
 * reservation, search for the mapping with the following
 * intervals (offset, size): 1. (reserv_start, reserv_middle),
 * 2. (reserv_middle, reserv_end), 3. (reserv_start, reserv_end)
 */
static int
test_vm_reserv_two_maps_find(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_two_maps_find "
			"<file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	size_t rsv_size;
	struct FHandle *fh;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_map *second_map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;

	rsv_size = 2 * size;
	size_t reserv_half = rsv_size / 2;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(pmem2_vm_reservation_get_address(rsv), NULL);
	UT_ASSERTeq(pmem2_vm_reservation_get_size(rsv), rsv_size);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);

	pmem2_config_set_vm_reservation(&cfg, rsv, 0);
	ret = pmem2_map_new(&map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	pmem2_config_set_vm_reservation(&cfg, rsv, reserv_half);
	ret = pmem2_map_new(&second_map, &cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	void *map_addr = pmem2_map_get_address(map);
	void *second_map_addr = pmem2_map_get_address(second_map);

	struct pmem2_map *fmap;
	/* search for the mapping at interval (reserv_start, reserv_middle) */
	ret = pmem2_vm_reservation_map_find(rsv, 0, reserv_half, &fmap);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map_addr, pmem2_map_get_address(fmap));

	/* search for the mapping at interval (reserv_middle, reserv_end) */
	ret = pmem2_vm_reservation_map_find(rsv, reserv_half, reserv_half,
			&fmap);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(second_map_addr, pmem2_map_get_address(fmap));

	/* search for the mapping at interval (reserv_start, reserv_end) */
	ret = pmem2_vm_reservation_map_find(rsv, 0, rsv_size, &fmap);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map_addr, pmem2_map_get_address(fmap));

	ret = pmem2_map_delete(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);

	ret = pmem2_map_delete(&second_map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(second_map, NULL);

	ret = pmem2_vm_reservation_delete(&rsv);
	UT_ASSERTeq(ret, 0);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_vm_reserv_new_unaligned_addr),
	TEST_CASE(test_vm_reserv_new_unaligned_size),
	TEST_CASE(test_vm_reserv_new_valid_addr),
	TEST_CASE(test_vm_reserv_new_region_occupied_map),
	TEST_CASE(test_vm_reserv_new_region_occupied_map_below),
	TEST_CASE(test_vm_reserv_new_region_occupied_map_above),
	TEST_CASE(test_vm_reserv_new_region_occupied_reserv),
	TEST_CASE(test_vm_reserv_new_alloc_enomem),
	TEST_CASE(test_vm_reserv_map_file),
	TEST_CASE(test_vm_reserv_map_part_file),
	TEST_CASE(test_vm_reserv_delete_contains_mapping),
	TEST_CASE(test_vm_reserv_map_unmap_multiple_files),
	TEST_CASE(test_vm_reserv_map_insufficient_space),
	TEST_CASE(test_vm_reserv_map_full_overlap),
	TEST_CASE(test_vm_reserv_map_partial_overlap_above),
	TEST_CASE(test_vm_reserv_map_partial_overlap_below),
	TEST_CASE(test_vm_reserv_map_invalid_granularity),
	TEST_CASE(test_vm_reserv_async_map_unmap_multiple_files),
	TEST_CASE(test_vm_reserv_empty_extend),
	TEST_CASE(test_vm_reserv_map_extend),
	TEST_CASE(test_vm_reserv_unaligned_extend),
	TEST_CASE(test_vm_reserv_empty_shrink),
	TEST_CASE(test_vm_reserv_map_shrink),
	TEST_CASE(test_vm_reserv_unaligned_shrink),
	TEST_CASE(test_vm_reserv_out_of_range_shrink),
	TEST_CASE(test_vm_reserv_unsupported_shrink),
	TEST_CASE(test_vm_reserv_occupied_region_shrink),
	TEST_CASE(test_vm_reserv_one_map_find),
	TEST_CASE(test_vm_reserv_two_maps_find),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_vm_reservation");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}

#ifdef _MSC_VER
MSVC_CONSTR(libpmem2_init)
MSVC_DESTR(libpmem2_fini)
#endif
