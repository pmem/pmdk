// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2019, Intel Corporation */

/*
 * libpmemblk.h -- definitions of libpmemblk entry points
 *
 * This library provides support for programming with persistent memory (pmem).
 *
 * libpmemblk provides support for arrays of atomically-writable blocks.
 *
 * See libpmemblk(7) for details.
 */

#ifndef LIBPMEMBLK_H
#define LIBPMEMBLK_H 1

#include <sys/types.h>

#ifdef _WIN32
#include <pmemcompat.h>

#ifndef PMDK_UTF8_API
#define pmemblk_open pmemblk_openW
#define pmemblk_create pmemblk_createW
#define pmemblk_check pmemblk_checkW
#define pmemblk_check_version pmemblk_check_versionW
#define pmemblk_errormsg pmemblk_errormsgW
#define pmemblk_ctl_get pmemblk_ctl_getW
#define pmemblk_ctl_set pmemblk_ctl_setW
#define pmemblk_ctl_exec pmemblk_ctl_execW
#else
#define pmemblk_open pmemblk_openU
#define pmemblk_create pmemblk_createU
#define pmemblk_check pmemblk_checkU
#define pmemblk_check_version pmemblk_check_versionU
#define pmemblk_errormsg pmemblk_errormsgU
#define pmemblk_ctl_get pmemblk_ctl_getU
#define pmemblk_ctl_set pmemblk_ctl_setU
#define pmemblk_ctl_exec pmemblk_ctl_execU
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * opaque type, internal to libpmemblk
 */
typedef struct pmemblk PMEMblkpool;

/*
 * PMEMBLK_MAJOR_VERSION and PMEMBLK_MINOR_VERSION provide the current version
 * of the libpmemblk API as provided by this header file.  Applications can
 * verify that the version available at run-time is compatible with the version
 * used at compile-time by passing these defines to pmemblk_check_version().
 */
#define PMEMBLK_MAJOR_VERSION 1
#define PMEMBLK_MINOR_VERSION 1

#ifndef _WIN32
const char *pmemblk_check_version(unsigned major_required,
	unsigned minor_required);
#else
const char *pmemblk_check_versionU(unsigned major_required,
	unsigned minor_required);
const wchar_t *pmemblk_check_versionW(unsigned major_required,
	unsigned minor_required);
#endif

/* XXX - unify minimum pool size for both OS-es */

#ifndef _WIN32
#if defined(__x86_64__) || defined(__M_X64__) || defined(__aarch64__)
/* minimum pool size: 16MiB + 4KiB (minimum BTT size + mmap alignment) */
#define PMEMBLK_MIN_POOL ((size_t)((1u << 20) * 16 + (1u << 10) * 8))
#elif defined(__PPC64__)
/* minimum pool size: 16MiB + 128KiB (minimum BTT size + mmap alignment) */
#define PMEMBLK_MIN_POOL ((size_t)((1u << 20) * 16 + (1u << 10) * 128))
#else
#error unable to recognize ISA at compile time
#endif
#else
/* minimum pool size: 16MiB + 64KiB (minimum BTT size + mmap alignment) */
#define PMEMBLK_MIN_POOL ((size_t)((1u << 20) * 16 + (1u << 10) * 64))
#endif

/*
 * This limit is set arbitrary to incorporate a pool header and required
 * alignment plus supply.
 */
#define PMEMBLK_MIN_PART ((size_t)(1024 * 1024 * 2)) /* 2 MiB */

#define PMEMBLK_MIN_BLK ((size_t)512)

#ifndef _WIN32
PMEMblkpool *pmemblk_open(const char *path, size_t bsize);
#else
PMEMblkpool *pmemblk_openU(const char *path, size_t bsize);
PMEMblkpool *pmemblk_openW(const wchar_t *path, size_t bsize);
#endif

#ifndef _WIN32
PMEMblkpool *pmemblk_create(const char *path, size_t bsize,
		size_t poolsize, mode_t mode);
#else
PMEMblkpool *pmemblk_createU(const char *path, size_t bsize,
		size_t poolsize, mode_t mode);
PMEMblkpool *pmemblk_createW(const wchar_t *path, size_t bsize,
		size_t poolsize, mode_t mode);
#endif

#ifndef _WIN32
int pmemblk_check(const char *path, size_t bsize);
#else
int pmemblk_checkU(const char *path, size_t bsize);
int pmemblk_checkW(const wchar_t *path, size_t bsize);
#endif

void pmemblk_close(PMEMblkpool *pbp);
size_t pmemblk_bsize(PMEMblkpool *pbp);
size_t pmemblk_nblock(PMEMblkpool *pbp);
int pmemblk_read(PMEMblkpool *pbp, void *buf, long long blockno);
int pmemblk_write(PMEMblkpool *pbp, const void *buf, long long blockno);
int pmemblk_set_zero(PMEMblkpool *pbp, long long blockno);
int pmemblk_set_error(PMEMblkpool *pbp, long long blockno);

/*
 * Passing NULL to pmemblk_set_funcs() tells libpmemblk to continue to use the
 * default for that function.  The replacement functions must not make calls
 * back into libpmemblk.
 */
void pmemblk_set_funcs(
		void *(*malloc_func)(size_t size),
		void (*free_func)(void *ptr),
		void *(*realloc_func)(void *ptr, size_t size),
		char *(*strdup_func)(const char *s));

#ifndef _WIN32
const char *pmemblk_errormsg(void);
#else
const char *pmemblk_errormsgU(void);
const wchar_t *pmemblk_errormsgW(void);
#endif

#ifndef _WIN32
/* EXPERIMENTAL */
int pmemblk_ctl_get(PMEMblkpool *pbp, const char *name, void *arg);
int pmemblk_ctl_set(PMEMblkpool *pbp, const char *name, void *arg);
int pmemblk_ctl_exec(PMEMblkpool *pbp, const char *name, void *arg);
#else
int pmemblk_ctl_getU(PMEMblkpool *pbp, const char *name, void *arg);
int pmemblk_ctl_getW(PMEMblkpool *pbp, const wchar_t *name, void *arg);
int pmemblk_ctl_setU(PMEMblkpool *pbp, const char *name, void *arg);
int pmemblk_ctl_setW(PMEMblkpool *pbp, const wchar_t *name, void *arg);
int pmemblk_ctl_execU(PMEMblkpool *pbp, const char *name, void *arg);
int pmemblk_ctl_execW(PMEMblkpool *pbp, const wchar_t *name, void *arg);
#endif

#ifdef __cplusplus
}
#endif
#endif	/* libpmemblk.h */
