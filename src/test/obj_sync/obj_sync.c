/*
 * Copyright 2015-2019, Intel Corporation
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
 * obj_sync.c -- unit test for PMEM-resident locks
 */
#include "obj.h"
#include "sync.h"
#include "unittest.h"
#include "util.h"
#include "os.h"
#include "pmemcommon.h"

#define MAX_THREAD_NUM 200

#define DATA_SIZE 128

#define LOCKED_MUTEX 1
#define NANO_PER_ONE 1000000000LL
#define TIMEOUT (NANO_PER_ONE / 1000LL)
#define WORKER_RUNS 10
#define MAX_OPENS 5

#define FATAL_USAGE() UT_FATAL("usage: obj_sync [mrc] <num_threads> <runs>\n")

#define LOG_PREFIX "ut"
#define LOG_LEVEL_VAR "TEST_LOG_LEVEL"
#define LOG_FILE_VAR "TEST_LOG_FILE"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0


/* posix thread worker typedef */
typedef void *(*worker)(void *);

/* the mock pmemobj pool */
static PMEMobjpool Mock_pop;

/* the tested object containing persistent synchronization primitives */
static struct mock_obj {
	PMEMmutex mutex;
	PMEMmutex mutex_locked;
	PMEMcond cond;
	PMEMrwlock rwlock;
	int check_data;
	uint8_t data[DATA_SIZE];
} *Test_obj;

PMEMobjpool *
pmemobj_pool_by_ptr(const void *arg)
{
	return &Mock_pop;
}

/*
 * mock_open_pool -- (internal) simulate pool opening
 */
static void
mock_open_pool(PMEMobjpool *pop)
{
	util_fetch_and_add64(&pop->run_id, 2);
}

/*
 * mutex_write_worker -- (internal) write data with mutex
 */
static void *
mutex_write_worker(void *arg)
{
	for (unsigned run = 0; run < WORKER_RUNS; run++) {
		if (pmemobj_mutex_lock(&Mock_pop, &Test_obj->mutex)) {
			UT_ERR("pmemobj_mutex_lock");
			return NULL;
		}

		memset(Test_obj->data, (int)(uintptr_t)arg, DATA_SIZE);
		if (pmemobj_mutex_unlock(&Mock_pop, &Test_obj->mutex))
			UT_ERR("pmemobj_mutex_unlock");
	}

	return NULL;
}

/*
 * mutex_check_worker -- (internal) check consistency with mutex
 */
static void *
mutex_check_worker(void *arg)
{
	for (unsigned run = 0; run < WORKER_RUNS; run++) {
		if (pmemobj_mutex_lock(&Mock_pop, &Test_obj->mutex)) {
			UT_ERR("pmemobj_mutex_lock");
			return NULL;
		}
		uint8_t val = Test_obj->data[0];
		for (int i = 1; i < DATA_SIZE; i++)
			UT_ASSERTeq(Test_obj->data[i], val);

		memset(Test_obj->data, 0, DATA_SIZE);
		if (pmemobj_mutex_unlock(&Mock_pop, &Test_obj->mutex))
			UT_ERR("pmemobj_mutex_unlock");
	}

	return NULL;
}

/*
 * cond_write_worker -- (internal) write data with cond variable
 */
static void *
cond_write_worker(void *arg)
{
	for (unsigned run = 0; run < WORKER_RUNS; run++) {
		if (pmemobj_mutex_lock(&Mock_pop, &Test_obj->mutex))
			return NULL;

		memset(Test_obj->data, (int)(uintptr_t)arg, DATA_SIZE);
		Test_obj->check_data = 1;
		if (pmemobj_cond_signal(&Mock_pop, &Test_obj->cond))
			UT_ERR("pmemobj_cond_signal");
		pmemobj_mutex_unlock(&Mock_pop, &Test_obj->mutex);
	}

	return NULL;
}

/*
 * cond_check_worker -- (internal) check consistency with cond variable
 */
