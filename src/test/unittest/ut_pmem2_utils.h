/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2019-2020, Intel Corporation */

/*
 * ut_pmem2_utils.h -- utility helper functions for libpmem2 tests
 */

#ifndef UT_PMEM2_UTILS_H
#define UT_PMEM2_UTILS_H 1

/* veryfies error code and prints appropriate error message in case of error */
#define UT_PMEM2_EXPECT_RETURN(value, expected)				\
	ut_pmem2_expect_return(__FILE__, __LINE__, __func__,		\
		value, expected)

void ut_pmem2_expect_return(const char *file, int line, const char *func,
	int value, int expected);

#endif /* UT_PMEM2_UTILS_H */
