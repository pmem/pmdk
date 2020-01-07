// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2018, Intel Corporation */

/*
 * out.h -- definitions for "out" module
 */

#ifndef PMDK_OUT_H
#define PMDK_OUT_H 1

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Suppress errors which are after appropriate ASSERT* macro for nondebug
 * builds.
 */
#if !defined(DEBUG) && (defined(__clang_analyzer__) || defined(__COVERITY__) ||\
		defined(__KLOCWORK__))
#define OUT_FATAL_DISCARD_NORETURN __attribute__((noreturn))
#else
#define OUT_FATAL_DISCARD_NORETURN
#endif

#ifndef EVALUATE_DBG_EXPRESSIONS
#if defined(DEBUG) || defined(__clang_analyzer__) || defined(__COVERITY__) ||\
	defined(__KLOCWORK__)
#define EVALUATE_DBG_EXPRESSIONS 1
#else
#define EVALUATE_DBG_EXPRESSIONS 0
#endif
#endif

#ifdef DEBUG

#define OUT_LOG out_log
#define OUT_NONL out_nonl
#define OUT_FATAL out_fatal
#define OUT_FATAL_ABORT out_fatal

#else

static __attribute__((always_inline)) inline void
out_log_discard(const char *file, int line, const char *func, int level,
		const char *fmt, ...)
{
	(void) file;
	(void) line;
	(void) func;
	(void) level;
	(void) fmt;
}

static __attribute__((always_inline)) inline void
out_nonl_discard(int level, const char *fmt, ...)
{
	(void) level;
	(void) fmt;
}

static __attribute__((always_inline)) OUT_FATAL_DISCARD_NORETURN inline void
out_fatal_discard(const char *file, int line, const char *func,
		const char *fmt, ...)
{
	(void) file;
	(void) line;
	(void) func;
	(void) fmt;
}

static __attribute__((always_inline)) NORETURN inline void
out_fatal_abort(const char *file, int line, const char *func,
		const char *fmt, ...)
{
	(void) file;
	(void) line;
	(void) func;
	(void) fmt;

	abort();
}

#define OUT_LOG out_log_discard
#define OUT_NONL out_nonl_discard
#define OUT_FATAL out_fatal_discard
#define OUT_FATAL_ABORT out_fatal_abort

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

/* produce debug/trace output */
#define LOG(level, ...) do { \
	if (!EVALUATE_DBG_EXPRESSIONS) break;\
	OUT_LOG(__FILE__, __LINE__, __func__, level, __VA_ARGS__);\
} while (0)

/* produce debug/trace output without prefix and new line */
#define LOG_NONL(level, ...) do { \
	if (!EVALUATE_DBG_EXPRESSIONS) break; \
	OUT_NONL(level, __VA_ARGS__); \
} while (0)

/* produce output and exit */
#define FATAL(...)\
	OUT_FATAL_ABORT(__FILE__, __LINE__, __func__, __VA_ARGS__)

/* assert a condition is true at runtime */
#define ASSERT_rt(cnd) do { \
	if (!EVALUATE_DBG_EXPRESSIONS || (cnd)) break; \
	OUT_FATAL(__FILE__, __LINE__, __func__, "assertion failure: %s", #cnd);\
} while (0)

/* assertion with extra info printed if assertion fails at runtime */
#define ASSERTinfo_rt(cnd, info) do { \
	if (!EVALUATE_DBG_EXPRESSIONS || (cnd)) break; \
	OUT_FATAL(__FILE__, __LINE__, __func__, \
		"assertion failure: %s (%s = %s)", #cnd, #info, info);\
} while (0)

/* assert two integer values are equal at runtime */
#define ASSERTeq_rt(lhs, rhs) do { \
	if (!EVALUATE_DBG_EXPRESSIONS || ((lhs) == (rhs))) break; \
	OUT_FATAL(__FILE__, __LINE__, __func__,\
	"assertion failure: %s (0x%llx) == %s (0x%llx)", #lhs,\
	(unsigned long long)(lhs), #rhs, (unsigned long long)(rhs)); \
} while (0)

/* assert two integer values are not equal at runtime */
#define ASSERTne_rt(lhs, rhs) do { \
	if (!EVALUATE_DBG_EXPRESSIONS || ((lhs) != (rhs))) break; \
	OUT_FATAL(__FILE__, __LINE__, __func__,\
	"assertion failure: %s (0x%llx) != %s (0x%llx)", #lhs,\
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

#define ERR(...)\
	out_err(__FILE__, __LINE__, __func__, __VA_ARGS__)

void out_init(const char *log_prefix, const char *log_level_var,
		const char *log_file_var, int major_version,
		int minor_version);
void out_fini(void);
void out(const char *fmt, ...) FORMAT_PRINTF(1, 2);
void out_nonl(int level, const char *fmt, ...) FORMAT_PRINTF(2, 3);
void out_log(const char *file, int line, const char *func, int level,
	const char *fmt, ...) FORMAT_PRINTF(5, 6);
void out_err(const char *file, int line, const char *func,
	const char *fmt, ...) FORMAT_PRINTF(4, 5);
void NORETURN out_fatal(const char *file, int line, const char *func,
	const char *fmt, ...) FORMAT_PRINTF(4, 5);
void out_set_print_func(void (*print_func)(const char *s));
void out_set_vsnprintf_func(int (*vsnprintf_func)(char *str, size_t size,
	const char *format, va_list ap));

#ifdef _WIN32
#ifndef PMDK_UTF8_API
#define out_get_errormsg out_get_errormsgW
#else
#define out_get_errormsg out_get_errormsgU
#endif
#endif

#ifndef _WIN32
const char *out_get_errormsg(void);
#else
const char *out_get_errormsgU(void);
const wchar_t *out_get_errormsgW(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
