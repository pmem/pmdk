/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020-2021, Intel Corporation */

/*
 * ut_pmemset_utils.h -- utility helper functions for libpmemset tests
 */

#ifndef UT_PMEMSET_UTILS_H
#define UT_PMEMSET_UTILS_H 1

/* veryfies error code and prints appropriate error message in case of error */
#define UT_PMEMSET_EXPECT_RETURN(value, expected)		        \
	ut_pmemset_expect_return(__FILE__, __LINE__, __func__,		\
		value, expected)

void ut_pmemset_expect_return(const char *file, int line, const char *func,
		int value, int expected);

#endif /* UT_PMEMSET_UTILS_H */

void ut_create_set_config(struct pmemset_config **cfg);

void ut_setup_source(struct pmemset_source **src,
		size_t offset, size_t length);
