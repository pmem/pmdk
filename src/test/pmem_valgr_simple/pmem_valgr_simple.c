// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2016, Intel Corporation */

/*
 * pmem_valgr_simple.c -- simple unit test using pmemcheck
 *
 * usage: pmem_valgr_simple file
 */

#include "unittest.h"

int
main(int argc, char *argv[])
{
	size_t mapped_len;
	char *dest;
	int is_pmem;

	START(argc, argv, "pmem_valgr_simple");

	if (argc != 4)
		UT_FATAL("usage: %s file offset length", argv[0]);

	int dest_off = atoi(argv[2]);
	size_t bytes = strtoul(argv[3], NULL, 0);

	dest = pmem_map_file(argv[1], 0, 0, 0, &mapped_len, &is_pmem);
	if (dest == NULL)
		UT_FATAL("!Could not mmap %s\n", argv[1]);

	/* these will not be made persistent */
	*(int *)dest = 4;

	/* this will be made persistent */
	uint64_t *tmp64dst = (void *)((uintptr_t)dest + 4096);
	*tmp64dst = 50;

	if (is_pmem) {
		pmem_persist(tmp64dst, sizeof(*tmp64dst));
	} else {
		UT_ASSERTeq(pmem_msync(tmp64dst, sizeof(*tmp64dst)), 0);
	}

	uint16_t *tmp16dst = (void *)((uintptr_t)dest + 1024);
	*tmp16dst = 21;
	/* will appear as flushed/fenced in valgrind log */
	pmem_flush(tmp16dst, sizeof(*tmp16dst));

	/* shows strange behavior of memset in some cases */
	memset(dest + dest_off, 0, bytes);

	UT_ASSERTeq(pmem_unmap(dest, mapped_len), 0);

	DONE(NULL);
}
