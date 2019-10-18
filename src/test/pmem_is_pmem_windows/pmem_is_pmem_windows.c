/*
 * Copyright 2014-2019, Intel Corporation
 * Copyright (c) 2015-2017, Microsoft Corporation. All rights reserved.
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
 * pmem_is_pmem_windows.c -- Windows specific unit test for is_pmem_detect()
 *
 * usage: pmem_is_pmem_windows file [env]
 */

#include "unittest.h"
#include "pmem.h"
#include "queue.h"
#include "win_mmap.h"
#include "util.h"

#define NTHREAD 16

static void *Addr;
static size_t Size;
static int pmem_is_pmem_force = 0;

enum test_mmap_scenarios {
	TEST_MMAP_SCENARIO_UNKNOWN,
	TEST_MMAP_SCENARIO_BEGIN_HOLE,
	TEST_MMAP_SCENARIO_END_HOLE,
	TEST_MMAP_SCENARIO_MIDDLE_HOLE,
	TEST_MMAP_SCENARIO_NO_HOLE
};

enum test_mmap_scenarios
get_mmap_scenarios(char *name)
{
	if (stricmp(name, "nothing") == 0)
		return TEST_MMAP_SCENARIO_NO_HOLE;
	if (stricmp(name, "begin") == 0)
		return TEST_MMAP_SCENARIO_BEGIN_HOLE;
	if (stricmp(name, "end") == 0)
		return TEST_MMAP_SCENARIO_END_HOLE;
	if (stricmp(name, "middle") == 0)
		return TEST_MMAP_SCENARIO_MIDDLE_HOLE;
	return TEST_MMAP_SCENARIO_UNKNOWN;
}

/*
 * mmap_file_mapping_comparer -- (internal) compares the two file mapping
 * trackers
 */
static LONG_PTR
mmap_file_mapping_comparer(PFILE_MAPPING_TRACKER a, PFILE_MAPPING_TRACKER b)
{
	return ((LONG_PTR)a->BaseAddress - (LONG_PTR)b->BaseAddress);
}

/*
 * worker -- the work each thread performs
 */
static void *
worker(void *arg)
{
	int *ret = (int *)arg;

	/*
	 * We honor the force just to let the scenarios that require pmem fs
	 * work in the environment that forces pmem.
	 *
	 * NOTE: We can't use pmem_is_pmem instead of checking for the ENV
	 * variable explicitly, because we want to call is_pmem_detect that is
	 * defined in this test so that it will use the FileMappingQHead
	 * that's defined here.  Because we are crafting the Q in the test.
	 */
	if (pmem_is_pmem_force)
		*ret = 1;
	else
		*ret =  is_pmem_detect(Addr, Size);
	return NULL;
}

extern SRWLOCK FileMappingQLock;
extern struct FMLHead FileMappingQHead;

int
main(int argc, char *argv[])
{
	HANDLE file_map;
	SIZE_T chunk_length;
	enum test_mmap_scenarios scenario;
	int still_holey = 1;
	int already_holey = 0;

	START(argc, argv, "pmem_is_pmem_windows");

	if (argc !=  3)
		UT_FATAL("usage: %s file {begin|end|middle|nothing}", argv[0]);

	util_init(); /* to initialize Mmap_align */

	char *str_pmem_is_pmem_force = os_getenv("PMEM_IS_PMEM_FORCE");
	if (str_pmem_is_pmem_force && atoi(str_pmem_is_pmem_force) == 1)
		pmem_is_pmem_force = 1;

	scenario = get_mmap_scenarios(argv[2]);
	UT_ASSERT(scenario != TEST_MMAP_SCENARIO_UNKNOWN);

	int fd = OPEN(argv[1], O_RDWR);

	os_stat_t stbuf;
	FSTAT(fd, &stbuf);

	Size = stbuf.st_size;

	chunk_length = Mmap_align;

	/*
	 * We don't support too small a file size.
	 */
	UT_ASSERT(Size / 8 > chunk_length);

	file_map = CreateFileMapping((HANDLE)_get_osfhandle(fd), NULL,
		PAGE_READONLY, 0, 0, NULL);
	UT_ASSERT(file_map != NULL);

	Addr = MapViewOfFile(file_map, FILE_MAP_READ, 0, 0, 0);

	/*
	 * let's setup FileMappingQHead such that, it appears to have lot of
	 * DAX mapping created through our mmap.  Here are our cases based
	 * on the input:
	 *  - entire region in mapped through our mmap
	 *  - there is a region at the beginning that's not mapped through our
	 *    mmap
	 *  - there is a region at the end that's not mapped through our mmap
	 *  - there is a region in the middle that mapped through our mmap
	 */

	for (size_t offset = 0;
		offset < Size;
		offset += chunk_length) {

		void *base_address = (void *)((char *)Addr + offset);

		switch (scenario) {
		case TEST_MMAP_SCENARIO_BEGIN_HOLE:
			if (still_holey &&
				((offset == 0) || ((rand() % 2) == 0)) &&
				(offset < (Size / 2)))
				continue;
			else
				still_holey = 0;
			break;

		case TEST_MMAP_SCENARIO_END_HOLE:
			if ((offset > (Size / 2)) &&
				(already_holey || ((rand() % 2) == 0) ||
					(offset >= (Size - chunk_length)))) {
				already_holey = 1;
				continue;
			} else
				UT_ASSERT(!already_holey);
			break;

		case TEST_MMAP_SCENARIO_MIDDLE_HOLE:
			if ((((offset > (Size / 8)) && ((rand() % 2) == 0)) ||
				(offset > (Size / 8) * 6)) &&
				(offset < (Size / 8) * 7))
				continue;
			break;
		}

		PFILE_MAPPING_TRACKER mt =
			MALLOC(sizeof(struct FILE_MAPPING_TRACKER));

		mt->Flags = FILE_MAPPING_TRACKER_FLAG_DIRECT_MAPPED;
		mt->FileHandle = (HANDLE)_get_osfhandle(fd);
		mt->FileMappingHandle = file_map;
		mt->BaseAddress = base_address;
		mt->EndAddress = (void *)((char *)base_address + chunk_length);
		mt->Access = FILE_MAP_READ;
		mt->Offset = offset;

		AcquireSRWLockExclusive(&FileMappingQLock);
		PMDK_SORTEDQ_INSERT(&FileMappingQHead, mt, ListEntry,
			FILE_MAPPING_TRACKER,
			mmap_file_mapping_comparer);
		ReleaseSRWLockExclusive(&FileMappingQLock);
	}

	CloseHandle(file_map);
	CLOSE(fd);

	os_thread_t threads[NTHREAD];
	int ret[NTHREAD];

	/* kick off NTHREAD threads */
	for (int i = 0; i < NTHREAD; i++)
		PTHREAD_CREATE(&threads[i], NULL, worker, &ret[i]);

	/* wait for all the threads to complete */
	for (int i = 0; i < NTHREAD; i++)
		PTHREAD_JOIN(&threads[i], NULL);

	/* verify that all the threads return the same value */
	for (int i = 1; i < NTHREAD; i++)
		UT_ASSERTeq(ret[0], ret[i]);

	UT_OUT("%d", ret[0]);

	DONE(NULL);
}

/*
 * Since libpmem is linked statically, we need to invoke its ctor/dtor.
 */
MSVC_CONSTR(libpmem_init)
MSVC_DESTR(libpmem_fini)
