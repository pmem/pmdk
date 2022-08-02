// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2022, Intel Corporation */

/*
 * pmem2_mover.c -- pmem2 mover tests
 */

#define PMEM2_USE_MINIASYNC 1

#include "libpmem2.h"
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
 * test_mover_memcpy_basic -- test basic functionality of pmem2 default mover
 */
static int
test_mover_memcpy_basic(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_mover_memcpy_basic <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_source *src;
	struct pmem2_config *cfg;
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
		PMEM2_GRANULARITY_PAGE);

	struct pmem2_map *map = map_valid(cfg, src);
	char *data = pmem2_map_get_address(map);
	pmem2_memset_fn memset_fn = pmem2_get_memset_fn(map);
	memset_fn(data, 0xBA, 4096, 0);
	memset_fn(data + 4096, 0xAB, 4096, 0);
	struct pmem2_future cpy =
		pmem2_memcpy_async(map, data, data + 4096, 4096, 0);

	FUTURE_BUSY_POLL(&cpy);

	if (memcmp(data, data + 4096, 4096))
		UT_FATAL("data should be equal");

	pmem2_map_delete(&map);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	CLOSE(fd);
	return 1;
}

/*
 * test_mover_memmove_basic -- test memmove functionality of the default mover
 * using two overlapping buffers: [1,2,3,4,0,0] -> [1,2,1,2,3,4]
 *                                 A   B            A   B
 */
static int
test_mover_memmove_basic(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_mover_memmove_basic <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);
	unsigned string_size = 255;

	struct pmem2_source *src;
	struct pmem2_config *cfg;
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
		PMEM2_GRANULARITY_PAGE);

	struct pmem2_map *map = map_valid(cfg, src);
	char *data = pmem2_map_get_address(map);
	pmem2_memcpy_fn pmem2_memcpy = pmem2_get_memcpy_fn(map);

	/*
	 * Create data for first buffer
	 */
	char *buffer_data = malloc(string_size);
	for (unsigned i = 0; i < string_size; i++) {
		buffer_data[i] = (char)i;
	}
	/*
	 * Fill first buffer with numbers from 0 to 255
	 */
	pmem2_memcpy(data, buffer_data, string_size, 0);

	/*
	 * Create future to copy contents of first buffer into second buffer
	 * which starts in the half of first buffer.
	 */
	struct pmem2_future move = pmem2_memmove_async(
		map, data + string_size / 2, data, string_size, 0);

	FUTURE_BUSY_POLL(&move);

	if (memcmp(data + string_size / 2, buffer_data, string_size))
		UT_FATAL("data should be equal");

	pmem2_map_delete(&map);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	CLOSE(fd);
	return 1;
}

/*
 * test_mover_memmove_basic -- test memmove functionality of the default mover
 * by filling a buffer with some data and then using memset to overwrite it
 */
static int
test_mover_memset_basic(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_mover_memset_basic <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);
	unsigned array_size = 4096;

	struct pmem2_source *src;
	struct pmem2_config *cfg;
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
		PMEM2_GRANULARITY_PAGE);

	struct pmem2_map *map = map_valid(cfg, src);
	char *data = pmem2_map_get_address(map);
	char *memset_result = malloc(array_size);
	memset(memset_result, 5, array_size);

	/*
	 * Fill buffer with some data different from a result of memset
	 */
	for (unsigned i = 0; i < array_size; i++) {
		data[i] = (char)i;
	}

	/*
	 * Create future to set every byte of the buffer to 5
	 */
	struct pmem2_future move =
		pmem2_memset_async(map, data, 5, array_size, 0);

	FUTURE_BUSY_POLL(&move);

	if (memcmp(data, memset_result, array_size))
		UT_FATAL("data should be equal");

	pmem2_map_delete(&map);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	CLOSE(fd);
	return 1;
}

#define WORKER_RUNS 20000
#define TEST_SIZE 4096
struct thread_arg {
    struct pmem2_map *map;
    void *addr;
    unsigned threads;
    unsigned thread_id;
};

/*
 * thread_memcpy_worker -- thread worker for test_mover_memcpy_multithread
 */
