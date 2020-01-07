// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2017, Intel Corporation */

/*
 * blk_nblock.c -- unit test for pmemblk_nblock()
 *
 * usage: blk_nblock bsize:file...
 *
 */

#include "unittest.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "blk_nblock");

	if (argc < 2)
		UT_FATAL("usage: %s bsize:file...", argv[0]);

	/* map each file argument with the given map type */
	for (int arg = 1; arg < argc; arg++) {
		char *fname;
		size_t bsize = strtoul(argv[arg], &fname, 0);
		if (*fname != ':')
			UT_FATAL("usage: %s bsize:file...", argv[0]);
		fname++;

		PMEMblkpool *handle;
		handle = pmemblk_create(fname, bsize, 0, S_IWUSR | S_IRUSR);
		if (handle == NULL) {
			UT_OUT("!%s: pmemblk_create", fname);
		} else {
			UT_OUT("%s: block size %zu usable blocks: %zu",
					fname, bsize, pmemblk_nblock(handle));
			UT_ASSERTeq(pmemblk_bsize(handle), bsize);
			pmemblk_close(handle);
			int result = pmemblk_check(fname, bsize);
			if (result < 0)
				UT_OUT("!%s: pmemblk_check", fname);
			else if (result == 0)
				UT_OUT("%s: pmemblk_check: not consistent",
						fname);
			else {
				UT_ASSERTeq(pmemblk_check(fname, bsize + 1),
						-1);
				UT_ASSERTeq(pmemblk_check(fname, 0), 1);

				handle = pmemblk_open(fname, 0);
				UT_ASSERTeq(pmemblk_bsize(handle), bsize);
				pmemblk_close(handle);
			}
		}
	}

	DONE(NULL);
}
