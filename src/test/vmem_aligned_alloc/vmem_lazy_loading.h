/*
 * Copyright 2018, Intel Corporation
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
 * vmem_lazy_loading.h -- lazy loading of libvmem
 */

#ifndef VMEM_LAZY_LOADING_H
#define VMEM_LAZY_LOADING_H 1

#ifndef _WIN32
#define LIBNAME "libvmem.so"
#define USUFFIX ""
#define WSUFFIX ""
#define LIBHANDLE void *
#else
#define LIBNAME "libvmem.dll"
#define USUFFIX "U"
#define WSUFFIX "W"
#define LIBHANDLE HMODULE
#endif

/*
 * Dynamically hook all of the VMEM exports from libvmem.h.
 * If the exports in libvmem.h are changed, new ones added, or old ones
 * deleted, these must change as well.
 */

typedef VMEM *
(*PVMEM_CREATEU)(const char *dir, size_t size);

typedef VMEM *
(*PVMEM_CREATEW)(const wchar_t *dir, size_t size);

typedef VMEM *
(*PVMEM_CREATE_IN_REGION)(void *addr, size_t size);

typedef void
(*PVMEM_DELETE)(VMEM *vmp);

typedef int
(*PVMEM_CHECK)(VMEM *vmp);

typedef void
(*PVMEM_STATS_PRINT)(VMEM *vmp, const char *opts);

typedef void *
(*PVMEM_MALLOC)(VMEM *vmp, size_t size);

typedef void *
(*PVMEM_CALLOC)(VMEM *vmp, size_t nmemb, size_t size);

typedef void *
(*PVMEM_REALLOC)(VMEM *vmp, void *ptr, size_t size);

typedef void
(*PVMEM_FREE)(VMEM *vmp, void *ptr);

typedef void *
(*PVMEM_ALIGNED_ALLOC)(VMEM *vmp, size_t alignment, size_t size);

typedef char *
(*PVMEM_STRDUP)(VMEM *vmp, const char *s);

typedef wchar_t *
(*PVMEM_WCSDUP)(VMEM *vmp, const wchar_t *s);

typedef size_t
(*PVMEM_MALLOC_USABLE_SIZE)(VMEM *vmp, void *ptr);

typedef const char *
(*PVMEM_CHECK_VERSIONU)(unsigned major_required,
	unsigned minor_required);

typedef const wchar_t *
(*PVMEM_CHECK_VERSIONW)(unsigned major_required,
	unsigned minor_required);

typedef void
(*PVMEM_SET_FUNCS)(
	void *(*malloc_func)(size_t size),
	void(*free_func)(void *ptr),
	void *(*realloc_func)(void *ptr, size_t size),
	char *(*strdup_func)(const char *s),
	void(*print_func)(const char *s));

typedef const char *
(*PVMEM_ERRORMSGU)(void);

typedef const wchar_t *
(*PVMEM_ERRORMSGW)(void);

extern PVMEM_CREATEU Pvmem_createU;
extern PVMEM_CREATEW Pvmem_createW;
extern PVMEM_CREATE_IN_REGION Pvmem_create_in_region;
extern PVMEM_DELETE Pvmem_delete;
extern PVMEM_CHECK Pvmem_check;
extern PVMEM_STATS_PRINT Pvmem_stats_print;

extern PVMEM_MALLOC Pvmem_malloc;
extern PVMEM_FREE Pvmem_free;
extern PVMEM_CALLOC Pvmem_calloc;
extern PVMEM_REALLOC Pvmem_realloc;

extern PVMEM_ALIGNED_ALLOC Pvmem_aligned_alloc;
extern PVMEM_STRDUP Pvmem_strdup;
extern PVMEM_WCSDUP Pvmem_wcsdup;
extern PVMEM_MALLOC_USABLE_SIZE Pvmem_malloc_usable_size;

extern PVMEM_CHECK_VERSIONU Pvmem_check_versionU;
extern PVMEM_CHECK_VERSIONW Pvmem_check_versionW;

extern PVMEM_SET_FUNCS Pvmem_set_funcs;

extern PVMEM_ERRORMSGU Pvmem_errormsgU;
extern PVMEM_ERRORMSGW Pvmem_errormsgW;

#ifdef _WIN32

#ifndef PMDK_UTF8_API
#define Pvmem_create Pvmem_createW
#define Pvmem_check_version Pvmem_check_versionW
#define Pvmem_errormsg Pvmem_errormsgW
#else
#define Pvmem_create Pvmem_createU
#define Pvmem_check_version Pvmem_check_versionU
#define Pvmem_errormsg Pvmem_errormsgU
#endif

#else

#define Pvmem_create Pvmem_createU
#define Pvmem_check_version Pvmem_check_versionU
#define Pvmem_errormsg Pvmem_errormsgU

#endif /* _WIN32 */

#ifdef vmem_create
#undef vmem_create
#endif
#define vmem_create Pvmem_create
#define vmem_createW Pvmem_createW
#define vmem_createU Pvmem_createU

#define vmem_create_in_region Pvmem_create_in_region
#define vmem_delete Pvmem_delete
#define vmem_check Pvmem_check
#define vmem_stats_print Pvmem_stats_print

#define vmem_malloc Pvmem_malloc
#define vmem_free Pvmem_free
#define vmem_calloc Pvmem_calloc
#define vmem_realloc Pvmem_realloc
#define vmem_aligned_alloc Pvmem_aligned_alloc

#define vmem_strdup Pvmem_strdup
#define vmem_wcsdup Pvmem_wcsdup
#define vmem_malloc_usable_size Pvmem_malloc_usable_size

#ifdef vmem_check_version
#undef vmem_check_version
#endif
#define vmem_check_version Pvmem_check_version
#define vmem_check_versionU Pvmem_check_versionU
#define vmem_check_versionW Pvmem_check_versionW

#define vmem_set_funcs Pvmem_set_funcs

#ifdef vmem_errormsg
#undef vmem_errormsg
#endif
#define vmem_errormsg Pvmem_errormsg
#define vmem_errormsgU Pvmem_errormsgU
#define vmem_errormsgW Pvmem_errormsgW

LIBHANDLE vmem_load(void);
void vmem_unload(LIBHANDLE handle);

#endif /* VMEM_LAZY_LOADING_H */
