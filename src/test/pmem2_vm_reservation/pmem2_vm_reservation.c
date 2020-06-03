// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmem2_vm_reservation.c -- pmem2_vm_reservation unittests
 */

#include <stdbool.h>

#include "config.h"
#include "pmem2_utils.h"
#include "source.h"
#include "map.h"
#include "out.h"
#include "pmem2.h"
#include "unittest.h"
#include "ut_pmem2.h"
#include "ut_pmem2_setup.h"

/*
 * test_vm_reserv_new_region_occupied_map - create a reservation
 * in the region belonging to existing mapping
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
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_source *src;
	struct pmem2_vm_reservation *rsv;
	struct FHandle *fh;

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);

	/* map a region of virtual address space */
	int ret = pmem2_map(&cfg, src, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	void *addr = pmem2_map_get_address(map);

	/* create a reservation in the region occupied by existing mapping */
	ret = pmem2_vm_reservation_new(&rsv, addr, size);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAPPING_EXISTS);

	ret = pmem2_unmap(&map);
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
	if (argc < 1)
		UT_FATAL("usage: test_vm_reserv_new_region_occupied_reserv "
				"<rsv_size>");

	size_t rsv_size = ATOUL(argv[0]);
	struct pmem2_vm_reservation *rsv1;
	struct pmem2_vm_reservation *rsv2;

	/* reserve a region in the virtual address space */
	int ret = pmem2_vm_reservation_new(&rsv1, NULL, rsv_size);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	void *addr = pmem2_vm_reservation_get_address(rsv1);
	UT_ASSERTne(addr, NULL);

	/*
	 * make a vm reservation of the region occupied by other
	 * existing reservation
	 */
	ret = pmem2_vm_reservation_new(&rsv2, addr, rsv_size);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAPPING_EXISTS);

	ret = pmem2_vm_reservation_delete(&rsv1);
	UT_ASSERTeq(ret, 0);

	return 1;
}

/*
 * test_vm_reserv_delete_contains_mapping - delete a vm reservation that
 *                                          contains mapping in the middle
 */