static void *
thread_memcpy_worker(void *arg)
{
	struct thread_arg *targ = arg;
	unsigned *pattern1, *pattern2;

	UT_COMPILE_ERROR_ON(TEST_SIZE % sizeof(*pattern1) != 0);
	unsigned array_size = TEST_SIZE / sizeof(*pattern1);

	pattern1 = MALLOC(array_size * sizeof(*pattern1));
	pattern2 = MALLOC(array_size * sizeof(*pattern2));

	for (unsigned j = 0; j < array_size; j++) {
		pattern1[j] = targ->thread_id + 1 * targ->threads;
		pattern2[j] = targ->thread_id + 2 * targ->threads;
	}

	for (int i = 0; i < WORKER_RUNS; i++) {
		unsigned *pattern = i % 2 ? pattern1 : pattern2;
		struct pmem2_future cpy = pmem2_memcpy_async(
			targ->map, targ->addr, pattern, TEST_SIZE, 0);

		FUTURE_BUSY_POLL(&cpy);

		if (memcmp(pattern, targ->addr, TEST_SIZE))
			UT_FATAL("data should be equal");
	}
	FREE(pattern1);
	FREE(pattern2);
	return NULL;
}

/*
 * test_mover_memcpy_multithreaded -- multi-threaded test for the mover's memcpy
 */
static int
test_mover_memcpy_multithreaded(const struct test_case *tc, int argc,
	char *argv[])
{
	if (argc < 2)
		UT_FATAL(
		"usage: test_mover_memcpy_multithreaded <file> <threads>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);
	unsigned long num_threads = STRTOUL(argv[1], NULL, 10);

	struct pmem2_source *src;
	struct pmem2_config *cfg;
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
		PMEM2_GRANULARITY_PAGE);

	struct pmem2_map *map = map_valid(cfg, src);

	UT_ASSERT(pmem2_map_get_size(map) >= TEST_SIZE * num_threads);

	os_thread_t *threads = MALLOC(num_threads * sizeof(*threads));
	struct thread_arg *args = MALLOC(num_threads * sizeof(*args));

	char *data = pmem2_map_get_address(map);
	for (unsigned i = 0; i < num_threads; i++) {
		args[i].map = map;
		args[i].addr = data + i * TEST_SIZE;
		args[i].threads = num_threads;
		args[i].thread_id = i;

		THREAD_CREATE(&threads[i], NULL, thread_memcpy_worker,
			&args[i]);
	}

	for (unsigned i = 0; i < num_threads; i++) {
		THREAD_JOIN(&threads[i], NULL);
	}

	FREE(threads);
	FREE(args);

	pmem2_map_delete(&map);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	CLOSE(fd);
	return 2;
}

/*
 * thread_memmove_worker -- thread worker for test_mover_memmove_multithread
 */
static void *
thread_memove_worker(void *arg)
{
	struct thread_arg *targ = arg;
	unsigned *pattern1, *pattern2;

	UT_COMPILE_ERROR_ON(TEST_SIZE % sizeof(*pattern1) != 0);
	unsigned array_size = TEST_SIZE / sizeof(*pattern1);

	pattern1 = MALLOC(array_size * sizeof(*pattern1));
	pattern2 = MALLOC(array_size * sizeof(*pattern2));

	for (unsigned j = 0; j < array_size; j++) {
		pattern1[j] = targ->thread_id + 1 * targ->threads;
		pattern2[j] = targ->thread_id + 2 * targ->threads;
	}

	for (int i = 0; i < WORKER_RUNS; i++) {
		unsigned *pattern = i % 2 ? pattern1 : pattern2;
		struct pmem2_future move = pmem2_memmove_async(
			targ->map, targ->addr, pattern, TEST_SIZE, 0);

		FUTURE_BUSY_POLL(&move);

		if (memcmp(pattern, targ->addr, TEST_SIZE))
			UT_FATAL("data should be equal");
	}
	FREE(pattern1);
	FREE(pattern2);
	return NULL;
}

/*
 * test_mover_memmove_multithreaded -- multi-threaded test for the mover's
 * memmove
 */
