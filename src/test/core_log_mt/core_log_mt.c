// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * core_log_internal.c -- unit test to CORE_LOG_...
 */

#include "unittest.h"
#include "log_internal.h"

#define NO_ARGS_CONSUMED 0

#define THREADS_IN_GROUP 10
#define TOTAL_THREADS (THREADS_IN_GROUP * 2)
#define OP_REDO 4096

struct test_threshold_helper_ctx {
	enum core_log_threshold threshold;
	enum core_log_level level;
} threshold_helper_ [TOTAL_THREADS];

static void *
test_threshold_helper_set(void *arg)
{
	struct test_threshold_helper_ctx *ctx =
		(struct test_threshold_helper_ctx *)arg;
	for (int i = 0; i < OP_REDO; ++i) {
		core_log_set_threshold(ctx->threshold, ctx->level);
	}
	return NULL;
}

static void *
test_threshold_helper_get(void *arg)
{
	struct test_threshold_helper_ctx *ctx =
		(struct test_threshold_helper_ctx *)arg;
	for (int i = 0; i < OP_REDO; ++i) {
		core_log_get_threshold(ctx->threshold, &ctx->level);
	}
	(void) ctx->level;
	return NULL;
}

static void
test_threshold_helper(enum core_log_threshold threshold)
{
	os_thread_t threads[TOTAL_THREADS];

	/* core_log_set_threshold() threads */
	for (int idx = 0; idx < THREADS_IN_GROUP; idx++) {
		threshold_helper_[idx].threshold = threshold;
		threshold_helper_[idx].level =
			(enum core_log_level)(idx % CORE_LOG_LEVEL_MAX);
		THREAD_CREATE(&threads[idx], 0, test_threshold_helper_set,
			(void *)&threshold_helper_[idx]);
	}

	/* core_log_get_threshold() threads */
	for (int idx = THREADS_IN_GROUP; idx < TOTAL_THREADS; idx++) {
		threshold_helper_[idx].threshold = threshold;
		THREAD_CREATE(&threads[idx], 0, test_threshold_helper_get,
			(void *)&threshold_helper_[idx]);
	}

	for (int idx = 0; idx < TOTAL_THREADS; idx++) {
		void *retval;
		THREAD_JOIN(&threads[idx], &retval);
	}
}

/* Run core_log_set/get_threshold(CORE_LOG_THRESHOLD, ...) in parallel. */
static int
test_threshold(const struct test_case *tc, int argc, char *argv[])
{
	test_threshold_helper(CORE_LOG_THRESHOLD);
	return NO_ARGS_CONSUMED;
}

/* Run core_log_set/get_threshold(CORE_LOG_THRESHOLD_AUX, ...) in parallel. */
static int
test_threshold_aux(const struct test_case *tc, int argc, char *argv[])
{
	test_threshold_helper(CORE_LOG_THRESHOLD_AUX);
	return NO_ARGS_CONSUMED;
}

/*
 * A Valgrind tool external to the test binary is assumed to monitor
 * the execution and assess synchronisation correctness.
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_threshold),
	TEST_CASE(test_threshold_aux),
};

#define NTESTS ARRAY_SIZE(test_cases)

int
main(int argc, char *argv[])
{
	START(argc, argv, "core_log_mt");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
