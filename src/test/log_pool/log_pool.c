// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

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
pool_create(const char *path, size_t poolsize, unsigned mode)
{
	PMEMlogpool *plp = pmemlog_create(path, poolsize, mode);

	if (plp == NULL)
		UT_OUT("!%s: pmemlog_create", path);
	else {
		os_stat_t stbuf;
		STAT(path, &stbuf);

		UT_OUT("%s: file size %zu usable space %zu mode 0%o",
				path, stbuf.st_size,
				pmemlog_nbyte(plp),
				stbuf.st_mode & 0777);

		pmemlog_close(plp);

		int result = pmemlog_check(path);

		if (result < 0)
			UT_OUT("!%s: pmemlog_check", path);
		else if (result == 0)
			UT_OUT("%s: pmemlog_check: not consistent", path);
	}
}

static void
pool_open(const char *path)
{
	PMEMlogpool *plp = pmemlog_open(path);
	if (plp == NULL)
		UT_OUT("!%s: pmemlog_open", path);
	else {
		UT_OUT("%s: pmemlog_open: Success", path);
		pmemlog_close(plp);
	}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "log_pool");

	if (argc < 3)
		UT_FATAL("usage: %s op path [poolsize mode]", argv[0]);

	size_t poolsize;
	unsigned mode;

	switch (argv[1][0]) {
	case 'c':
		poolsize = strtoul(argv[3], NULL, 0) * MB; /* in megabytes */
		mode = strtoul(argv[4], NULL, 8);

		pool_create(argv[2], poolsize, mode);
		break;

	case 'o':
		pool_open(argv[2]);
		break;

	default:
		UT_FATAL("unknown operation");
	}

	DONE(NULL);
}
