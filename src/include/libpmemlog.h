/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2023, Intel Corporation */

/*
 * Libpmemlog is deprecated.
 *
 * libpmemlog.h -- definitions of libpmemlog entry points
 *
 * This library provides support for programming with persistent memory (pmem).
 *
 * libpmemlog provides support for pmem-resident log files.
 *
 * See libpmemlog(7) for details.
 */

#ifndef LIBPMEMLOG_H
#define LIBPMEMLOG_H 1

#include <sys/types.h>
#include <sys/uio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_DEPR_STR "Libpmemlog is deprecated."
#define PMEMLOG_DEPR_ATTR __attribute__((deprecated(LOG_DEPR_STR)))

/*
 * opaque type, internal to libpmemlog
 */
typedef struct pmemlog PMEMlogpool;

/*
 * PMEMLOG_MAJOR_VERSION and PMEMLOG_MINOR_VERSION provide the current
 * version of the libpmemlog API as provided by this header file.
 * Applications can verify that the version available at run-time
 * is compatible with the version used at compile-time by passing
 * these defines to pmemlog_check_version().
 */
#define PMEMLOG_MAJOR_VERSION 1
#define PMEMLOG_MINOR_VERSION 1

PMEMLOG_DEPR_ATTR
const char *pmemlog_check_version(unsigned major_required,
	unsigned minor_required);

/*
 * support for PMEM-resident log files...
 */
#define PMEMLOG_MIN_POOL ((size_t)(1024 * 1024 * 2)) /* min pool size: 2MiB */

/*
 * This limit is set arbitrary to incorporate a pool header and required
 * alignment plus supply.
 */
#define PMEMLOG_MIN_PART ((size_t)(1024 * 1024 * 2)) /* 2 MiB */

PMEMLOG_DEPR_ATTR
PMEMlogpool *pmemlog_open(const char *path);

PMEMLOG_DEPR_ATTR
PMEMlogpool *pmemlog_create(const char *path, size_t poolsize, mode_t mode);

PMEMLOG_DEPR_ATTR
int pmemlog_check(const char *path);

PMEMLOG_DEPR_ATTR
void pmemlog_close(PMEMlogpool *plp);
PMEMLOG_DEPR_ATTR
size_t pmemlog_nbyte(PMEMlogpool *plp);
PMEMLOG_DEPR_ATTR
int pmemlog_append(PMEMlogpool *plp, const void *buf, size_t count);
PMEMLOG_DEPR_ATTR
int pmemlog_appendv(PMEMlogpool *plp, const struct iovec *iov, int iovcnt);
PMEMLOG_DEPR_ATTR
long long pmemlog_tell(PMEMlogpool *plp);
PMEMLOG_DEPR_ATTR
void pmemlog_rewind(PMEMlogpool *plp);
PMEMLOG_DEPR_ATTR
void pmemlog_walk(PMEMlogpool *plp, size_t chunksize,
	int (*process_chunk)(const void *buf, size_t len, void *arg),
	void *arg);

/*
 * Passing NULL to pmemlog_set_funcs() tells libpmemlog to continue to use the
 * default for that function.  The replacement functions must not make calls
 * back into libpmemlog.
 */
PMEMLOG_DEPR_ATTR
void pmemlog_set_funcs(
		void *(*malloc_func)(size_t size),
		void (*free_func)(void *ptr),
		void *(*realloc_func)(void *ptr, size_t size),
		char *(*strdup_func)(const char *s));

PMEMLOG_DEPR_ATTR
const char *pmemlog_errormsg(void);

/* EXPERIMENTAL */
PMEMLOG_DEPR_ATTR
int pmemlog_ctl_get(PMEMlogpool *plp, const char *name, void *arg);
PMEMLOG_DEPR_ATTR
int pmemlog_ctl_set(PMEMlogpool *plp, const char *name, void *arg);
PMEMLOG_DEPR_ATTR
int pmemlog_ctl_exec(PMEMlogpool *plp, const char *name, void *arg);

#ifdef __cplusplus
}
#endif
#endif	/* libpmemlog.h */
