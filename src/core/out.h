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
#else

static __attribute__((always_inline)) inline void
out_log_discard(const char *file, unsigned line, const char *func, int level,
		const char *fmt, ...)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(file, line, func, level, fmt);
}

#define OUT_LOG out_log_discard

#endif

#if defined(DEBUG) || defined(__KLOCWORK__)

/* produce debug/trace output */
#define LOG(level, ...) do { \
	if (!EVALUATE_DBG_EXPRESSIONS) break;\
	OUT_LOG(__FILE__, __LINE__, __func__, level, __VA_ARGS__);\
} while (0)

#else
/*
 * 'do { ... } while()' is required for a proper interpretation of
 * SUPPRESS_UNUSED(__VA_ARGS__) in the context of if() statement
 * without brackets {}, especially in the situation when '...' expands
 * to more than one argument in '__VA_ARGS__' list
 */
#define LOG(level, ...) do { \
		SUPPRESS_UNUSED(__VA_ARGS__); \
	} while (0)
#endif

void out_init(const char *log_prefix, const char *log_level_var,
		const char *log_file_var, int major_version,
		int minor_version);
void out_fini(void);

#ifdef DEBUG
void out_log(const char *file, unsigned line, const char *func,
	int level, const char *fmt, ...) FORMAT_PRINTF(5, 6);
#endif

#ifdef __cplusplus
}
#endif

#endif