static void *
cond_check_worker(void *arg)
{
	for (unsigned run = 0; run < WORKER_RUNS; run++) {
		if (pmemobj_mutex_lock(&Mock_pop, &Test_obj->mutex))
			return NULL;

		while (Test_obj->check_data != 1) {
			if (pmemobj_cond_wait(&Mock_pop, &Test_obj->cond,
						&Test_obj->mutex))
				UT_ERR("pmemobj_cond_wait");
		}
		uint8_t val = Test_obj->data[0];
		for (int i = 1; i < DATA_SIZE; i++)
			UT_ASSERTeq(Test_obj->data[i], val);

		memset(Test_obj->data, 0, DATA_SIZE);
		pmemobj_mutex_unlock(&Mock_pop, &Test_obj->mutex);
	}

	return NULL;
}

/*
 * rwlock_write_worker -- (internal) write data with rwlock
 */
static void *
rwlock_write_worker(void *arg)
{
	for (unsigned run = 0; run < WORKER_RUNS; run++) {
		if (pmemobj_rwlock_wrlock(&Mock_pop, &Test_obj->rwlock)) {
			UT_ERR("pmemobj_rwlock_wrlock");
			return NULL;
		}

		memset(Test_obj->data, (int)(uintptr_t)arg, DATA_SIZE);
		if (pmemobj_rwlock_unlock(&Mock_pop, &Test_obj->rwlock))
			UT_ERR("pmemobj_rwlock_unlock");
	}

	return NULL;
}

/*
 * rwlock_check_worker -- (internal) check consistency with rwlock
 */
static void *
rwlock_check_worker(void *arg)
{
	for (unsigned run = 0; run < WORKER_RUNS; run++) {
		if (pmemobj_rwlock_rdlock(&Mock_pop, &Test_obj->rwlock)) {
			UT_ERR("pmemobj_rwlock_rdlock");
			return NULL;
		}
		uint8_t val = Test_obj->data[0];
		for (int i = 1; i < DATA_SIZE; i++)
			UT_ASSERTeq(Test_obj->data[i], val);

		if (pmemobj_rwlock_unlock(&Mock_pop, &Test_obj->rwlock))
			UT_ERR("pmemobj_rwlock_unlock");
	}

	return NULL;
}

/*
 * timed_write_worker -- (internal) intentionally doing nothing
 */
static void *
timed_write_worker(void *arg)
{
	return NULL;
}

/*
 * timed_check_worker -- (internal) check consistency with mutex
 */
static void *
timed_check_worker(void *arg)
{
	for (unsigned run = 0; run < WORKER_RUNS; run++) {

		int mutex_id = (int)(uintptr_t)arg % 2;
		PMEMmutex *mtx = mutex_id == LOCKED_MUTEX ?
				&Test_obj->mutex_locked : &Test_obj->mutex;

		struct timespec t1, t2, abs_time;
		os_clock_gettime(CLOCK_REALTIME, &t1);
		abs_time = t1;
		abs_time.tv_nsec += TIMEOUT;
		if (abs_time.tv_nsec >= NANO_PER_ONE) {
			abs_time.tv_sec++;
			abs_time.tv_nsec -= NANO_PER_ONE;
		}

		int ret = pmemobj_mutex_timedlock(&Mock_pop, mtx, &abs_time);

		os_clock_gettime(CLOCK_REALTIME, &t2);

		if (mutex_id == LOCKED_MUTEX) {
			UT_ASSERTeq(ret, ETIMEDOUT);

			uint64_t diff = (uint64_t)((t2.tv_sec - t1.tv_sec) *
				NANO_PER_ONE + t2.tv_nsec - t1.tv_nsec);

			UT_ASSERT(diff >= TIMEOUT);

			return NULL;
		}

		if (ret == 0) {
			UT_ASSERTne(mutex_id, LOCKED_MUTEX);
			pmemobj_mutex_unlock(&Mock_pop, mtx);
		} else if (ret == ETIMEDOUT) {
			uint64_t diff = (uint64_t)((t2.tv_sec - t1.tv_sec)
				* NANO_PER_ONE + t2.tv_nsec - t1.tv_nsec);

			UT_ASSERT(diff >= TIMEOUT);
		} else {
			errno = ret;
			UT_ERR("!pmemobj_mutex_timedlock");
		}
	}

	return NULL;
}

/*
 * cleanup -- (internal) clean up after each run
 */
