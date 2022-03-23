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
 * test_mover_basic -- test basic functionality of pmem2 default mover
 */
static int
test_mover_basic(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_mover_basic <file>");

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
	struct vdm_operation_future cpy =
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

#define WORKER_RUNS 20000
#define TEST_SIZE 4096
struct thread_arg {
	struct pmem2_map *map;
	void *addr;
	unsigned threads;
	unsigned thread_id;
};

/*
 * thread_worker -- thread worker for test_mover_multithread
 */
static void *
thread_worker(void *arg)
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
		struct vdm_operation_future cpy =
			pmem2_memcpy_async(targ->map, targ->addr, pattern,
				TEST_SIZE, 0);

		FUTURE_BUSY_POLL(&cpy);

		if (memcmp(pattern, targ->addr, TEST_SIZE))
			UT_FATAL("data should be equal");
	}
	FREE(pattern1);
	FREE(pattern2);
	return NULL;
}

/*
 * test_mover_multithreaded -- multi-threaded test for the memcpy
 */
static int
test_mover_multithreaded(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_mover_basic <file> <threads>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);
	unsigned long num_threads = STRTOUL(argv[1], NULL, 10);

	struct pmem2_source *src;
	struct pmem2_config *cfg;
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
		PMEM2_GRANULARITY_PAGE);

	struct pmem2_map *map = map_valid(cfg, src);

	UT_ASSERT(pmem2_map_get_size(map) >= TEST_SIZE * num_threads);

	os_thread_t *threads
		= MALLOC(num_threads * sizeof(*threads));
	struct thread_arg *args = MALLOC(num_threads * sizeof(*args));

	char *data = pmem2_map_get_address(map);
	for (unsigned i = 0; i < num_threads; i++) {
		args[i].map = map;
		args[i].addr = data + i * TEST_SIZE;
		args[i].threads = num_threads;
		args[i].thread_id = i;

		THREAD_CREATE(&threads[i], NULL, thread_worker,
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
	struct vdm_operation_future cpy =
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
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_mover_basic),
	TEST_CASE(test_mover_multithreaded),
	TEST_CASE(test_miniasync_mover)
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_mover");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
