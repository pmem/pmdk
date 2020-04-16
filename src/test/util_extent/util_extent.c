// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * util_extent.c -- unit test for the linux fs extent query API
 *
 */

#include "unittest.h"
#include "extent.h"
#include "libpmem2.h"

/*
 * test_size -- test if sum of all file's extents sums up to the file's size
 */
static void
test_size(int fd, size_t size)
{
	size_t total_length = 0;

	struct extents *exts = NULL;

	UT_ASSERTeq(pmem2_extents_create_get(fd, &exts), 0);
	UT_ASSERT(exts->extents_count > 0);
	UT_OUT("exts->extents_count: %u", exts->extents_count);

	unsigned e;
	for (e = 0; e < exts->extents_count; e++)
		total_length += exts->extents[e].length;

	pmem2_extents_destroy(&exts);

	UT_ASSERTeq(total_length, size);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_extent");

	if (argc != 3)
		UT_FATAL("usage: %s file file-size", argv[0]);

	const char *file = argv[1];
	long long isize = atoi(argv[2]);
	UT_ASSERT(isize > 0);
	size_t size = (size_t)isize;

	int fd = OPEN(file, O_RDONLY);

	test_size(fd, size);

	close(fd);

	DONE(NULL);
}
