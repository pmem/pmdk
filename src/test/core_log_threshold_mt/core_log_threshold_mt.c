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
#define THREADS_LOG_MIN THREADS_GET_MAX
#define THREADS_LOG_MAX (THREADS_LOG_MIN + THREADS_IN_GROUP)
#define TOTAL_THREADS THREADS_LOG_MAX

#define OP_REDO 4096

/* Prevent abort() in CORE_LOG_FATAL() from killing the process. */
FUNC_MOCK(abort, void, void)
FUNC_MOCK_RUN_DEFAULT {
	/* NOP */
}
FUNC_MOCK_END

struct helper_ctx {
	enum core_log_threshold threshold;
	enum core_log_level level;
} helper_ctx_ [TOTAL_THREADS];

static void *
helper_set(void *arg)
{
	struct helper_ctx *ctx = (struct helper_ctx *)arg;
	for (int i = 0; i < OP_REDO; ++i) {
		int ret = core_log_set_threshold(ctx->threshold, ctx->level);
		UT_ASSERTeq(ret, 0);
	}
	return NULL;
}

static void *
helper_get(void *arg)
{
	struct helper_ctx *ctx = (struct helper_ctx *)arg;
	for (int i = 0; i < OP_REDO; ++i) {
		int ret = core_log_get_threshold(ctx->threshold, &ctx->level);
		UT_ASSERTeq(ret, 0);
	}
	return NULL;
}

static void *
helper_CORE_LOG_X(void *arg)
{
	SUPPRESS_UNUSED(arg);
	for (int i = 0; i < OP_REDO; ++i) {
		CORE_LOG_HARK("");
		CORE_LOG_FATAL("");
		CORE_LOG_ERROR("");
		CORE_LOG_WARNING("");
		CORE_LOG_NOTICE("");
		CORE_LOG_INFO("");
		CORE_LOG_DEBUG("");
	}
	return NULL;
}

static void
helper(enum core_log_threshold threshold)
{
	os_thread_t threads[TOTAL_THREADS];

	/* core_log_set_threshold() threads */
	for (int idx = THREADS_SET_MIN; idx < THREADS_SET_MAX; idx++) {
		helper_ctx_[idx].threshold = threshold;
		helper_ctx_[idx].level =
			(enum core_log_level)(idx % CORE_LOG_LEVEL_MAX);
		THREAD_CREATE(&threads[idx], 0, helper_set,
			(void *)&helper_ctx_[idx]);
	}

	/* core_log_get_threshold() threads */
	for (int idx = THREADS_GET_MIN; idx < THREADS_GET_MAX; idx++) {
		helper_ctx_[idx].threshold = threshold;
		THREAD_CREATE(&threads[idx], 0, helper_get,
			(void *)&helper_ctx_[idx]);
	}

	/* CORE_LOG_*() threads */
	for (int idx = THREADS_LOG_MIN; idx < THREADS_LOG_MAX; idx++) {
		THREAD_CREATE(&threads[idx], 0, helper_CORE_LOG_X, NULL);
	}

	for (int idx = 0; idx < TOTAL_THREADS; idx++) {
		void *retval;
		THREAD_JOIN(&threads[idx], &retval);
	}
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
