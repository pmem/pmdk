// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * pmem_log_set_function.c -- unit test for pmem_log_set_function
 */

#include "unittest.h"
#include "log_internal.h"
#include "libpmem.h"

#define NO_ARGS_CONSUMED 0

#define VALIDATED_CALL 127
#define CALLED (VALIDATED_CALL + 1)

#define PMEM_LOG_CUSTOM_FUNCTION_MOCK ((pmem_log_function *) 0xA1C5D68F)

/* Mock */
static struct {
	int ret;
} Core_log_set_function;

FUNC_MOCK(core_log_set_function, int, core_log_function *log_function)
	FUNC_MOCK_RUN(VALIDATED_CALL) {
		UT_ASSERTeq((void *)log_function,
			(void *)PMEM_LOG_CUSTOM_FUNCTION_MOCK);
		return Core_log_set_function.ret;
	}
FUNC_MOCK_RUN_DEFAULT {
	return _FUNC_REAL(core_log_set_function)(log_function);
}
FUNC_MOCK_END

/* Helper */
static int
test_log_set_function_helper(int error)
{
	errno = 0;
	Core_log_set_function.ret = error == NO_ERRNO ? 0 : error;
	FUNC_MOCK_RCOUNTER_SET(core_log_set_function, VALIDATED_CALL);
	int ret = pmem_log_set_function(PMEM_LOG_CUSTOM_FUNCTION_MOCK);
	if (error == NO_ERRNO) {
		UT_ASSERTeq(ret, 0);
	} else {
		UT_ASSERTeq(ret, 1);
		UT_ASSERTeq(errno, error);
	}
	UT_ASSERTeq(RCOUNTER(core_log_set_function), CALLED);

	return NO_ARGS_CONSUMED;
}

/* Tests */
/*
 * Check:
 * - if core_log_set_function is called with proper argument
 * - if pmem_log_set_function return 0 (no error)
 * - no errno is set
 */
static int
test_log_set_function(const struct test_case *tc, int argc, char *argv[])
{
	return test_log_set_function_helper(NO_ERRNO);
}

/*
 * core_log_set_function() with EAGAIN error
 * Check:
 * - if core_log_set_function is called with proper argument
 * - if pmem_log_set_function return 1 (error via errno)
 * - errno is set to EAGAIN
 */
static int
test_log_set_function_EAGAIN(const struct test_case *tc, int argc, char *argv[])
{
	return test_log_set_function_helper(EAGAIN);
}

static struct test_case test_cases[] = {
	TEST_CASE(test_log_set_function),
	TEST_CASE(test_log_set_function_EAGAIN),
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem_log_set_function");
	TEST_CASE_PROCESS(argc, argv, test_cases, ARRAY_SIZE(test_cases));
	DONE(NULL);
}
