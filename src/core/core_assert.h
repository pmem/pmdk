/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2024, Intel Corporation */

/*
 * core_assert.h -- definitions for the "core_assert" module
 */

#ifndef CORE_ASSERT_H
#define CORE_ASSERT_H

#include "log_internal.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__KLOCWORK__)
#define TEST_ALWAYS_TRUE_EXPR(cnd)
#define TEST_ALWAYS_EQ_EXPR(cnd)
#define TEST_ALWAYS_NE_EXPR(cnd)
#else
#define TEST_ALWAYS_TRUE_EXPR(cnd)\
	if (__builtin_constant_p(cnd))\
		ASSERT_COMPILE_ERROR_ON(cnd);
#define TEST_ALWAYS_EQ_EXPR(lhs, rhs)\
	if (__builtin_constant_p(lhs) && __builtin_constant_p(rhs))\
		ASSERT_COMPILE_ERROR_ON((lhs) == (rhs));
#define TEST_ALWAYS_NE_EXPR(lhs, rhs)\
	if (__builtin_constant_p(lhs) && __builtin_constant_p(rhs))\
		ASSERT_COMPILE_ERROR_ON((lhs) != (rhs));
#endif

/* assert a condition is true at runtime */
#if defined(DEBUG) || defined(__clang_analyzer__) || defined(__COVERITY__) ||\
	defined(__KLOCWORK__)
#define ASSERT_rt(cnd) do { \
	if ((cnd)) break; \
	CORE_LOG_FATAL("assertion failure: %s", #cnd);\
} while (0)

/* assertion with extra info printed if assertion fails at runtime */
#define ASSERTinfo_rt(cnd, info) do { \
	if ((cnd)) break; \
	CORE_LOG_FATAL("assertion failure: %s (%s = %s)", #cnd, #info, info);\
} while (0)

/* assert two integer values are equal at runtime */
#define ASSERTeq_rt(lhs, rhs) do { \
	if ((lhs) == (rhs)) break; \
	CORE_LOG_FATAL( \
		"assertion failure: %s (0x%llx) == %s (0x%llx)", #lhs, \
		(unsigned long long)(lhs), #rhs, (unsigned long long)(rhs)); \
} while (0)

/* assert two integer values are not equal at runtime */
#define ASSERTne_rt(lhs, rhs) do { \
	if ((lhs) != (rhs)) break; \
	CORE_LOG_FATAL("assertion failure: %s (0x%llx) != %s (0x%llx)", #lhs,\
		(unsigned long long)(lhs), #rhs, (unsigned long long)(rhs)); \
} while (0)

/* assert a condition is true */
#define ASSERT(cnd)\
	do {\
		/*\
		 * Detect useless asserts on always true expression. Please use\
		 * COMPILE_ERROR_ON(!cnd) or ASSERT_rt(cnd) in such cases.\
		 */\
		TEST_ALWAYS_TRUE_EXPR(cnd);\
		ASSERT_rt(cnd);\
	} while (0)

/* assertion with extra info printed if assertion fails */
#define ASSERTinfo(cnd, info)\
	do {\
		/* See comment in ASSERT. */\
		TEST_ALWAYS_TRUE_EXPR(cnd);\
		ASSERTinfo_rt(cnd, info);\
	} while (0)

/* assert two integer values are equal */
#define ASSERTeq(lhs, rhs)\
	do {\
		/* See comment in ASSERT. */\
		TEST_ALWAYS_EQ_EXPR(lhs, rhs);\
		ASSERTeq_rt(lhs, rhs);\
	} while (0)

/* assert two integer values are not equal */
#define ASSERTne(lhs, rhs)\
	do {\
		/* See comment in ASSERT. */\
		TEST_ALWAYS_NE_EXPR(lhs, rhs);\
		ASSERTne_rt(lhs, rhs);\
	} while (0)
#else
#define ASSERT_rt(cnd)
#define ASSERTinfo_rt(cnd, info)
#define ASSERTeq_rt(lhs, rhs)
#define ASSERTne_rt(lhs, rhs)
#define ASSERT(cnd)
#define ASSERTinfo(cnd, info)
#define ASSERTeq(lhs, rhs)
#define ASSERTne(lhs, rhs)
#endif /* DEBUG */

#ifdef __cplusplus
}
#endif

#endif /* CORE_ASSERT_H */
