/*
 * Copyright 2016-2019, Intel Corporation
 * Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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
 * pmem_windows.c -- pmem utilities with OS-specific implementation
 */

#include <memoryapi.h>
#include "pmem.h"
#include "out.h"
#include "mmap.h"
#include "win_mmap.h"
#include "sys/mman.h"

#if (NTDDI_VERSION >= NTDDI_WIN10_RS1)
typedef BOOL (WINAPI *PQVM)(
		HANDLE, const void *,
		enum WIN32_MEMORY_INFORMATION_CLASS, PVOID,
		SIZE_T, PSIZE_T);

static PQVM Func_qvmi = NULL;
#endif

/*
 * is_direct_mapped -- (internal) for each page in the given region
 * checks with MM, if it's direct mapped.
 */
static int
is_direct_mapped(const void *begin, const void *end)
{
	LOG(3, "begin %p end %p", begin, end);

#if (NTDDI_VERSION >= NTDDI_WIN10_RS1)
	int retval = 1;
	WIN32_MEMORY_REGION_INFORMATION region_info;
	SIZE_T bytes_returned;

	if (Func_qvmi == NULL) {
		LOG(4, "QueryVirtualMemoryInformation not supported, "
			"assuming non-DAX.");
		return 0;
	}

	const void *begin_aligned = (const void *)rounddown((intptr_t)begin,
					Pagesize);
	const void *end_aligned = (const void *)roundup((intptr_t)end,
					Pagesize);

	for (const void *page = begin_aligned;
			page < end_aligned;
			page = (const void *)((char *)page + Pagesize)) {
		if (Func_qvmi(GetCurrentProcess(), page,
				MemoryRegionInfo, &region_info,
				sizeof(region_info), &bytes_returned)) {
			retval = region_info.DirectMapped;
		} else {
			LOG(4, "QueryVirtualMemoryInformation failed, assuming "
				"non-DAX.  Last error: %08x", GetLastError());
			retval = 0;
		}

		if (retval == 0) {
			LOG(4, "page %p is not direct mapped", page);
			break;
		}
	}

	return retval;
#else
	/* if the MM API is not available the safest answer is NO */
	return 0;
#endif /* NTDDI_VERSION >= NTDDI_WIN10_RS1 */

}

/*
 * is_pmem_detect -- implement pmem_is_pmem()
 *
 * This function returns true only if the entire range can be confirmed
 * as being direct access persistent memory.  Finding any part of the
 * range is not direct access, or failing to look up the information
 * because it is unmapped or because any sort of error happens, just
 * results in returning false.
 */
int
is_pmem_detect(const void *addr, size_t len)
{
	LOG(3, "addr %p len %zu", addr, len);

	if (len == 0)
		return 0;

	if (len > UINTPTR_MAX - (uintptr_t)addr) {
		len = UINTPTR_MAX - (uintptr_t)addr;
		LOG(4, "limit len to %zu to not get beyond address space", len);
	}

	int retval = 1;
	const void *begin = addr;
	const void *end = (const void *)((char *)addr + len);

	LOG(4, "begin %p end %p", begin, end);

	AcquireSRWLockShared(&FileMappingQLock);

	PFILE_MAPPING_TRACKER mt;
	PMDK_SORTEDQ_FOREACH(mt, &FileMappingQHead, ListEntry) {
		if (mt->BaseAddress >= end) {
			LOG(4, "ignoring all mapped ranges beyond given range");
			break;
		}
		if (mt->EndAddress <= begin) {
			LOG(4, "skipping all mapped ranges before given range");
			continue;
		}

		if (!(mt->Flags & FILE_MAPPING_TRACKER_FLAG_DIRECT_MAPPED)) {
			LOG(4, "tracked range [%p, %p) is not direct mapped",
				mt->BaseAddress, mt->EndAddress);
			retval = 0;
			break;
		}

		/*
		 * If there is a gap between the given region that we process
		 * currently and the mapped region in our tracking list, we
		 * need to process the gap by taking the long route of asking
		 * MM for each page in that range.
		 */
		if (begin < mt->BaseAddress &&
		    !is_direct_mapped(begin, mt->BaseAddress)) {
			LOG(4, "untracked range [%p, %p) is not direct mapped",
				begin, mt->BaseAddress);
			retval = 0;
			break;
		}

		/* push our begin to reflect what we have already processed */
		begin = mt->EndAddress;
	}

	/*
	 * If we still have a range to verify, check with MM if the entire
	 * region is direct mapped.
	 */
	if (begin < end && !is_direct_mapped(begin, end)) {
		LOG(4, "untracked end range [%p, %p) is not direct mapped",
			begin, end);
		retval = 0;
	}

	ReleaseSRWLockShared(&FileMappingQLock);

	LOG(4, "returning %d", retval);
	return retval;
}

/*
 * pmem_map_register -- memory map file and register mapping
 */
void *
pmem_map_register(int fd, size_t len, const char *path, int is_dev_dax)
{
	/* there is no device dax on windows */
	ASSERTeq(is_dev_dax, 0);

	return util_map(fd, 0, len, MAP_SHARED, 0, 0, NULL);
}

/*
 * pmem_os_init -- os-dependent part of pmem initialization
 */
void
pmem_os_init(void)
{
	LOG(3, NULL);
#if NTDDI_VERSION >= NTDDI_WIN10_RS1
	Func_qvmi = (PQVM)GetProcAddress(
			GetModuleHandle(TEXT("KernelBase.dll")),
			"QueryVirtualMemoryInformation");
#endif
}
