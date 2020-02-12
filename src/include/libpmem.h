// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2019, Intel Corporation */

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

#ifdef _WIN32
#include <pmemcompat.h>

#ifndef PMDK_UTF8_API
#define pmem_map_file pmem_map_fileW
#define pmem_check_version pmem_check_versionW
#define pmem_errormsg pmem_errormsgW
#else
#define pmem_map_file pmem_map_fileU
#define pmem_check_version pmem_check_versionU
#define pmem_errormsg pmem_errormsgU
#endif

#endif

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

#ifndef _WIN32
void *pmem_map_file(const char *path, size_t len, int flags, mode_t mode,
	size_t *mapped_lenp, int *is_pmemp);
#else
void *pmem_map_fileU(const char *path, size_t len, int flags, mode_t mode,
	size_t *mapped_lenp, int *is_pmemp);
void *pmem_map_fileW(const wchar_t *path, size_t len, int flags, mode_t mode,
	size_t *mapped_lenp, int *is_pmemp);
#endif

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

#ifndef _WIN32
const char *pmem_check_version(unsigned major_required,
	unsigned minor_required);
#else
const char *pmem_check_versionU(unsigned major_required,
	unsigned minor_required);
const wchar_t *pmem_check_versionW(unsigned major_required,
	unsigned minor_required);
#endif

#ifndef _WIN32
const char *pmem_errormsg(void);
#else
const char *pmem_errormsgU(void);
const wchar_t *pmem_errormsgW(void);
#endif

#ifdef __cplusplus
}
#endif
#endif	/* libpmem.h */
