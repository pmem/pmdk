// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2023, Intel Corporation */

/*
 * blk_pool_lock.c -- unit test which checks whether it's possible to
 *                    simultaneously open the same blk pool
 */

#include "unittest.h"

static void
test_reopen(const char *path)
{
	PMEMblkpool *blk1 = pmemblk_create(path, 4096, PMEMBLK_MIN_POOL,
			S_IWUSR | S_IRUSR);
	if (!blk1)
		UT_FATAL("!create");

	PMEMblkpool *blk2 = pmemblk_open(path, 4096);
	if (blk2)
		UT_FATAL("pmemblk_open should not succeed");

	if (errno != EWOULDBLOCK)
		UT_FATAL("!pmemblk_open failed but for unexpected reason");

	pmemblk_close(blk1);

	blk2 = pmemblk_open(path, 4096);
	if (!blk2)
		UT_FATAL("pmemobj_open should succeed after close");

	pmemblk_close(blk2);

	UNLINK(path);
}

static void
test_open_in_different_process(int argc, char **argv, unsigned sleep)
{
	pid_t pid = fork();
	PMEMblkpool *blk;
	char *path = argv[1];

	if (pid < 0)
		UT_FATAL("fork failed");

	if (pid == 0) {
		/* child */
		if (sleep)
			usleep(sleep);
		while (os_access(path, R_OK))
			usleep(100 * 1000);

		blk = pmemblk_open(path, 4096);
		if (blk)
			UT_FATAL("pmemblk_open after fork should not succeed");

		if (errno != EWOULDBLOCK)
			UT_FATAL("!pmemblk_open after fork failed but for "
				"unexpected reason");

		exit(0);
	}

	blk = pmemblk_create(path, 4096, PMEMBLK_MIN_POOL,
		S_IWUSR | S_IRUSR);
	if (!blk)
		UT_FATAL("!create");

	int status;

	if (waitpid(pid, &status, 0) < 0)
		UT_FATAL("!waitpid failed");

	if (!WIFEXITED(status))
		UT_FATAL("child process failed");

	pmemblk_close(blk);

	UNLINK(path);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "blk_pool_lock");

	if (argc < 2)
		UT_FATAL("usage: %s path", argv[0]);

	if (argc == 2) {
		test_reopen(argv[1]);
		test_open_in_different_process(argc, argv, 0);
		for (unsigned i = 1; i < 100000; i *= 2)
			test_open_in_different_process(argc, argv, i);
	} else if (argc == 3) {
		PMEMblkpool *blk;
		/* 2nd arg used by windows for 2 process test */
		blk = pmemblk_open(argv[1], 4096);
		if (blk)
			UT_FATAL("pmemblk_open after create process should "
				"not succeed");

		if (errno != EWOULDBLOCK)
			UT_FATAL("!pmemblk_open after create process failed "
				"but for unexpected reason");
	}

	DONE(NULL);
}
