/*
 * Copyright 2014-2016, Intel Corporation
 * Copyright (c) 2015-2016, Microsoft Corporation. All rights reserved.
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
 * pmem_is_pmem_windows.c -- Windows specific unit test for pmem_is_pmem()
 *
 * usage: pmem_is_pmem_windows file [env]
 */

#include "unittest.h"

NTSTATUS
NtFreeVirtualMemory(_In_ HANDLE ProcessHandle, _Inout_ PVOID *BaseAddress,
	_Inout_ PSIZE_T RegionSize, _In_ ULONG FreeType);

#define NTHREAD 16

void *Addr;
size_t Size;

/*
 * worker -- the work each thread performs
 */
static void *
worker(void *arg)
{
	int *ret = (int *)arg;
	*ret =  pmem_is_pmem(Addr, Size);
	return NULL;
}

int
main(int argc, char *argv[])
{
	SYSTEM_INFO system_info;
	NTSTATUS nt_status;
	HANDLE file_map;
	SIZE_T chunk_length;

	START(argc, argv, "pmem_is_pmem_windows");

	if (argc !=  2)
		UT_FATAL("usage: %s file", argv[0]);

	int fd = OPEN(argv[1], O_RDWR);

	ut_util_stat_t stbuf;
	FSTAT(fd, &stbuf);

	Size = stbuf.st_size;

	GetSystemInfo(&system_info);
	chunk_length = system_info.dwAllocationGranularity;

	file_map = CreateFileMapping((HANDLE)_get_osfhandle(fd), NULL,
		PAGE_READONLY, 0, 0, NULL);
	UT_ASSERT(file_map != NULL);

	/*
	 * let's have multiple mappings in the range we are going to
	 * test
	 */
	Addr = VirtualAlloc(NULL, Size, MEM_RESERVE, PAGE_NOACCESS);

	for (size_t offset = 0;
		offset < Size;
		offset += chunk_length) {

		void *base_address = (void *)((char *)Addr + offset);
		nt_status = NtFreeVirtualMemory(GetCurrentProcess(),
				&base_address, &chunk_length, MEM_RELEASE);
		UT_ASSERTeq(nt_status, 0);
		UT_ASSERTeq(chunk_length, system_info.dwAllocationGranularity);
		UT_ASSERTeq(base_address, (void *)((char *)Addr + offset));

		if (rand() % 2) {
			MMAP(base_address, chunk_length, PROT_READ,
				MAP_SHARED | MAP_FIXED, fd, offset);
		} else
			MapViewOfFileEx(file_map, FILE_MAP_READ,
				offset >> 32,
				offset & 0x0ffffffff,
				chunk_length,
				base_address);
	}

	CloseHandle(file_map);
	CLOSE(fd);

	pthread_t threads[NTHREAD];
	int ret[NTHREAD];

	/* kick off NTHREAD threads */
	for (int i = 0; i < NTHREAD; i++)
		PTHREAD_CREATE(&threads[i], NULL, worker, &ret[i]);

	/* wait for all the threads to complete */
	for (int i = 0; i < NTHREAD; i++)
		PTHREAD_JOIN(threads[i], NULL);

	/* verify that all the threads return the same value */
	for (int i = 1; i < NTHREAD; i++)
		UT_ASSERTeq(ret[0], ret[i]);

	UT_OUT("%d", ret[0]);

	UT_OUT("%d", pmem_is_pmem(Addr, Size));

	DONE(NULL);
}
