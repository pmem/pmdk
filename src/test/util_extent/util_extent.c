// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * util_extent.c -- unit test for the linux fs extent query API
 *
 */

#include "unittest.h"
#include "extent.h"

/*
 * test_size -- test if sum of all file's extents sums up to the file's size
 */
static void
test_size(const char *path, size_t size)
{
	size_t total_length = 0;

	struct extents *exts = MALLOC(sizeof(struct extents));

	UT_ASSERT(os_extents_count(path, exts) >= 0);

	UT_OUT("exts->extents_count: %u", exts->extents_count);

	if (exts->extents_count > 0) {
		exts->extents = MALLOC(exts->extents_count *
							sizeof(struct extent));

		UT_ASSERTeq(os_extents_get(path, exts), 0);

		unsigned e;
		for (e = 0; e < exts->extents_count; e++)
			total_length += exts->extents[e].length;

		FREE(exts->extents);
	}

	FREE(exts);

	UT_ASSERTeq(total_length, size);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_extent");

	if (argc != 3)
		UT_FATAL("usage: %s file file-size", argv[0]);

	long long isize = atoi(argv[2]);
	UT_ASSERT(isize > 0);
	size_t size = (size_t)isize;

	test_size(argv[1], size);

	DONE(NULL);
}
