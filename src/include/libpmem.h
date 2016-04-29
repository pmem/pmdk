/*
 * Copyright 2014-2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * libpmem.h -- definitions of libpmem entry points
 *
 * This library provides support for programming with persistent memory (pmem).
 *
 * libpmem provides support for using raw pmem directly.
 *
 * See libpmem(3) for details.
 */

#ifndef LIBPMEM_H
#define LIBPMEM_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

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
void pmem_flush(const void *addr, size_t len);
void pmem_drain(void);
int pmem_has_hw_drain(void);
void *pmem_memmove_persist(void *pmemdest, const void *src, size_t len);
void *pmem_memcpy_persist(void *pmemdest, const void *src, size_t len);
void *pmem_memset_persist(void *pmemdest, int c, size_t len);
void *pmem_memmove_nodrain(void *pmemdest, const void *src, size_t len);
void *pmem_memcpy_nodrain(void *pmemdest, const void *src, size_t len);
void *pmem_memset_nodrain(void *pmemdest, int c, size_t len);

/*
 * PMEM_MAJOR_VERSION and PMEM_MINOR_VERSION provide the current version of the
 * libpmem API as provided by this header file.  Applications can verify that
 * the version available at run-time is compatible with the version used at
 * compile-time by passing these defines to pmem_check_version().
 */
#define PMEM_MAJOR_VERSION 1
#define PMEM_MINOR_VERSION 0
const char *pmem_check_version(
		unsigned major_required,
		unsigned minor_required);

const char *pmem_errormsg(void);

#ifdef __cplusplus
}
#endif
#endif	/* libpmem.h */
