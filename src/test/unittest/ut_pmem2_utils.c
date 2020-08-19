// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * ut_pmem2_utils.c -- utility helper functions for libpmem2 tests
 */

#include "unittest.h"
#include "ut_pmem2_utils.h"

/*
 * ut_pmem2_expect_return -- veryfies error code and prints appropriate
 *	error message in case of error
 */
void ut_pmem2_expect_return(const char *file, int line, const char *func,
	int value, int expected)
{
	if (value != expected) {
		ut_fatal(file, line, func,
			"unexpected return code (got %d, expected: %d): %s",
			value, expected,
			(value == 0 ? "success" : pmem2_errormsg()));
	}

	if (expected) {
		const char *msg = pmem2_errormsg();
		if (!strlen(msg))
			ut_fatal(file, line, func,
				"expected return value is %d, so "
				"error message should not be empty!",
				expected);
	}
}
