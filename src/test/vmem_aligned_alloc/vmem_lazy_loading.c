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
 * vmem_lazy_loading.c -- lazy loading of libvmem
 */

#include "unittest.h"
#include "vmem_lazy_loading.h"

PVMEM_CREATEU Pvmem_createU = NULL;
PVMEM_CREATEW Pvmem_createW = NULL;
PVMEM_CREATE_IN_REGION Pvmem_create_in_region = NULL;
PVMEM_DELETE Pvmem_delete = NULL;
PVMEM_CHECK Pvmem_check = NULL;
PVMEM_STATS_PRINT Pvmem_stats_print = NULL;

PVMEM_MALLOC Pvmem_malloc = NULL;
PVMEM_FREE Pvmem_free = NULL;
PVMEM_CALLOC Pvmem_calloc = NULL;
PVMEM_REALLOC Pvmem_realloc = NULL;

PVMEM_ALIGNED_ALLOC Pvmem_aligned_alloc = NULL;
PVMEM_STRDUP Pvmem_strdup = NULL;
PVMEM_WCSDUP Pvmem_wcsdup = NULL;
PVMEM_MALLOC_USABLE_SIZE Pvmem_malloc_usable_size = NULL;

PVMEM_CHECK_VERSIONU Pvmem_check_versionU = NULL;
PVMEM_CHECK_VERSIONW Pvmem_check_versionW = NULL;

PVMEM_SET_FUNCS Pvmem_set_funcs = NULL;

PVMEM_ERRORMSGU Pvmem_errormsgU = NULL;
PVMEM_ERRORMSGW Pvmem_errormsgW = NULL;

/*
 * vmem_load -- load libvmem and all its symbols
 */
LIBHANDLE
vmem_load(void)
{
	LIBHANDLE handle = ut_libopen(LIBNAME);

	Pvmem_createU = (PVMEM_CREATEU)ut_libsym(handle, "vmem_create" USUFFIX);
	Pvmem_createW = (PVMEM_CREATEW)ut_libsym(handle, "vmem_create" WSUFFIX);

	Pvmem_create_in_region = (PVMEM_CREATE_IN_REGION)ut_libsym(handle,
			"vmem_create_in_region");
	Pvmem_delete = (PVMEM_DELETE)ut_libsym(handle, "vmem_delete");
	Pvmem_check = (PVMEM_CHECK)ut_libsym(handle, "vmem_check");
	Pvmem_stats_print = (PVMEM_STATS_PRINT)ut_libsym(handle,
			"vmem_stats_print");

	Pvmem_malloc = (PVMEM_MALLOC)ut_libsym(handle, "vmem_malloc");
	Pvmem_free = (PVMEM_FREE)ut_libsym(handle, "vmem_free");
	Pvmem_calloc = (PVMEM_CALLOC)ut_libsym(handle, "vmem_calloc");
	Pvmem_realloc = (PVMEM_REALLOC)ut_libsym(handle, "vmem_realloc");

	Pvmem_aligned_alloc = (PVMEM_ALIGNED_ALLOC)ut_libsym(handle,
			"vmem_aligned_alloc");

	Pvmem_strdup = (PVMEM_STRDUP)ut_libsym(handle, "vmem_strdup");
	Pvmem_wcsdup = (PVMEM_WCSDUP)ut_libsym(handle, "vmem_wcsdup");
	Pvmem_malloc_usable_size = (PVMEM_MALLOC_USABLE_SIZE)ut_libsym(handle,
			"vmem_malloc_usable_size");

	Pvmem_check_versionU = (PVMEM_CHECK_VERSIONU)ut_libsym(handle,
			"vmem_check_version" USUFFIX);
	Pvmem_check_versionW = (PVMEM_CHECK_VERSIONW)ut_libsym(handle,
			"vmem_check_version" WSUFFIX);

	Pvmem_set_funcs = (PVMEM_SET_FUNCS)ut_libsym(handle, "vmem_set_funcs");

	Pvmem_errormsgU = (PVMEM_ERRORMSGU)ut_libsym(handle,
			"vmem_errormsg" USUFFIX);
	Pvmem_errormsgW = (PVMEM_ERRORMSGW)ut_libsym(handle,
			"vmem_errormsg" WSUFFIX);

	return handle;
}

/*
 * vmem_unload -- unload libvmem
 */
void
vmem_unload(LIBHANDLE handle)
{
	ut_libclose(handle);
}
