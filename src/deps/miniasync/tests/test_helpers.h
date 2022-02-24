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

#define UT_ASSERTin(x, min, max) do if ((x) < (min) || (x) > (max)) {\
	UT_FATAL("ASSERT FAILED : " #x " = %llu not in [%llu,%llu]",\
		(unsigned long long)(x),\
		(unsigned long long)(min), (unsigned long long)(max));\
} while (/*CONSTCOND*/0)

#endif /* TEST_HELPERS_H */
