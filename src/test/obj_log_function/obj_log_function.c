// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * core_log_default_function.c -- unit test for core_log_default_function
 */

#undef _GNU_SOURCE
#include <string.h>
#include <syslog.h>
#include <stdbool.h>

#include "unittest.h"
#include "log_internal.h"
#include "log_default.h"
#include "libpmemobj/log.h"

#define NO_ARGS_CONSUMED 0

#define VALIDATED_CALL 127
#define CALLED (VALIDATED_CALL + 1)

#define PMEMOBJ_LOG_CUSTOM_FUNCTION_MOCK ((pmemobj_log_function *) 0xA1C5D68F)

static struct {
	int ret;
} Core_log_set_function;

FUNC_MOCK(core_log_set_function, int, core_log_function *log_function)
	FUNC_MOCK_RUN(VALIDATED_CALL) {
		UT_ASSERT((void *)log_function ==
			(void *)PMEMOBJ_LOG_CUSTOM_FUNCTION_MOCK);
		return Core_log_set_function.ret;
	}
FUNC_MOCK_RUN_DEFAULT {
	return _FUNC_REAL(core_log_set_function)(log_function);
}
FUNC_MOCK_END

/*
 * Check:
 * - if core_log_set_function is called with proper argument
 * - if pmemobj_log_set_function return 0 (no error)
 * - no errno is set
 */
static int
test_set_log_function(const struct test_case *tc, int argc, char *argv[])
{
	errno = NO_ERRNO;
	Core_log_set_function.ret = 0;
	FUNC_MOCK_RCOUNTER_SET(core_log_set_function, VALIDATED_CALL);
	int ret = pmemobj_log_set_function(PMEMOBJ_LOG_CUSTOM_FUNCTION_MOCK);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(errno, NO_ERRNO);
	UT_ASSERTeq(RCOUNTER(core_log_set_function), CALLED);

	return NO_ARGS_CONSUMED;
}

/*
 * core_log_set_function() with EAGAIN error
 * Check:
 * - if core_log_set_function is called with proper argument
 * - if pmemobj_log_set_function return 1 (error via errno)
 * - errno is set to EAGAIN
 */
static int
test_set_log_function_EAGAIN(const struct test_case *tc, int argc, char *argv[])
{
	errno = NO_ERRNO;
	Core_log_set_function.ret = EAGAIN;
	FUNC_MOCK_RCOUNTER_SET(core_log_set_function, VALIDATED_CALL);
	int ret = pmemobj_log_set_function(PMEMOBJ_LOG_CUSTOM_FUNCTION_MOCK);
	UT_ASSERTeq(ret, 1);
	UT_ASSERTeq(errno, EAGAIN);
	UT_ASSERTeq(RCOUNTER(core_log_set_function), CALLED);

	return NO_ARGS_CONSUMED;
}

static struct test_case test_cases[] = {
	TEST_CASE(test_set_log_function),
	TEST_CASE(test_set_log_function_EAGAIN),
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_log_function");
	TEST_CASE_PROCESS(argc, argv, test_cases, ARRAY_SIZE(test_cases));
	DONE(NULL);
}
