// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2023, Intel Corporation */

/*
 * pmem_map_file_trunc.c -- test for mapping specially crafted files
 *
 * usage: pmem_map_file_trunc file
 */

#include "unittest.h"
#define EXPECTED_SIZE (4 * 1024)

#define FILL_CHAR 0x1a

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem_map_file_trunc");

	if (argc < 2)
		UT_FATAL("not enough args");

	size_t mapped;
	int ispmem;
	char *p;
	os_stat_t st;

	p = pmem_map_file(argv[1], EXPECTED_SIZE, PMEM_FILE_CREATE, 0644,
		&mapped, &ispmem);
	UT_ASSERT(p);
	UT_ASSERTeq(mapped, EXPECTED_SIZE);

	p[EXPECTED_SIZE - 1] = FILL_CHAR;
	pmem_persist(&p[EXPECTED_SIZE - 1], 1);

	pmem_unmap(p, EXPECTED_SIZE);

	STAT(argv[1], &st);
	UT_ASSERTeq(st.st_size, EXPECTED_SIZE);

	p = pmem_map_file(argv[1], 0, 0, 0644, &mapped, &ispmem);
	UT_ASSERT(p);
	UT_ASSERTeq(mapped, EXPECTED_SIZE);
	UT_ASSERTeq(p[EXPECTED_SIZE - 1], FILL_CHAR);

	pmem_unmap(p, EXPECTED_SIZE);

	STAT(argv[1], &st);
	UT_ASSERTeq(st.st_size, EXPECTED_SIZE);

	DONE(NULL);
}
