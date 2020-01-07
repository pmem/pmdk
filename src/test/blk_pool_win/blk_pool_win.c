// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */

/*
 * blk_pool_win.c -- unit test for pmemblk_create() and pmemblk_open()
 *
 * usage: blk_pool_win op path bsize [poolsize mode]
 *
 * op can be:
 *   c - create
 *   o - open
 *
 * "poolsize" and "mode" arguments are ignored for "open"
 */
#include "unittest.h"

#define MB ((size_t)1 << 20)

static void
pool_create(const wchar_t *path, size_t bsize, size_t poolsize, unsigned mode)
{
	char *upath = ut_toUTF8(path);
	UT_ASSERTne(upath, NULL);

	PMEMblkpool *pbp = pmemblk_createW(path, bsize, poolsize, mode);
	if (pbp == NULL)
		UT_OUT("!%s: pmemblk_create", upath);
	else {
		os_stat_t stbuf;
		STATW(path, &stbuf);

		UT_OUT("%s: file size %zu usable blocks %zu mode 0%o",
				upath, stbuf.st_size,
				pmemblk_nblock(pbp),
				stbuf.st_mode & 0777);

		pmemblk_close(pbp);

		int result = pmemblk_checkW(path, bsize);

		if (result < 0)
			UT_OUT("!%s: pmemblk_check", upath);
		else if (result == 0)
			UT_OUT("%s: pmemblk_check: not consistent", upath);
		else
			UT_ASSERTeq(pmemblk_checkW(path, bsize * 2), -1);

		free(upath);
	}
}

static void
pool_open(const wchar_t *path, size_t bsize)
{
	char *upath = ut_toUTF8(path);
	UT_ASSERTne(upath, NULL);

	PMEMblkpool *pbp = pmemblk_openW(path, bsize);
	if (pbp == NULL)
		UT_OUT("!%s: pmemblk_open", upath);
	else {
		UT_OUT("%s: pmemblk_open: Success", upath);
		pmemblk_close(pbp);
	}
	free(upath);
}

int
wmain(int argc, wchar_t *argv[])
{
	STARTW(argc, argv, "blk_pool_win");

	if (argc < 4)
		UT_FATAL("usage: %s op path bsize [poolsize mode]",
			ut_toUTF8(argv[0]));

	size_t bsize = wcstoul(argv[3], NULL, 0);
	size_t poolsize;
	unsigned mode;

	switch (argv[1][0]) {
	case 'c':
		poolsize = wcstoul(argv[4], NULL, 0) * MB; /* in megabytes */
		mode = wcstoul(argv[5], NULL, 8);

		pool_create(argv[2], bsize, poolsize, mode);
		break;

	case 'o':
		pool_open(argv[2], bsize);
		break;

	default:
		UT_FATAL("unknown operation");
	}

	DONEW(NULL);
}
