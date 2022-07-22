// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/*
 * blk_future.c -- pmemblk future tests
 */

#define PMEMBLK_USE_MINIASYNC 1

#include "libpmemblk.h"
#include "unittest.h"

#include <libminiasync.h>

enum get_pool {
	GET_POOL_CREATE,
	GET_POOL_OPEN,
};

static PMEMblkpool *
get_pool(char *path, size_t block_size, size_t pool_size, enum get_pool get,
		struct vdm *vdm)
{
	PMEMblkpool *pbp = NULL;
	switch (get) {
		case GET_POOL_CREATE:
			pbp = pmemblk_xcreate(path, block_size, pool_size,
					S_IWUSR | S_IRUSR, vdm);
			if (pbp == NULL)
				UT_FATAL("!%s: pmemblk_create", path);
			break;
		case GET_POOL_OPEN:
			pbp = pmemblk_xopen(path, block_size, vdm);
			if (pbp == NULL)
				UT_FATAL("!%s: pmemblk_open", path);
			break;
		default:
			UT_FATAL("wrong get pool value: %c", get);
	}

	return pbp;
}

/*
 * pool_async_write -- performs an asychronous write to the pmemblk
 */
static int
pool_async_write(struct pmemblk *pbp, void *buf, os_off_t lba)
{
	struct pmemblk_write_async_future write =
			pmemblk_write_async(pbp, buf, lba);

	FUTURE_BUSY_POLL(&write);
	UT_ASSERTeq(FUTURE_STATE(&write), FUTURE_STATE_COMPLETE);

	return FUTURE_OUTPUT(&write)->return_value;
}

/*
 * pool_async_read -- performs an asychronous read from the pmemblk
 */
static int
pool_async_read(struct pmemblk *pbp, void *buf, os_off_t lba)
{
	struct pmemblk_read_async_future read =
			pmemblk_read_async(pbp, buf, lba);

	FUTURE_BUSY_POLL(&read);
	UT_ASSERTeq(FUTURE_STATE(&read), FUTURE_STATE_COMPLETE);

	return FUTURE_OUTPUT(&read)->return_value;
}

/*
 * pool_assert_block -- confirm the block value is equal to the provided buffer
 */
static void
pool_assert_block(struct pmemblk *pbp, os_off_t lba, void *buf,
		size_t block_size)
{
	void *block = MALLOC(block_size);

	/* synchronously read the block */
	int ret = pmemblk_read(pbp, block, lba);
	UT_ASSERTeq(ret, 0);

	UT_ASSERTeq(memcmp(block, buf, block_size), 0);

	free(block);
}

/*
 * pool_set_block -- sets the block to the value equal to the provided buffer
 */
static void
pool_set_block(struct pmemblk *pbp, os_off_t lba, void *buf, size_t block_size)
{
	/* synchronously write to the block */
	pmemblk_write(pbp, buf, lba);

	pool_assert_block(pbp, lba, buf, block_size);
}

/*
 * test_write_async_basic -- test pmemblk async write
 */
static int
test_write_async_basic(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 3)
		UT_FATAL("usage: test_write_async_basic <path> <block_size> "
				"<lba>");

	char *path = argv[0];
	size_t block_size = strtoul(argv[1], NULL, 0);
	os_off_t lba = strtol(argv[2], NULL, 0);

	/* setup a miniasync synchronous data mover */
	struct data_mover_sync *dms = data_mover_sync_new();
	UT_ASSERTne(dms, NULL);
	struct vdm *vdm = data_mover_sync_get_vdm(dms);
	UT_ASSERTne(vdm, NULL);

	/* create a new pool */
	PMEMblkpool *pbp = get_pool(path, block_size, 0, GET_POOL_CREATE, vdm);

	unsigned char *buf = MALLOC(block_size);
	memset(buf, 9, block_size);

	/* asynchronously set pmemblk memory at LBA 'lba' to '9's */
	int ret = pool_async_write(pbp, buf, lba);
	UT_ASSERTeq(ret, 0);

	/* confirm that the block contains '9's */
	pool_assert_block(pbp, lba, buf, block_size);

	/* cleanup */
	FREE(buf);
	pmemblk_close(pbp);

	return 3;
}

/*
 * test_read_async_basic -- test pmemblk async read
 */
