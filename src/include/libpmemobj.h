/*
 * Copyright (c) 2014, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * libpmemobj.h -- definitions of libpmemobj entry points
 *
 * This library provides support for programming with persistent memory (pmem).
 *
 * libpmemobj provides a pmem-resident transactional object store.
 *
 * See libpmemobj(3) for details.
 */

#ifndef	LIBPMEMOBJ_H
#define	LIBPMEMOBJ_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

/*
 * opaque type internal to libpmemobj
 */
typedef struct pmemobjpool PMEMobjpool;

/*
 * PMEMOBJ_MAJOR_VERSION and PMEMOBJ_MINOR_VERSION provide the current version
 * of the libpmemobj API as provided by this header file.  Applications can
 * verify that the version available at run-time is compatible with the version
 * used at compile-time by passing these defines to pmemobj_check_version().
 */
#define	PMEMOBJ_MAJOR_VERSION 1
#define	PMEMOBJ_MINOR_VERSION 0
const char *pmemobj_check_version(
		unsigned major_required,
		unsigned minor_required);

#define	PMEMOBJ_MIN_POOL ((size_t)(1 << 21))	/* min pool size: 2MB */

PMEMobjpool *pmemobj_pool_open(const char *path);
void pmemobj_pool_close(PMEMobjpool *pop);
int pmemobj_pool_check(const char *path);

/*
 * Passing NULL to pmemobj_set_funcs() tells libpmemobj to continue to use the
 * default for that function.  The replacement functions must not make calls
 * back into libpmemobj.
 */
void pmemobj_set_funcs(
		void *(*malloc_func)(size_t size),
		void (*free_func)(void *ptr),
		void *(*realloc_func)(void *ptr, size_t size),
		char *(*strdup_func)(const char *s));

#ifdef __cplusplus
}
#endif
#endif	/* libpmemobj.h */