static int
test_vm_reserv_delete_contains_mapping(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_reserv_delete_contains_mapping "
				"<file> <size>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_source *src;
	struct pmem2_vm_reservation *rsv;
	struct FHandle *fh;

	/* create a reservation in the virtual memory */
	int ret = pmem2_vm_reservation_new(&rsv, NULL, 3 * size);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	size_t rsv_offset = size;

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0, FH_RDWR);
	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	/* create a mapping in the reserved region */
	ret = pmem2_map(&cfg, src, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	/* delete the reservation while it contains a mapping */
	ret = pmem2_vm_reservation_delete(&rsv);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_VM_RESERVATION_NOT_EMPTY);

	ret = pmem2_unmap(&map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(map, NULL);

	ret = pmem2_vm_reservation_delete(&rsv);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

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
	size_t rsv_offset;
	size_t rsv_size;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;
	struct FHandle *fh;

	rsv_size = size;
	rsv_offset = 0;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0,
			FH_RDWR);
	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	ret = pmem2_map(&cfg, src, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	UT_ASSERTeq((char *)pmem2_vm_reservation_get_address(rsv) +
			rsv_offset, pmem2_map_get_address(map));

	ret = pmem2_unmap(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);
	pmem2_vm_reservation_delete(&rsv);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_vm_reserv_map_half_file - map a half of the file to a vm reservation
 */
static int
test_vm_reserv_map_half_file(const struct test_case *tc,
		int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_vm_reserv_map_half_file <file> <size>");

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

	/* reservation size is not big enough for the whole file */
	rsv_size = size / 2;
	rsv_offset = 0;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, offset,
			FH_RDWR);
	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	ret = pmem2_map(&cfg, src, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	UT_ASSERTeq((char *)pmem2_vm_reservation_get_address(rsv) +
			rsv_offset, pmem2_map_get_address(map));

	ret = pmem2_unmap(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);
	pmem2_vm_reservation_delete(&rsv);
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
	size_t rsv_offset = 0;
	size_t rsv_size;
	size_t NMAPPINGS = 10;
	struct pmem2_config cfg;
	struct pmem2_map **map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;
	struct FHandle *fh;

	map = MALLOC(sizeof(struct pmem2_map *) * NMAPPINGS);

	rsv_size = NMAPPINGS * size;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0,
			FH_RDWR);

	for (size_t i = 0; i < NMAPPINGS; i++, rsv_offset += size) {
		pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

		ret = pmem2_map(&cfg, src, &map[i]);
		UT_PMEM2_EXPECT_RETURN(ret, 0);

		UT_ASSERTeq((char *)pmem2_vm_reservation_get_address(rsv) +
				rsv_offset, pmem2_map_get_address(map[i]));
	}

	for (size_t i = 0; i < NMAPPINGS; i += 2) {
		ret = pmem2_unmap(&map[i]);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(map[i], NULL);
	}

	rsv_offset = 0;
	for (size_t i = 0; i < NMAPPINGS; i += 2, rsv_offset += 2 * size) {
		pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

		ret = pmem2_map(&cfg, src, &map[i]);
		UT_PMEM2_EXPECT_RETURN(ret, 0);

		UT_ASSERTeq((char *)pmem2_vm_reservation_get_address(rsv) +
				rsv_offset, pmem2_map_get_address(map[i]));
	}

	for (size_t i = 0; i < NMAPPINGS; i++) {
		ret = pmem2_unmap(&map[i]);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(map[i], NULL);
	}

	pmem2_vm_reservation_delete(&rsv);
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
	size_t rsv_size;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;
	struct FHandle *fh;

	rsv_size = size / 2;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0,
			FH_RDWR);
	pmem2_config_set_vm_reservation(&cfg, rsv, 0);

	ret = pmem2_map(&cfg, src, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_LENGTH_OUT_OF_RANGE);

	pmem2_vm_reservation_delete(&rsv);
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
	size_t rsv_size;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_map *overlap_map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;
	struct FHandle *fh;

	rsv_size = size;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0,
			FH_RDWR);
	pmem2_config_set_vm_reservation(&cfg, rsv, 0);

	ret = pmem2_map(&cfg, src, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	ret = pmem2_map(&cfg, src, &overlap_map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAPPING_EXISTS);

	ret = pmem2_unmap(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);
	pmem2_vm_reservation_delete(&rsv);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

/*
 * test_vm_reserv_map_partial_overlap_below - map a file to a vm reservation
 * overlapping with the ealier half of the other existing mapping
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
	size_t rsv_size;
	size_t rsv_offset = 0;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_map *overlap_map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;
	struct FHandle *fh;

	rsv_size = size + size / 2;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0,
			FH_RDWR);
	rsv_offset = size / 2;
	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	ret = pmem2_map(&cfg, src, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	rsv_offset = 0;
	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	ret = pmem2_map(&cfg, src, &overlap_map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAPPING_EXISTS);

	ret = pmem2_unmap(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);
	pmem2_vm_reservation_delete(&rsv);
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
	size_t rsv_size;
	size_t rsv_offset = 0;
	struct pmem2_config cfg;
	struct pmem2_map *map;
	struct pmem2_map *overlap_map;
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;
	struct FHandle *fh;

	rsv_size = size + size / 2;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0,
			FH_RDWR);
	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	ret = pmem2_map(&cfg, src, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	rsv_offset = size / 2;
	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	ret = pmem2_map(&cfg, src, &overlap_map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAPPING_EXISTS);

	ret = pmem2_unmap(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);
	pmem2_vm_reservation_delete(&rsv);
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

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, offset,
			FH_RDWR);
	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	/* spoil requested granularity */
	enum pmem2_granularity gran = cfg.requested_max_granularity;
	cfg.requested_max_granularity = PMEM2_GRANULARITY_BYTE;

	ret = pmem2_map(&cfg, src, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_GRANULARITY_NOT_SUPPORTED);

	/* map whole file */
	offset = 0;
	rsv_offset = 0;

	/* restore correct granularity */
	cfg.requested_max_granularity = gran;
	cfg.offset = offset;

	pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

	ret = pmem2_map(&cfg, src, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	UT_ASSERTeq((char *)pmem2_vm_reservation_get_address(rsv) +
		rsv_offset, pmem2_map_get_address(map));

	ret = pmem2_unmap(&map);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(map, NULL);
	pmem2_vm_reservation_delete(&rsv);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 2;
}

#define MAX_THREADS 32

static unsigned N_threads;
static unsigned Ops_per_thread;

struct worker_args {
	struct pmem2_config *cfg;
	struct pmem2_source *src;
	struct pmem2_map **map;
};

static void *
map_worker(void *arg)
{
	struct worker_args *warg = arg;

	for (unsigned i = 0; i < Ops_per_thread; i++) {
		int ret = pmem2_map(warg->cfg, warg->src, warg->map);
		if (ret != PMEM2_E_MAPPING_EXISTS)
			UT_PMEM2_EXPECT_RETURN(ret, 0);
	}

	return NULL;
}

static void *
unmap_worker(void *arg)
{
	struct worker_args *warg = arg;

	for (unsigned i = 0; i < Ops_per_thread; i++) {
		if (*(warg->map))
			pmem2_unmap(warg->map);
	}

	return NULL;
}

static void
run_worker(void *(worker_func)(void *arg), struct worker_args args[])
{
	os_thread_t threads[MAX_THREADS];

	for (unsigned i = 0; i < N_threads; i++)
		THREAD_CREATE(&threads[i], NULL, worker_func, &args[i]);

	for (unsigned i = 0; i < N_threads; i++)
		THREAD_JOIN(&threads[i], NULL);
}

/*
 * test_vm_reserv_async_map_unmap_multiple_files - map and unmap
 * asynchronously multiple files to the vm reservation. Mappings
 * will occur to 7 different overlapping regions of the vm reservation.
 */
static int
test_vm_reserv_async_map_unmap_multiple_files(const struct test_case *tc,
	int argc, char *argv[])
{
	if (argc < 4)
		UT_FATAL("usage: test_vm_reserv_async_map_unmap_multiple_files"
				"<file> <size> <threads> <ops/thread>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);
	N_threads = ATOU(argv[2]);
	Ops_per_thread = ATOU(argv[3]);
	size_t rsv_size;
	size_t rsv_offset;
	struct pmem2_config cfg;
	struct pmem2_map *map[MAX_THREADS];
	struct pmem2_vm_reservation *rsv;
	struct pmem2_source *src;
	struct FHandle *fh;

	rsv_size = 4 * size;
	rsv_offset = 0;

	int ret = pmem2_vm_reservation_new(&rsv, NULL, rsv_size);
	UT_ASSERTeq(ret, 0);

	struct worker_args args[MAX_THREADS];

	ut_pmem2_prepare_config(&cfg, &src, &fh, FH_FD, file, 0, 0,
			FH_RDWR);

	for (size_t n = 0; n < N_threads; n++) {
		rsv_offset = (n % 7) * (size / 2);
		pmem2_config_set_vm_reservation(&cfg, rsv, rsv_offset);

		args[n].cfg = &cfg;
		args[n].src = src;
		args[n].map = &map[n];
	}

	run_worker(map_worker, args);
	run_worker(unmap_worker, args);

	pmem2_vm_reservation_delete(&rsv);
	PMEM2_SOURCE_DELETE(&src);
	UT_FH_CLOSE(fh);

	return 4;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_vm_reserv_new_region_occupied_map),
	TEST_CASE(test_vm_reserv_new_region_occupied_reserv),
	TEST_CASE(test_vm_reserv_delete_contains_mapping),
	TEST_CASE(test_vm_reserv_map_file),
	TEST_CASE(test_vm_reserv_map_half_file),
	TEST_CASE(test_vm_reserv_map_unmap_multiple_files),
	TEST_CASE(test_vm_reserv_map_insufficient_space),
	TEST_CASE(test_vm_reserv_map_full_overlap),
	TEST_CASE(test_vm_reserv_map_partial_overlap_above),
	TEST_CASE(test_vm_reserv_map_partial_overlap_below),
	TEST_CASE(test_vm_reserv_map_invalid_granularity),
	TEST_CASE(test_vm_reserv_async_map_unmap_multiple_files),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_vm_reservation");
	util_init();
	out_init("pmem2_vm_reservation", "TEST_LOG_LEVEL", "TEST_LOG_FILE",
			0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();
	DONE(NULL);
}

#ifdef _MSC_VER
MSVC_CONSTR(libpmem2_init)
MSVC_DESTR(libpmem2_fini)
#endif
