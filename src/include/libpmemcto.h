/*
 * Copyright 2014-2017, Intel Corporation
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
 * libpmemcto -- definitions of libpmemcto entry points
 *
 * This library exposes memory-mapped files as persistent memory heap
 * with malloc-like interfaces.
 *
 * See libpmemcto(3) for details.
 */

#ifndef LIBPMEMCTO_H
#define LIBPMEMCTO_H 1

#ifdef _WIN32
#include <pmemcompat.h>

#ifndef PMDK_UTF8_API
#define pmemcto_open pmemcto_openW
#define pmemcto_create pmemcto_createW
#define pmemcto_check pmemcto_checkW
#define pmemcto_check_version pmemcto_check_versionW
#define pmemcto_errormsg pmemcto_errormsgW
#else
#define pmemcto_open pmemcto_openU
#define pmemcto_create pmemcto_createU
#define pmemcto_check pmemcto_checkU
#define pmemcto_check_version pmemcto_check_versionU
#define pmemcto_errormsg pmemcto_errormsgU
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <wchar.h>

/*
 * opaque type, internal to libpmemcto
 */
typedef struct pmemcto PMEMctopool;

/*
 * PMEMCTO_MAJOR_VERSION and PMEMCTO_MINOR_VERSION provide the current version
 * of the libpmemcto API as provided by this header file.  Applications can
 * verify that the version available at run-time is compatible with the version
 * used at compile-time by passing these defines to pmemcto_check_version().
 */
#define PMEMCTO_MAJOR_VERSION 1
#define PMEMCTO_MINOR_VERSION 0

#ifndef _WIN32
const char *pmemcto_check_version(unsigned major_required,
	unsigned minor_required);
#else
const char *pmemcto_check_versionU(unsigned major_required,
	unsigned minor_required);
const wchar_t *pmemcto_check_versionW(unsigned major_required,
	unsigned minor_required);
#endif

/* minimum pool size: 16MB */
#define PMEMCTO_MIN_POOL ((size_t)(1024 * 1024 * 16))

/*
 * This limit is set arbitrary to incorporate a pool header and required
 * alignment plus supply.
 */
#define PMEMCTO_MIN_PART ((size_t)(1024 * 1024 * 2)) /* 2 MiB */

/* maximum layout size */
#define PMEMCTO_MAX_LAYOUT ((size_t)1024)

#ifndef _WIN32
PMEMctopool *pmemcto_open(const char *path, const char *layout);
#else
PMEMctopool *pmemcto_openU(const char *path, const char *layout);
PMEMctopool *pmemcto_openW(const wchar_t *path, const wchar_t *layout);
#endif

#ifndef _WIN32
PMEMctopool *pmemcto_create(const char *path, const char *layout,
		size_t poolsize, mode_t mode);
#else
PMEMctopool *pmemcto_createU(const char *path, const char *layout,
		size_t poolsize, mode_t mode);
PMEMctopool *pmemcto_createW(const wchar_t *path, const wchar_t *layout,
		size_t poolsize, mode_t mode);
#endif

#ifndef _WIN32
int pmemcto_check(const char *path, const char *layout);
#else
int pmemcto_checkU(const char *path, const char *layout);
int pmemcto_checkW(const wchar_t *path, const wchar_t *layout);
#endif

void pmemcto_close(PMEMctopool *pcp);
void pmemcto_stats_print(PMEMctopool *pcp, const char *opts);

/*
 * support for malloc and friends...
 */
void *pmemcto_malloc(PMEMctopool *pcp, size_t size);
void pmemcto_free(PMEMctopool *pcp, void *ptr);
void *pmemcto_calloc(PMEMctopool *pcp, size_t nmemb, size_t size);
void *pmemcto_realloc(PMEMctopool *pcp, void *ptr, size_t size);
void *pmemcto_aligned_alloc(PMEMctopool *pcp, size_t alignment, size_t size);
char *pmemcto_strdup(PMEMctopool *pcp, const char *s);
wchar_t *pmemcto_wcsdup(PMEMctopool *pcp, const wchar_t *s);
size_t pmemcto_malloc_usable_size(PMEMctopool *pcp, void *ptr);

/*
 * close-to-open persistence...
 */
void pmemcto_set_root_pointer(PMEMctopool *pcp, void *ptr);
void *pmemcto_get_root_pointer(PMEMctopool *pcp);

/*
 * Passing NULL to pmemcto_set_funcs() tells libpmemcto to continue to use
 * the default for that function.  The replacement functions must
 * not make calls back into libpmemcto.
 *
 * The print_func is called by libpmemcto based on the environment
 * variable PMEMCTO_LOG_LEVEL:
 *	0 or unset: print_func is only called for pmemcto_stats_print()
 *	1:          additional details are logged when errors are returned
 *	2:          basic operations (allocations/frees) are logged
 *	3:          produce very verbose tracing of function calls in libpmemcto
 *	4:          also log obscure stuff used to debug the library itself
 *
 * The default print_func prints to stderr.  Applications can override this
 * by setting the environment variable PMEMCTO_LOG_FILE, or by supplying a
 * replacement print function.
 */
void pmemcto_set_funcs(
		void *(*malloc_func)(size_t size),
		void (*free_func)(void *ptr),
		void *(*realloc_func)(void *ptr, size_t size),
		char *(*strdup_func)(const char *s),
		void (*print_func)(const char *s));

#ifndef _WIN32
const char *pmemcto_errormsg(void);
#else
const char *pmemcto_errormsgU(void);
const wchar_t *pmemcto_errormsgW(void);
#endif

#ifdef __cplusplus
}
#endif
#endif	/* libpmemcto.h */
