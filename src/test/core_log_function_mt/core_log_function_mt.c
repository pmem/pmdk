// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * core_log_function_mt.c -- unit test for core_log_set_function() and
 * core_log() since both of them may write/read the log function pointer in
 * parallel.
 */

#include "unittest.h"
#include "log_internal.h"

#define NO_ARGS_CONSUMED 0

#define THREADS_IN_GROUP 10
#define THREADS_SET_MIN 0
#define THREADS_SET_MAX (THREADS_SET_MIN + THREADS_IN_GROUP)
#define THREADS_CALL_MIN THREADS_SET_MAX
#define THREADS_CALL_MAX (THREADS_CALL_MIN + THREADS_IN_GROUP)
#define TOTAL_THREADS THREADS_CALL_MAX

#define OP_REDO 4096

#define LOG_FUNC(FUNC_NAME) \
static void \
FUNC_NAME(enum core_log_level level, const char *file_name, unsigned line_no, \
	const char *function_name, const char *message) \
{ \
	SUPPRESS_UNUSED(level, file_name, line_no, function_name, message); \
}

LOG_FUNC(log_func0)
LOG_FUNC(log_func1)
LOG_FUNC(log_func2)
LOG_FUNC(log_func3)
LOG_FUNC(log_func4)
LOG_FUNC(log_func5)
LOG_FUNC(log_func6)
LOG_FUNC(log_func7)
LOG_FUNC(log_func8)
LOG_FUNC(log_func9)

static core_log_function *log_funcs[] = {
	log_func0,
	log_func1,
	log_func2,
	log_func3,
	log_func4,
	log_func5,
	log_func6,
	log_func7,
	log_func8,
	log_func9,
};

#define N_LOG_FUNCS ARRAY_SIZE(log_funcs)

static os_mutex_t mutex;
static os_cond_t cond;
static unsigned threads_waiting;

static void *
helper_set(void *arg)
{
	uint64_t idx = (uint64_t)arg;
	os_mutex_lock(&mutex);
	++threads_waiting;
	os_cond_wait(&cond, &mutex);
	os_mutex_unlock(&mutex);
	for (uint64_t i = 0; i < OP_REDO; ++i) {
		core_log_function *log_func =
			log_funcs[(i * (idx + 1)) % N_LOG_FUNCS];
		int ret = core_log_set_function(log_func);
		UT_ASSERT(ret == 0 || ret == EAGAIN);
		if (ret == EAGAIN) {
			UT_OUT("ret == EAGAIN"); /* just out of curiosity */
		}
	}
	return NULL;
}

static void *
helper_call(void *arg)
{
	SUPPRESS_UNUSED(arg);

	os_mutex_lock(&mutex);
	++threads_waiting;
	os_cond_wait(&cond, &mutex);
	os_mutex_unlock(&mutex);
	for (uint64_t i = 0; i < OP_REDO; ++i) {
		core_log(CORE_LOG_LEVEL_ERROR, NO_ERRNO, "", 0, "", "");
	}
	return NULL;
}

/* tests */

/* Run core_log_set_function() and core_log() in parallel. */
static int
test_function_set_call(const struct test_case *tc, int argc, char *argv[])
{
	os_thread_t threads[TOTAL_THREADS];

	os_mutex_init(&mutex);
	os_cond_init(&cond);
	threads_waiting = 0;

	/* core_log_set_function() threads */
	for (uint64_t idx = THREADS_SET_MIN; idx < THREADS_SET_MAX; idx++) {
		THREAD_CREATE(&threads[idx], 0, helper_set, (void *)idx);
	}

	/* core_log() threads */
	for (uint64_t idx = THREADS_CALL_MIN; idx < THREADS_CALL_MAX; idx++) {
		THREAD_CREATE(&threads[idx], 0, helper_call, NULL);
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

	for (uint64_t idx = 0; idx < TOTAL_THREADS; idx++) {
		void *retval;
		THREAD_JOIN(&threads[idx], &retval);
	}

	os_cond_destroy(&cond);
	os_mutex_destroy(&mutex);
	return NO_ARGS_CONSUMED;
}

/*
 * A Valgrind tool external to the test binary is assumed to monitor
 * the execution and assess synchronisation correctness.
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_function_set_call),
};

#define NTESTS ARRAY_SIZE(test_cases)

int
main(int argc, char *argv[])
{
	START(argc, argv, "core_log_function_mt");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