static void
cleanup(char test_type)
{
	switch (test_type) {
		case 'm':
			os_mutex_destroy(&((PMEMmutex_internal *)
				&(Test_obj->mutex))->PMEMmutex_lock);
			break;
		case 'r':
			os_rwlock_destroy(&((PMEMrwlock_internal *)
				&(Test_obj->rwlock))->PMEMrwlock_lock);
			break;
		case 'c':
			os_mutex_destroy(&((PMEMmutex_internal *)
				&(Test_obj->mutex))->PMEMmutex_lock);
			os_cond_destroy(&((PMEMcond_internal *)
				&(Test_obj->cond))->PMEMcond_cond);
			break;
		case 't':
			os_mutex_destroy(&((PMEMmutex_internal *)
				&(Test_obj->mutex))->PMEMmutex_lock);
			os_mutex_destroy(&((PMEMmutex_internal *)
				&(Test_obj->mutex_locked))->PMEMmutex_lock);
			break;
		default:
			FATAL_USAGE();
	}

}

static int
obj_sync_persist(void *ctx, const void *ptr, size_t sz, unsigned flags)
{
	/* no-op */
	return 0;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_sync");
	common_init(LOG_PREFIX, LOG_LEVEL_VAR, LOG_FILE_VAR,
		MAJOR_VERSION, MINOR_VERSION);

	if (argc < 4)
		FATAL_USAGE();

	worker writer;
	worker checker;

	char test_type = argv[1][0];
	switch (test_type) {
		case 'm':
			writer = mutex_write_worker;
			checker = mutex_check_worker;
			break;
		case 'r':
			writer = rwlock_write_worker;
			checker = rwlock_check_worker;
			break;
		case 'c':
			writer = cond_write_worker;
			checker = cond_check_worker;
			break;
		case 't':
			writer = timed_write_worker;
			checker = timed_check_worker;
			break;
		default:
			FATAL_USAGE();

	}

	unsigned long num_threads = strtoul(argv[2], NULL, 10);
	if (num_threads > MAX_THREAD_NUM)
		UT_FATAL("Do not use more than %d threads.\n", MAX_THREAD_NUM);

	unsigned long opens = strtoul(argv[3], NULL, 10);
	if (opens > MAX_OPENS)
		UT_FATAL("Do not use more than %d runs.\n", MAX_OPENS);

	os_thread_t *write_threads
		= (os_thread_t *)MALLOC(num_threads * sizeof(os_thread_t));
	os_thread_t *check_threads
		= (os_thread_t *)MALLOC(num_threads * sizeof(os_thread_t));

	/* first pool open */
	mock_open_pool(&Mock_pop);
	Mock_pop.p_ops.persist = obj_sync_persist;
	Mock_pop.p_ops.base = &Mock_pop;
	Test_obj = (struct mock_obj *)MALLOC(sizeof(struct mock_obj));
	/* zero-initialize the test object */
	pmemobj_mutex_zero(&Mock_pop, &Test_obj->mutex);
	pmemobj_mutex_zero(&Mock_pop, &Test_obj->mutex_locked);
	pmemobj_cond_zero(&Mock_pop, &Test_obj->cond);
	pmemobj_rwlock_zero(&Mock_pop, &Test_obj->rwlock);
	Test_obj->check_data = 0;
	memset(&Test_obj->data, 0, DATA_SIZE);

	for (unsigned long run = 0; run < opens; run++) {
		if (test_type == 't') {
			pmemobj_mutex_lock(&Mock_pop,
					&Test_obj->mutex_locked);
		}

		for (unsigned i = 0; i < num_threads; i++) {
			PTHREAD_CREATE(&write_threads[i], NULL, writer,
				(void *)(uintptr_t)i);
			PTHREAD_CREATE(&check_threads[i], NULL, checker,
				(void *)(uintptr_t)i);
		}
		for (unsigned i = 0; i < num_threads; i++) {
			PTHREAD_JOIN(&write_threads[i], NULL);
			PTHREAD_JOIN(&check_threads[i], NULL);
		}

		if (test_type == 't') {
			pmemobj_mutex_unlock(&Mock_pop,
					&Test_obj->mutex_locked);
		}
		/* up the run_id counter and cleanup */
		mock_open_pool(&Mock_pop);
		cleanup(test_type);
	}

	FREE(check_threads);
	FREE(write_threads);
	FREE(Test_obj);
	DONE(NULL);
}
