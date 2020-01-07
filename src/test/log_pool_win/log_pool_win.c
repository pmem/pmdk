// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */

/*
 * log_pool.c -- unit test for pmemlog_create() and pmemlog_open()
 *
 * usage: log_pool op path [poolsize mode]
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
pool_create(const wchar_t *path, size_t poolsize, unsigned mode)
{
	char *upath = ut_toUTF8(path);
	PMEMlogpool *plp = pmemlog_createW(path, poolsize, mode);

	if (plp == NULL)
		UT_OUT("!%s: pmemlog_create", upath);
	else {
		os_stat_t stbuf;
		STATW(path, &stbuf);

		UT_OUT("%s: file size %zu usable space %zu mode 0%o",
			upath, stbuf.st_size,
				pmemlog_nbyte(plp),
				stbuf.st_mode & 0777);

		pmemlog_close(plp);

		int result = pmemlog_checkW(path);

		if (result < 0)
			UT_OUT("!%s: pmemlog_check", upath);
		else if (result == 0)
			UT_OUT("%s: pmemlog_check: not consistent", upath);
	}
	free(upath);
}

static void
pool_open(const wchar_t *path)
{
	char *upath = ut_toUTF8(path);

	PMEMlogpool *plp = pmemlog_openW(path);
	if (plp == NULL)
		UT_OUT("!%s: pmemlog_open", upath);
	else {
		UT_OUT("%s: pmemlog_open: Success", upath);
		pmemlog_close(plp);
	}
	free(upath);
}

int
wmain(int argc, wchar_t *argv[])
{
	STARTW(argc, argv, "log_pool_win");

	if (argc < 3)
		UT_FATAL("usage: %s op path [poolsize mode]",
			ut_toUTF8(argv[0]));

	size_t poolsize;
	unsigned mode;

	switch (argv[1][0]) {
	case 'c':
		poolsize = wcstoul(argv[3], NULL, 0) * MB; /* in megabytes */
		mode = wcstoul(argv[4], NULL, 8);

		pool_create(argv[2], poolsize, mode);
		break;

	case 'o':
		pool_open(argv[2]);
		break;

	default:
		UT_FATAL("unknown operation");
	}

	DONEW(NULL);
}
