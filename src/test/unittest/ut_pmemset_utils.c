// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * ut_pmemset_utils.c -- utility helper functions for libpmemset tests
 */

#include "unittest.h"
#include "ut_pmemset_utils.h"

/*
 * ut_pmemset_expect_return -- verifies error code and prints appropriate
 *                             error message in case of error
 */
void ut_pmemset_expect_return(const char *file, int line, const char *func,
		int value, int expected)
{
	if (value != expected) {
		ut_fatal(file, line, func,
			"unexpected return code (got: %d, expected: %d): %s",
			value, expected,
			(value == 0 ? "success" : pmemset_errormsg()));
	}

	if (expected) {
		const char *msg = pmemset_errormsg();
		if (!strlen(msg))
			ut_fatal(file, line, func,
				"expected return value is %d, so "
				"error message should not be empty!",
				expected);
	}
}

/*
 * ut_create_set_config -- create pmemset config with default
 * granularity value for test
 */
void ut_create_set_config(struct pmemset_config **cfg) {
	int ret = pmemset_config_new(cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);

	ret = pmemset_config_set_required_store_granularity(*cfg,
		PMEM2_GRANULARITY_PAGE);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);
}

/*
 * ut_setup_source -- set pmemset source using test args
 */
void ut_setup_source(struct pmemset_source **src,
		size_t offset, size_t length) {
	UT_ASSERTne(src, NULL);
	pmemset_source_set_offset(*src, offset);
	pmemset_source_set_length(*src, length);
}
