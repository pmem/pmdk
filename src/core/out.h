/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2024, Intel Corporation */

/*
 * out.h -- definitions for "out" module
 */

#ifndef PMDK_OUT_H
#define PMDK_OUT_H 1

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Suppress errors messages (LOG()) in non-debug version
 * builds.
 */
#ifndef EVALUATE_DBG_EXPRESSIONS
#if defined(DEBUG) || defined(__clang_analyzer__) || defined(__COVERITY__) ||\
	defined(__KLOCWORK__)
#define EVALUATE_DBG_EXPRESSIONS 1
#else
#define EVALUATE_DBG_EXPRESSIONS 0
#endif
#endif

/* klocwork does not seem to respect __attribute__((noreturn)) */
#if defined(DEBUG) || defined(__KLOCWORK__)
#define OUT_LOG out_log
#define OUT_NONL out_nonl
#else

static __attribute__((always_inline)) inline void
out_log_discard(const char *file, int line, const char *func, int level,
		const char *fmt, ...)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(file, line, func, level, fmt);
}

static __attribute__((always_inline)) inline void
out_nonl_discard(int level, const char *fmt, ...)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(level, fmt);
}

#define OUT_LOG out_log_discard
#define OUT_NONL out_nonl_discard

#endif

#if defined(DEBUG) || defined(__KLOCWORK__)

/* produce debug/trace output */
#define LOG(level, ...) do { \
	if (!EVALUATE_DBG_EXPRESSIONS) break;\
	OUT_LOG(__FILE__, __LINE__, __func__, level, __VA_ARGS__);\
} while (0)

#else
/* macro for suppresing errors from unused variables (up to 9) */
#define LOG_SUPPRESS_UNUSED(...)\
	GLUE(LOG_SUPPRESS_ARG_, COUNT(__VA_ARGS__))(__VA_ARGS__)
#define LOG_SUPPRESS_ARG_1(X)
#define LOG_SUPPRESS_ARG_2(X, ...) SUPPRESS_ARG_1(__VA_ARGS__)
#define LOG_SUPPRESS_ARG_3(X, ...) SUPPRESS_ARG_2(__VA_ARGS__)
#define LOG_SUPPRESS_ARG_4(X, ...) SUPPRESS_ARG_3(__VA_ARGS__)
#define LOG_SUPPRESS_ARG_5(X, ...) SUPPRESS_ARG_4(__VA_ARGS__)
#define LOG_SUPPRESS_ARG_6(X, ...) SUPPRESS_ARG_5(__VA_ARGS__)
#define LOG_SUPPRESS_ARG_7(X, ...) SUPPRESS_ARG_6(__VA_ARGS__)
#define LOG_SUPPRESS_ARG_8(X, ...) SUPPRESS_ARG_7(__VA_ARGS__)
#define LOG_SUPPRESS_ARG_9(X, ...) SUPPRESS_ARG_8(__VA_ARGS__)

#define LOG(level, ...) LOG_SUPPRESS_UNUSED(__VA_ARGS__)
#endif

void out_init(const char *log_prefix, const char *log_level_var,
		const char *log_file_var, int major_version,
		int minor_version);
void out_fini(void);
void out(const char *fmt, ...) FORMAT_PRINTF(1, 2);
void out_nonl(int level, const char *fmt, ...) FORMAT_PRINTF(2, 3);
void out_log_va(const char *file, int line, const char *func, int level,
		const char *fmt, va_list ap);
void out_log(const char *file, int line, const char *func, int level,
	const char *fmt, ...) FORMAT_PRINTF(5, 6);

#ifdef __cplusplus
}
#endif

#endif
