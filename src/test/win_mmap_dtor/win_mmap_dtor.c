// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * win_mmap_dtor.c -- unit test for windows mmap destructor
 */

#include "unittest.h"
#include "os.h"
#include "win_mmap.h"

#define KILOBYTE (1 << 10)
#define MEGABYTE (1 << 20)

unsigned long long Mmap_align;

int
main(int argc, char *argv[])
{
	START(argc, argv, "win_mmap_dtor");

	if (argc != 2)
		UT_FATAL("usage: %s path", argv[0]);

	SYSTEM_INFO si;
	GetSystemInfo(&si);

	/* set pagesize for mmap */
	Mmap_align = si.dwAllocationGranularity;

	const char *path = argv[1];

	int fd = os_open(path, O_RDWR);
	UT_ASSERTne(fd, -1);

	/*
	 * Input file has size equal to 2MB, but the mapping is 3MB.
	 * In this case mmap should map whole file and reserve 1MB
	 * of virtual address space for remaining part of the mapping.
	 */
	void *addr = mmap(NULL, 3 * MEGABYTE, PROT_READ, MAP_SHARED, fd, 0);
	UT_ASSERTne(addr, MAP_FAILED);

	MEMORY_BASIC_INFORMATION basic_info;
	SIZE_T bytes_returned;

	bytes_returned = VirtualQuery(addr, &basic_info,
		sizeof(basic_info));

	UT_ASSERTeq(bytes_returned, sizeof(basic_info));
	UT_ASSERTeq(basic_info.RegionSize, 2 * MEGABYTE);
	UT_ASSERTeq(basic_info.State, MEM_COMMIT);

	bytes_returned = VirtualQuery((char *)addr + 2 * MEGABYTE,
		&basic_info, sizeof(basic_info));

	UT_ASSERTeq(bytes_returned, sizeof(basic_info));
	UT_ASSERTeq(basic_info.RegionSize, MEGABYTE);
	UT_ASSERTeq(basic_info.State, MEM_RESERVE);

	win_mmap_fini();

	bytes_returned = VirtualQuery((char *)addr + 2 * MEGABYTE,
		&basic_info, sizeof(basic_info));

	UT_ASSERTeq(bytes_returned, sizeof(basic_info));
	/*
	 * region size can be bigger than 1MB because there was probably
	 * free space after this mapping
	 */
	UT_ASSERTeq(basic_info.State, MEM_FREE);

	DONE(NULL);
}
