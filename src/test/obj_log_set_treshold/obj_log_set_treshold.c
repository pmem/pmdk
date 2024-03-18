// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * obj_log_set_treshold.c -- unit test for pmemobj_log_set_treshold
 */

#include "unittest.h"
#include "log_internal.h"
#include "libpmemobj/log.h"

#define NO_ARGS_CONSUMED 0

#define VALIDATED_CALL 127
#define CALLED (VALIDATED_CALL + 1)

static enum core_log_threshold core_tresholds[] = {
	CORE_LOG_THRESHOLD,
	CORE_LOG_THRESHOLD_AUX
};

static enum core_log_level core_levels[] = {
	CORE_LOG_LEVEL_HARK,
	CORE_LOG_LEVEL_FATAL,
	CORE_LOG_LEVEL_ERROR,
	CORE_LOG_LEVEL_WARNING,
	CORE_LOG_LEVEL_NOTICE,
	CORE_LOG_LEVEL_INFO,
	CORE_LOG_LEVEL_DEBUG
};

/* Mock */
static struct {
	enum core_log_threshold exp_threshold;
	enum core_log_level exp_level;
	int ret;
} Core_log_set_treshold;

FUNC_MOCK(core_log_set_threshold, int, enum core_log_threshold threshold,
	enum core_log_level level)
	FUNC_MOCK_RUN(VALIDATED_CALL) {
		UT_ASSERTeq(threshold, Core_log_set_treshold.exp_threshold);
		UT_ASSERTeq(level, Core_log_set_treshold.exp_level);
		return Core_log_set_treshold.ret;
	}
FUNC_MOCK_RUN_DEFAULT {
	return _FUNC_REAL(core_log_set_threshold)(threshold, level);
}
FUNC_MOCK_END

/* Helper */
static int
test_log_set_treshold_helper(int error)
{
	errno = 0;
	Core_log_set_treshold.ret = error == NO_ERRNO ? 0 : error;
	for (enum pmemobj_log_threshold treshold = PMEMOBJ_LOG_THRESHOLD;
		treshold <= PMEMOBJ_LOG_THRESHOLD_AUX; treshold++) {
		Core_log_set_treshold.exp_threshold = core_tresholds[treshold];
		for (enum pmemobj_log_level level = PMEMOBJ_LOG_LEVEL_HARK;
			level <= PMEMOBJ_LOG_LEVEL_DEBUG; level++) {
			Core_log_set_treshold.exp_level = core_levels[level];
			FUNC_MOCK_RCOUNTER_SET(core_log_set_threshold,
				VALIDATED_CALL);
			int ret = pmemobj_log_set_threshold(treshold, level);
			if (error == NO_ERRNO) {
				UT_ASSERTeq(ret, 0);
			} else {
				UT_ASSERTeq(ret, 1);
				UT_ASSERTeq(errno, error);
			}
			UT_ASSERTeq(RCOUNTER(core_log_set_threshold), CALLED);
			/* no need to test the error path for all values */
			if (error != NO_ERRNO)
				return NO_ARGS_CONSUMED;
		}
	}
	return NO_ARGS_CONSUMED;
}

/* Tests */
/*
 * Check:
 * - if core_log_set_treshold is called with proper arguments
 * - if pmemobj_log_set_treshold return 0 (no error)
 * - if each PMEMOBJ_LOG_LEVEL corespond to relevant CORE_LOG_LEVEL
 * - no errno is set
 */
static int
test_log_set_treshold(const struct test_case *tc, int argc, char *argv[])
{
	return test_log_set_treshold_helper(NO_ERRNO);
}

/* Check pmemobj_log_set_threshold EAGAIN error handling. */
static int
test_log_set_treshold_EAGAIN(const struct test_case *tc, int argc, char *argv[])
{
	return test_log_set_treshold_helper(EAGAIN);
}

/* Check pmemobj_log_set_threshold EINVAL error handling. */
static int
test_log_set_treshold_EINVAL(const struct test_case *tc, int argc, char *argv[])
{
	return test_log_set_treshold_helper(EINVAL);
}

static struct test_case test_cases[] = {
	TEST_CASE(test_log_set_treshold),
	TEST_CASE(test_log_set_treshold_EAGAIN),
	TEST_CASE(test_log_set_treshold_EINVAL),
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_log_set_treshold");
	TEST_CASE_PROCESS(argc, argv, test_cases, ARRAY_SIZE(test_cases));
	DONE(NULL);
}
