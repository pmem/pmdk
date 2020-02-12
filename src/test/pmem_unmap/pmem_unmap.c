// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * pmem_unmap.c -- unit tests for pmem_unmap
 */

#include "unittest.h"

#define KILOBYTE (1 << 10)
#define MEGABYTE (1 << 20)

#define PAGE_4K (4 * KILOBYTE)
#define PAGE_2M (2 * MEGABYTE)

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem_unmap");
	const char *path;
	unsigned long long len;
	int flags;
	mode_t mode;
	size_t mlenp;
	size_t size;
	int is_pmem;
	char *ret;
	os_stat_t stbuf;
	if (argc != 2)
		UT_FATAL("usage: %s path", argv[0]);

	path = argv[1];
	len = 0;
	flags = 0;
	mode = S_IWUSR | S_IRUSR;

	STAT(path, &stbuf);
	size = (size_t)stbuf.st_size;

	UT_ASSERTeq(size, 20 * MEGABYTE);

	ret = pmem_map_file(path, len, flags, mode, &mlenp, &is_pmem);
	UT_ASSERTeq(pmem_unmap(ret, PAGE_4K), 0);

	ret += PAGE_2M;
	UT_ASSERTeq(pmem_unmap(ret, PAGE_2M), 0);

	ret += PAGE_2M;
	UT_ASSERTeq(pmem_unmap(ret, PAGE_2M - 1), 0);

	ret += PAGE_2M;
	UT_ASSERTne(pmem_unmap(ret, 0), 0);

	ret += PAGE_2M - 1;
	UT_ASSERTne(pmem_unmap(ret, PAGE_4K), 0);

	DONE(NULL);
}