static int
test_mover_memmove_multithreaded(const struct test_case *tc, int argc,
	char *argv[])
{
	if (argc < 2)
		UT_FATAL(
		"usage: test_mover_memmove_multithreaded <file> <threads>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);
	unsigned long num_threads = STRTOUL(argv[1], NULL, 10);

	struct pmem2_source *src;
	struct pmem2_config *cfg;
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
		PMEM2_GRANULARITY_PAGE);

	struct pmem2_map *map = map_valid(cfg, src);

	UT_ASSERT(pmem2_map_get_size(map) >= TEST_SIZE * num_threads);

	os_thread_t *threads = MALLOC(num_threads * sizeof(*threads));
	struct thread_arg *args = MALLOC(num_threads * sizeof(*args));

	char *data = pmem2_map_get_address(map);
	for (unsigned i = 0; i < num_threads; i++) {
		args[i].map = map;
		args[i].addr = data + i * TEST_SIZE;
		args[i].threads = num_threads;
		args[i].thread_id = i;

		THREAD_CREATE(&threads[i], NULL, thread_memove_worker,
			&args[i]);
	}

	for (unsigned i = 0; i < num_threads; i++) {
		THREAD_JOIN(&threads[i], NULL);
	}

	FREE(threads);
	FREE(args);

	pmem2_map_delete(&map);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	CLOSE(fd);
	return 2;
}

/*
 * thread_memset_worker -- thread worker for test_mover_memset_multithread
 */
static void *
thread_memset_worker(void *arg)
{
	struct thread_arg *targ = arg;
	char *expected_result1, *expected_result2;

	UT_COMPILE_ERROR_ON(TEST_SIZE % sizeof(*expected_result1) != 0);
	unsigned array_size = TEST_SIZE / sizeof(*expected_result1);

	expected_result1 = MALLOC(array_size * sizeof(*expected_result1));
	expected_result2 = MALLOC(array_size * sizeof(*expected_result2));

	memset(expected_result1, (int)targ->thread_id, TEST_SIZE);
	memset(expected_result2, (int)(targ->thread_id + targ->threads),
		TEST_SIZE);

	for (int i = 0; i < WORKER_RUNS; i++) {
		int number = i % 2 ? (int)targ->thread_id
				: (int)(targ->thread_id + targ->threads);
		char *expected_result =
			i % 2 ? expected_result1 : expected_result2;
		struct pmem2_future set = pmem2_memset_async(
			targ->map, targ->addr, number, array_size, 0);

		FUTURE_BUSY_POLL(&set);

		if (memcmp(expected_result, targ->addr, TEST_SIZE))
			UT_FATAL("data should be equal");
	}
	FREE(expected_result1);
	FREE(expected_result2);
	return NULL;
}

/*
 * test_mover_memset_multithreaded -- multi-threaded test for the mover's memset
 */
static int
test_mover_memset_multithreaded(const struct test_case *tc, int argc,
	char *argv[])
{
	if (argc < 2)
		UT_FATAL(
		"usage: test_mover_memset_multithreaded <file> <threads>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);
	unsigned long num_threads = STRTOUL(argv[1], NULL, 10);

	struct pmem2_source *src;
	struct pmem2_config *cfg;
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
		PMEM2_GRANULARITY_PAGE);

	struct pmem2_map *map = map_valid(cfg, src);

	UT_ASSERT(pmem2_map_get_size(map) >= TEST_SIZE * num_threads);

	os_thread_t *threads = MALLOC(num_threads * sizeof(*threads));
	struct thread_arg *args = MALLOC(num_threads * sizeof(*args));

	char *data = pmem2_map_get_address(map);
	for (unsigned i = 0; i < num_threads; i++) {
		args[i].map = map;
		args[i].addr = data + i * TEST_SIZE;
		args[i].threads = num_threads;
		args[i].thread_id = i;

		THREAD_CREATE(&threads[i], NULL, thread_memset_worker,
			&args[i]);
	}

	for (unsigned i = 0; i < num_threads; i++) {
		THREAD_JOIN(&threads[i], NULL);
	}

	FREE(threads);
	FREE(args);

	pmem2_map_delete(&map);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	CLOSE(fd);
	return 2;
}

