/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2024, Intel Corporation */

/*
 * libpmem.h -- definitions of libpmem entry points
 *
 * This library provides support for programming with persistent memory (pmem).
 *
 * libpmem provides support for using raw pmem directly.
 *
 * See libpmem(7) for details.
 */

#ifndef LIBPMEM_H
#define LIBPMEM_H 1

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This limit is set arbitrary to incorporate a pool header and required
 * alignment plus supply.
 */
#define PMEM_MIN_PART ((size_t)(1024 * 1024 * 2)) /* 2 MiB */

/*
 * flags supported by pmem_map_file()
 */
#define PMEM_FILE_CREATE	(1 << 0)
#define PMEM_FILE_EXCL		(1 << 1)
#define PMEM_FILE_SPARSE	(1 << 2)
#define PMEM_FILE_TMPFILE	(1 << 3)

void *pmem_map_file(const char *path, size_t len, int flags, mode_t mode,
	size_t *mapped_lenp, int *is_pmemp);

int pmem_unmap(void *addr, size_t len);
int pmem_is_pmem(const void *addr, size_t len);
void pmem_persist(const void *addr, size_t len);
int pmem_msync(const void *addr, size_t len);
int pmem_has_auto_flush(void);
void pmem_flush(const void *addr, size_t len);
void pmem_deep_flush(const void *addr, size_t len);
int pmem_deep_drain(const void *addr, size_t len);
int pmem_deep_persist(const void *addr, size_t len);
void pmem_drain(void);
int pmem_has_hw_drain(void);

void *pmem_memmove_persist(void *pmemdest, const void *src, size_t len);
void *pmem_memcpy_persist(void *pmemdest, const void *src, size_t len);
void *pmem_memset_persist(void *pmemdest, int c, size_t len);
void *pmem_memmove_nodrain(void *pmemdest, const void *src, size_t len);
void *pmem_memcpy_nodrain(void *pmemdest, const void *src, size_t len);
void *pmem_memset_nodrain(void *pmemdest, int c, size_t len);

#define PMEM_F_MEM_NODRAIN	(1U << 0)

#define PMEM_F_MEM_NONTEMPORAL	(1U << 1)
#define PMEM_F_MEM_TEMPORAL	(1U << 2)

#define PMEM_F_MEM_WC		(1U << 3)
#define PMEM_F_MEM_WB		(1U << 4)

#define PMEM_F_MEM_NOFLUSH	(1U << 5)

#define PMEM_F_MEM_VALID_FLAGS (PMEM_F_MEM_NODRAIN | \
				PMEM_F_MEM_NONTEMPORAL | \
				PMEM_F_MEM_TEMPORAL | \
				PMEM_F_MEM_WC | \
				PMEM_F_MEM_WB | \
				PMEM_F_MEM_NOFLUSH)

void *pmem_memmove(void *pmemdest, const void *src, size_t len, unsigned flags);
void *pmem_memcpy(void *pmemdest, const void *src, size_t len, unsigned flags);
void *pmem_memset(void *pmemdest, int c, size_t len, unsigned flags);

/*
 * PMEM_MAJOR_VERSION and PMEM_MINOR_VERSION provide the current version of the
 * libpmem API as provided by this header file.  Applications can verify that
 * the version available at run-time is compatible with the version used at
 * compile-time by passing these defines to pmem_check_version().
 */
#define PMEM_MAJOR_VERSION 1
#define PMEM_MINOR_VERSION 1

const char *pmem_check_version(unsigned major_required,
	unsigned minor_required);

const char *pmem_errormsg(void);

/*
 * Available log levels. Log levels are used in the logging API calls
 * to indicate logging message severity. Log levels are also used
 * to define thresholds for the logging.
 */
enum pmem_log_level {
	/* only basic library info */
	PMEM_LOG_LEVEL_HARK,
	/* an error that causes the program to stop working immediately */
	PMEM_LOG_LEVEL_FATAL,
	/* an error that causes the current operation to fail */
	PMEM_LOG_LEVEL_ERROR,
	/*
	 * an unexpected situation that does NOT cause
	 * the current operation to fail
	 */
	PMEM_LOG_LEVEL_WARNING,
	/* non-massive info mainly related to public API function completions */
	PMEM_LOG_LEVEL_NOTICE,
	/* massive info e.g. every write operation indication */
	PMEM_LOG_LEVEL_INFO,
	/* debug info e.g. write operation dump */
	PMEM_LOG_LEVEL_DEBUG,
};

enum pmem_log_threshold {
	/*
	 * the main threshold level - the logging messages less severe than
	 * indicated by this threshold's value won't trigger the logging
	 * functions
	 */
	PMEM_LOG_THRESHOLD,
	/*
	 * the auxiliary threshold level - may or may not be used by the logging
	 * function
	 */
	PMEM_LOG_THRESHOLD_AUX,
};

/*
 * pmem_log_set_threshold - set the logging threshold value
 */
int pmem_log_set_threshold(enum pmem_log_threshold threshold,
	enum pmem_log_level value);

/*
 * pmem_log_get_threshold - get the logging threshold value
 */
int pmem_log_get_threshold(enum pmem_log_threshold threshold,
	enum pmem_log_level *value);

/*
 * the type used for defining logging functions
 */
typedef void pmem_log_function(
	/* the log level of the message */
	enum pmem_log_level level,
	/* name of the source file where the message coming from */
	const char *file_name,
	/* the source file line where the message coming from */
	unsigned line_no,
	/* the function name where the message coming from */
	const char *function_name,
	/* message */
	const char *message);

#define PMEM_LOG_USE_DEFAULT_FUNCTION (NULL)

/*
 * pmem_log_set_function - set the logging function
 */
int pmem_log_set_function(pmem_log_function *log_function);

#ifdef __cplusplus
}
#endif
#endif	/* libpmem.h */
