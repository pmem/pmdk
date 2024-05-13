// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * core_log_threshold_mt.c -- unit test for core_log_set/get_threshold() and
 * CORE_LOG_X() since all of them may write/read thresholds in parallel.
 */

#include "unittest.h"
#include "log_internal.h"

#define NO_ARGS_CONSUMED 0

#define THREADS_IN_GROUP 10
#define THREADS_SET_MIN 0
#define THREADS_SET_MAX (THREADS_SET_MIN + THREADS_IN_GROUP)
#define THREADS_GET_MIN THREADS_SET_MAX
#define THREADS_GET_MAX (THREADS_GET_MIN + THREADS_IN_GROUP)
#define TOTAL_THREADS THREADS_GET_MAX

#define OP_REDO 4096

static struct helper_ctx {
	enum core_log_threshold threshold;
	int idx;
} helper_ctx_ [TOTAL_THREADS];

static os_mutex_t mutex;
static os_cond_t cond;
static unsigned threads_waiting;

static void *
helper_set(void *arg)
{
	struct helper_ctx *ctx = (struct helper_ctx *)arg;
	os_mutex_lock(&mutex);
	++threads_waiting;
	os_cond_wait(&cond, &mutex);
	os_mutex_unlock(&mutex);
	for (int i = 0; i < OP_REDO; ++i) {
		/*
		 * a simple attempt to generate a different sequence of values
		 * for each of the threads
		 */
		enum core_log_level level =
			(enum core_log_level)((i * (ctx->idx + 1)) %
			CORE_LOG_LEVEL_MAX);
		int ret = core_log_set_threshold(ctx->threshold, level);
		UT_ASSERT(ret == 0 || ret == EAGAIN);
		if (ret == EAGAIN) {
			UT_OUT("ret == EAGAIN"); /* just out of curiosity */
		}
	}
	return NULL;
}

static void *
helper_get(void *arg)
{
	struct helper_ctx *ctx = (struct helper_ctx *)arg;
	os_mutex_lock(&mutex);
	++threads_waiting;
	os_cond_wait(&cond, &mutex);
	os_mutex_unlock(&mutex);
	for (int i = 0; i < OP_REDO; ++i) {
		enum core_log_level level;
		int ret = core_log_get_threshold(ctx->threshold, &level);
		UT_ASSERTeq(ret, 0);
		level = _core_log_get_threshold_internal();
	}
	return NULL;
}

static void
helper(enum core_log_threshold threshold)
{
	os_thread_t threads[TOTAL_THREADS];

	os_mutex_init(&mutex);
	os_cond_init(&cond);
	threads_waiting = 0;

	/* core_log_set_threshold() threads */
	for (int idx = THREADS_SET_MIN; idx < THREADS_SET_MAX; idx++) {
		helper_ctx_[idx].threshold = threshold;
		helper_ctx_[idx].idx = idx;
		THREAD_CREATE(&threads[idx], 0, helper_set,
			(void *)&helper_ctx_[idx]);
	}

	/* core_log_get_threshold/_core_log_get_threshold_internal() threads */
	for (int idx = THREADS_GET_MIN; idx < THREADS_GET_MAX; idx++) {
		helper_ctx_[idx].threshold = threshold;
		THREAD_CREATE(&threads[idx], 0, helper_get,
			(void *)&helper_ctx_[idx]);
	}

	do {
		os_mutex_lock(&mutex);
		if (threads_waiting == TOTAL_THREADS) {
			os_cond_broadcast(&cond);
			os_mutex_unlock(&mutex);
			break;
		}
		os_mutex_unlock(&mutex);
	} while (1);

	for (int idx = 0; idx < TOTAL_THREADS; idx++) {
		void *retval;
		THREAD_JOIN(&threads[idx], &retval);
	}

	os_cond_destroy(&cond);
	os_mutex_destroy(&mutex);
}

/* tests */

/*
 * Run core_log_set/get_threshold(CORE_LOG_THRESHOLD, ...) and CORE_LOG_X()
 * in parallel.
 */
static int
test_threshold_set_get(const struct test_case *tc, int argc, char *argv[])
{
	helper(CORE_LOG_THRESHOLD);
	return NO_ARGS_CONSUMED;
}

/*
 * Run core_log_set/get_threshold(CORE_LOG_THRESHOLD_AUX, ...) and CORE_LOG_X()
 * in parallel.
 */
static int
test_threshold_aux_set_get(const struct test_case *tc, int argc, char *argv[])
{
	helper(CORE_LOG_THRESHOLD_AUX);
	return NO_ARGS_CONSUMED;
}

/*
 * A Valgrind tool external to the test binary is assumed to monitor
 * the execution and assess synchronisation correctness.
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_threshold_set_get),
	TEST_CASE(test_threshold_aux_set_get),
};

#define NTESTS ARRAY_SIZE(test_cases)

int
main(int argc, char *argv[])
{
	START(argc, argv, "core_log_threshold_mt");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