/*
 * test_miniasync_mover -- test memcpy async with miniasync mover
 */
static int
test_miniasync_mover(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_miniasync_mover <file>");

	char *file = argv[0];
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
	pmem2_memset_fn memset_fn = pmem2_get_memset_fn(map);
	memset_fn(data, 0xBA, 4096, 0);
	memset_fn(data + 4096, 0xAB, 4096, 0);
	struct pmem2_future cpy =
		pmem2_memcpy_async(map, data, data + 4096, 4096, 0);

	FUTURE_BUSY_POLL(&cpy);

	if (memcmp(data, data + 4096, 4096))
		UT_FATAL("data should be equal");

	pmem2_map_delete(&map);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	CLOSE(fd);
	return 1;
}

static void
memops_source_existing(char *file, size_t size, int is_pmem)
{
	struct pmem2_config *cfg;
	struct pmem2_source *src;
	struct pmem2_map *map;

	int fd = OPEN(file, O_RDWR);
	if (fd < 0)
		UT_FATAL("open: %s", file);

	void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
			0);
	if (!addr) {
		UT_FATAL("!mmap");
	}

	int ret = pmem2_source_from_existing(&src, addr, size, is_pmem);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	pmem2_config_new(&cfg);
	pmem2_config_set_offset(cfg, 0);
	pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE);

	ret = pmem2_map_new(&map, cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(map, NULL);

	char *map_addr = pmem2_map_get_address(map);
	UT_ASSERTeq(map_addr, addr);
	size_t map_size = pmem2_map_get_size(map);
	UT_ASSERTeq(map_size, size);

	/* test asynchronous memset */
	char *memset_cmp = malloc(4096);
	memset(memset_cmp, 0xAF, 4096);

	struct pmem2_future set =
			pmem2_memset_async(map, map_addr, 0xAF, 4096, 0);
	FUTURE_BUSY_POLL(&set);

	if (memcmp(memset_cmp, map_addr, 4096))
		UT_FATAL("data should be equal");

	/* test asynchronous memcpy */
	struct pmem2_future cpy =
		pmem2_memcpy_async(map, map_addr + 4096, map_addr, 4096, 0);
	FUTURE_BUSY_POLL(&cpy);

	if (memcmp(map_addr, map_addr + 4096, 4096))
		UT_FATAL("data should be equal");

	/* test asynchronous memmove */
	struct pmem2_future move =
		pmem2_memmove_async(map, map_addr + 8192, map_addr, 8192, 0);
	FUTURE_BUSY_POLL(&move);

	if (memcmp(map_addr, map_addr + 8192, 8192))
		UT_FATAL("data should be equal");

	ret = pmem2_map_delete(&map);
	UT_ASSERTeq(ret, 0);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	munmap(addr, size);
	CLOSE(fd);
}

/*
 * test_mover_memops_src_existing -- test functionality of pmem2 default
 *                                   mover with the source from existing mapping
 *
 */
static int
test_mover_memops_src_existing(const struct test_case *tc, int argc,
					char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_mover_memops_src_existing <file> "
				"<filesize>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);

	memops_source_existing(file, size, 0);

	return 2;
}

/*
 * test_mover_memops_src_existing_pmem -- test functionality of pmem2 default
 *                                        mover with the source on pmem from
 *                                        existing mapping
 */
static int
test_mover_memops_src_existing_pmem(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_mover_memops_src_existing_pmem <file> "
				"<filesize>");

	char *file = argv[0];
	size_t size = ATOUL(argv[1]);

	memops_source_existing(file, size, 1);

	return 2;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_mover_memcpy_basic),
	TEST_CASE(test_mover_memmove_basic),
	TEST_CASE(test_mover_memset_basic),
	TEST_CASE(test_mover_memcpy_multithreaded),
	TEST_CASE(test_mover_memmove_multithreaded),
	TEST_CASE(test_mover_memset_multithreaded),
	TEST_CASE(test_miniasync_mover),
	TEST_CASE(test_mover_memops_src_existing),
	TEST_CASE(test_mover_memops_src_existing_pmem),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_mover");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
