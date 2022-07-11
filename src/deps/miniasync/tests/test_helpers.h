/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2022, Intel Corporation */

/*
 * test_helpers.h -- header with helpers
 */

#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H 1

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include "libminiasync/vdm.h"

#define UT_ERR(...) do {\
	fprintf(stderr, "ERROR: " __VA_ARGS__);\
	fprintf(stderr, "\n");\
} while (/*CONSTCOND*/0)

#define UT_FATAL(...) do {\
	fprintf(stderr, "FATAL ERROR at %s:%i in %s(): ",\
			__FILE__, __LINE__, __func__);\
	fprintf(stderr, __VA_ARGS__);\
	fprintf(stderr, "\n");\
	abort();\
} while (/*CONSTCOND*/0)

#define UT_ASSERTeq(x, y) do if ((x) != (y)) {\
	UT_FATAL("ASSERT FAILED : " #x " (%llu) ≠ %llu",\
		(unsigned long long)(x), (unsigned long long)(y));\
} while (/*CONSTCOND*/0)

#define UT_ASSERTne(x, y) do if ((x) == (y)) {\
	UT_FATAL("ASSERT FAILED : " #x " (%llu) = %llu",\
		(unsigned long long)(x), (unsigned long long)(y));\
} while (/*CONSTCOND*/0)

#define UT_ASSERTin(x, min, max) do if ((x) < (min) || (x) > (max)) {\
	UT_FATAL("ASSERT FAILED : " #x " = %llu not in [%llu,%llu]",\
		(unsigned long long)(x),\
		(unsigned long long)(min), (unsigned long long)(max));\
} while (/*CONSTCOND*/0)

#define UT_LOG_SKIP(testcase) do {\
	fprintf(stdout, "SKIP: " testcase " test case");\
	fprintf(stdout, "\n");\
} while (/*CONSTCOND*/0)

static inline int
test_flag(struct vdm *vdm, unsigned flag, int expected_value)
{
	int ret = vdm_is_supported(vdm, flag) != expected_value;

	if (ret) {
		fprintf(stderr,
			"vdm_is_flag_supported(%u) returned: %u, when expected: %u\n",
			flag, ret, expected_value);
	}
	return ret;
}
#endif /* TEST_HELPERS_H */