static int
test_read_async_basic(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 3)
		UT_FATAL("usage: test_read_async_basic <path> <block_size> "
				"<lba>");

	char *path = argv[0];
	size_t block_size = strtoul(argv[1], NULL, 0);
	os_off_t lba = strtol(argv[2], NULL, 0);

	/* setup a miniasync synchronous data mover */
	struct data_mover_sync *dms = data_mover_sync_new();
	UT_ASSERTne(dms, NULL);
	struct vdm *vdm = data_mover_sync_get_vdm(dms);
	UT_ASSERTne(vdm, NULL);

	/* create a new pool */
	PMEMblkpool *pbp = get_pool(path, block_size, 0, GET_POOL_CREATE, vdm);

	/* set pmemblk memory at LBA 'lba' to '9's */
	unsigned char *buf = MALLOC(block_size);
	memset(buf, 9, block_size);
	pool_set_block(pbp, lba, buf, block_size);

	/* asynchronously read the same block */
	int ret = pool_async_read(pbp, buf, lba);
	UT_ASSERTeq(ret, 0);

	/* confirm that this block contains '9's */
	pool_assert_block(pbp, lba, buf, block_size);

	/* cleanup */
	FREE(buf);
	pmemblk_close(pbp);

	return 3;
}

#define MAX_THREADS 32
#define N_ITERATIONS 64

struct poll_worker_args {
	PMEMblkpool *pbp;
	os_off_t lba;
	unsigned char *buf;
};

static void *
async_write_worker(void *arg)
{
	struct poll_worker_args *args = (struct poll_worker_args *)arg;
	PMEMblkpool *pbp = args->pbp;
	os_off_t lba = args->lba;
	unsigned char *buf = args->buf;

	struct pmemblk_write_async_future write =
			pmemblk_write_async(pbp, buf, lba);
	FUTURE_BUSY_POLL(&write);

	UT_ASSERTeq(FUTURE_OUTPUT(&write)->return_value, 0);

	return NULL;
}

static void
run_worker(void *(worker_func)(void *arg), struct poll_worker_args args[],
		size_t nthreads)
{
	os_thread_t threads[MAX_THREADS];

	for (size_t i = 0; i < nthreads; i++)
		THREAD_CREATE(&threads[i], NULL, worker_func, &args[i]);

	for (size_t i = 0; i < nthreads; i++)
		THREAD_JOIN(&threads[i], NULL);
}

/*
 * test_async_write_mt -- test pmemblk async write in multithreaded environment
 */
static int
test_async_write_mt(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 3)
		UT_FATAL("usage: test_async_write_mt <path> <block_size> "
				"<nthreads>");

	char *path = argv[0];
	size_t block_size = strtoul(argv[1], NULL, 0);
	size_t nthreads = strtoul(argv[2], NULL, 0);

	/* setup a miniasync synchronous data mover */
	struct data_mover_threads *dmt = data_mover_threads_new(nthreads, 128,
			FUTURE_NOTIFIER_NONE);
	UT_ASSERTne(dmt, NULL);
	struct vdm *vdm = data_mover_threads_get_vdm(dmt);
	UT_ASSERTne(vdm, NULL);

	/* create a new pool */
	PMEMblkpool *pbp = get_pool(path, block_size, 0, GET_POOL_CREATE, vdm);

	unsigned char **buf = MALLOC(sizeof(unsigned char *) * nthreads);
	struct poll_worker_args args[MAX_THREADS];
	/* prepare the arguments and buffers */
	for (int i = 0; i < nthreads; i++) {
		buf[i] = MALLOC(block_size);
		/* set the buffer to the 'lba' value */
		memset(buf[i], i + 1, block_size);

		args[i].pbp = pbp;
		args[i].lba = i;
		args[i].buf = buf[i];
	}

	for (int i = 0; i < N_ITERATIONS; i++) {
		/* poll async write futures from multiple threads */
		run_worker(async_write_worker, args, nthreads);

		/* confirm the writes and zero used blocks */
		for (int j = 0; j < nthreads; j++) {
			pool_assert_block(pbp, j, buf[j], block_size);
			pmemblk_set_zero(pbp, j);
		}
	}

	/* cleanup */
	for (int i = 0; i < nthreads; i++) {
		FREE(buf[i]);
	}

	pmemblk_close(pbp);

	return 3;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_write_async_basic),
	TEST_CASE(test_read_async_basic),
	TEST_CASE(test_async_write_mt),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "blk_future");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
