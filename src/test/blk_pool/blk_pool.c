// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */

/*
 * blk_pool.c -- unit test for pmemblk_create() and pmemblk_open()
 *
 * usage: blk_pool op path bsize [poolsize mode]
 *
 * op can be:
 *   c - create
 *   o - open
 *   f - do fault injection
 *
 * "poolsize" and "mode" arguments are ignored for "open"
 */
#include "unittest.h"
#include "../libpmemblk/blk.h"

#define MB ((size_t)1 << 20)

static void
do_fault_injection(const char *path, size_t bsize,
		size_t poolsize, unsigned mode)
{
	if (!pmemblk_fault_injection_enabled())
		return;

	pmemblk_inject_fault_at(PMEM_MALLOC, 1, "blk_runtime_init");
	PMEMblkpool *pbp = pmemblk_create(path, bsize, poolsize, mode);
	UT_ASSERTeq(pbp, NULL);
	UT_ASSERTeq(errno, ENOMEM);
}

static void
pool_create(const char *path, size_t bsize, size_t poolsize, unsigned mode)
{
	PMEMblkpool *pbp = pmemblk_create(path, bsize, poolsize, mode);

	if (pbp == NULL)
		UT_OUT("!%s: pmemblk_create", path);
	else {
		os_stat_t stbuf;
		STAT(path, &stbuf);

		UT_OUT("%s: file size %zu usable blocks %zu mode 0%o",
				path, stbuf.st_size,
				pmemblk_nblock(pbp),
				stbuf.st_mode & 0777);

		pmemblk_close(pbp);

		int result = pmemblk_check(path, bsize);

		if (result < 0)
			UT_OUT("!%s: pmemblk_check", path);
		else if (result == 0)
			UT_OUT("%s: pmemblk_check: not consistent", path);
		else
			UT_ASSERTeq(pmemblk_check(path, bsize * 2), -1);
	}
}

static void
pool_open(const char *path, size_t bsize)
{
	PMEMblkpool *pbp = pmemblk_open(path, bsize);
	if (pbp == NULL)
		UT_OUT("!%s: pmemblk_open", path);
	else {
		UT_OUT("%s: pmemblk_open: Success", path);
		pmemblk_close(pbp);
	}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "blk_pool");

	if (argc < 4)
		UT_FATAL("usage: %s op path bsize [poolsize mode]", argv[0]);

	size_t bsize = strtoul(argv[3], NULL, 0);
	size_t poolsize;
	unsigned mode;

	switch (argv[1][0]) {
	case 'c':
		poolsize = strtoul(argv[4], NULL, 0) * MB; /* in megabytes */
		mode = strtoul(argv[5], NULL, 8);

		pool_create(argv[2], bsize, poolsize, mode);
		break;

	case 'o':
		pool_open(argv[2], bsize);
		break;
	case 'f':
		poolsize = strtoul(argv[4], NULL, 0) * MB; /* in megabytes */
		mode = strtoul(argv[5], NULL, 8);

		do_fault_injection(argv[2], bsize, poolsize, mode);
		break;

	default:
		UT_FATAL("unknown operation");
	}

	DONE(NULL);
}
