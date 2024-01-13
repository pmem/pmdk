/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020-2022, Intel Corporation */

/*
 * log_internal.h -- internal logging interfaces used by librpma
 */

#ifndef LIBRPMA_LOG_INTERNAL_H
#define LIBRPMA_LOG_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include "librpma.h"
#include "log_default.h"
#ifdef ATOMIC_OPERATIONS_SUPPORTED
#include <stdatomic.h>
#endif /* ATOMIC_OPERATIONS_SUPPORTED */

/* pointer to the logging function */
extern
#ifdef ATOMIC_OPERATIONS_SUPPORTED
_Atomic
#endif /* ATOMIC_OPERATIONS_SUPPORTED */
uintptr_t Rpma_log_function;

/* threshold levels */
extern
#ifdef ATOMIC_OPERATIONS_SUPPORTED
_Atomic
#endif /* ATOMIC_OPERATIONS_SUPPORTED */
enum rpma_log_level Rpma_log_threshold[RPMA_LOG_THRESHOLD_MAX];

void rpma_log_init();

void rpma_log_fini();

#define RPMA_LOG(level, format, ...) \
	do { \
		if (level <= Rpma_log_threshold[RPMA_LOG_THRESHOLD] && 0 != Rpma_log_function) { \
			((rpma_log_function *)Rpma_log_function)(level, __FILE__, __LINE__, \
					__func__, format, ##__VA_ARGS__); \
		} \
	} while (0)

#define RPMA_LOG_LEVEL_ALWAYS (RPMA_LOG_DISABLED - 1)

#define RPMA_LOG_ALWAYS(format, ...) \
	rpma_log_default_function(RPMA_LOG_LEVEL_ALWAYS, __FILE__, __LINE__, __func__, \
					format "\n", ##__VA_ARGS__)

/*
 * Set of macros that should be used as the primary API for logging.
 * Direct call to rpma_log shall be used only in exceptional, corner cases.
 */
#define RPMA_LOG_DEBUG(format, ...) \
	RPMA_LOG(RPMA_LOG_LEVEL_DEBUG, format "\n", ##__VA_ARGS__)

#define RPMA_LOG_INFO(format, ...) \
	RPMA_LOG(RPMA_LOG_LEVEL_INFO, format "\n", ##__VA_ARGS__)

#define RPMA_LOG_NOTICE(format, ...) \
	RPMA_LOG(RPMA_LOG_LEVEL_NOTICE, format "\n", ##__VA_ARGS__)

#define RPMA_LOG_WARNING(format, ...) \
	RPMA_LOG(RPMA_LOG_LEVEL_WARNING, format "\n", ##__VA_ARGS__)

#define RPMA_LOG_ERROR(format, ...) \
	RPMA_LOG(RPMA_LOG_LEVEL_ERROR, format "\n", ##__VA_ARGS__)

#define RPMA_LOG_FATAL(format, ...) \
	RPMA_LOG(RPMA_LOG_LEVEL_FATAL, format "\n", ##__VA_ARGS__)

/*
 * 'f' stands here for 'function' or 'format' where the latter may accept
 * additional arguments.
 */
#define RPMA_LOG_ERROR_WITH_ERRNO(e, f, ...) \
	RPMA_LOG_ERROR(f " failed: %s", ##__VA_ARGS__, strerror(abs(e)));

#endif /* LIBRPMA_LOG_INTERNAL_H */
