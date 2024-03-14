// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * obj_log_get_treshold.c -- unit test for pmemobj_log_get_treshold
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
	enum core_log_level level;
	int ret;
} Core_log_get_treshold;

FUNC_MOCK(core_log_get_threshold, int, enum core_log_threshold threshold,
	enum core_log_level *level)
	FUNC_MOCK_RUN(VALIDATED_CALL) {
		UT_ASSERTeq(threshold, Core_log_get_treshold.exp_threshold);
		if (Core_log_get_treshold.ret == 0)
			*level = Core_log_get_treshold.level;
		return Core_log_get_treshold.ret;
	}
FUNC_MOCK_RUN_DEFAULT {
	return _FUNC_REAL(core_log_get_threshold)(threshold, level);
}
FUNC_MOCK_END

/* Helper */
static int
test_log_get_treshold_helper(int error)
{
	errno = 0;
	Core_log_get_treshold.ret = error == NO_ERRNO ? 0 : error;
	for (enum pmemobj_log_threshold treshold = PMEMOBJ_LOG_THRESHOLD;
		treshold <= PMEMOBJ_LOG_THRESHOLD_AUX; treshold++) {
		Core_log_get_treshold.exp_threshold = core_tresholds[treshold];
		for (enum pmemobj_log_level exp_level = PMEMOBJ_LOG_LEVEL_HARK;
			exp_level <= PMEMOBJ_LOG_LEVEL_DEBUG; exp_level++) {
			enum pmemobj_log_level level;
			Core_log_get_treshold.level = core_levels[exp_level];
			FUNC_MOCK_RCOUNTER_SET(core_log_get_threshold,
				VALIDATED_CALL);
			int ret = pmemobj_log_get_threshold(treshold, &level);
			if (error == NO_ERRNO) {
				UT_ASSERTeq(ret, 0);
				UT_ASSERTeq(level, exp_level);
			} else {
				UT_ASSERTeq(ret, 1);
				UT_ASSERTeq(errno, error);
			}
			UT_ASSERTeq(RCOUNTER(core_log_get_threshold), CALLED);
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
 * - if core_log_get_treshold is called with proper arguments
 * - if pmemobj_log_get_treshold return 0 (no error)
 * - if each PMEMOBJ_LOG_LEVEL corespond to relevant CORE_LOG_LEVEL
 * - no errno is set
 */
static int
test_log_get_treshold(const struct test_case *tc, int argc, char *argv[])
{
	return test_log_get_treshold_helper(NO_ERRNO);
}

/* Check pmemobj_log_get_threshold EAGAIN error handling. */
static int
test_log_get_treshold_EAGAIN(const struct test_case *tc, int argc, char *argv[])
{
	return test_log_get_treshold_helper(EAGAIN);
}

static struct test_case test_cases[] = {
	TEST_CASE(test_log_get_treshold),
	TEST_CASE(test_log_get_treshold_EAGAIN),
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_log_get_treshold");
	TEST_CASE_PROCESS(argc, argv, test_cases, ARRAY_SIZE(test_cases));
	DONE(NULL);
}
