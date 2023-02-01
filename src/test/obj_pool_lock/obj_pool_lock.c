// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2023, Intel Corporation */

/*
 * obj_pool_lock.c -- unit test which checks whether it's possible to
 *                    simultaneously open the same obj pool
 */

#include "unittest.h"
#define LAYOUT "layout"

static void
test_reopen(const char *path)
{
	PMEMobjpool *pop1 = pmemobj_create(path, LAYOUT, PMEMOBJ_MIN_POOL,
			S_IWUSR | S_IRUSR);
	if (!pop1)
		UT_FATAL("!create");

	PMEMobjpool *pop2 = pmemobj_open(path, LAYOUT);
	if (pop2)
		UT_FATAL("pmemobj_open should not succeed");

	if (errno != EWOULDBLOCK)
		UT_FATAL("!pmemobj_open failed but for unexpected reason");

	pmemobj_close(pop1);

	pop2 = pmemobj_open(path, LAYOUT);
	if (!pop2)
		UT_FATAL("pmemobj_open should succeed after close");

	pmemobj_close(pop2);

	UNLINK(path);
}

static void
test_open_in_different_process(int argc, char **argv, unsigned sleep)
{
	pid_t pid = fork();
	PMEMobjpool *pop;
	char *path = argv[1];

	if (pid < 0)
		UT_FATAL("fork failed");

	if (pid == 0) {
		/* child */
		if (sleep)
			usleep(sleep);
		while (os_access(path, R_OK))
			usleep(100 * 1000);

		pop = pmemobj_open(path, LAYOUT);
		if (pop)
			UT_FATAL("pmemobj_open after fork should not succeed");

		if (errno != EWOULDBLOCK)
			UT_FATAL("!pmemobj_open after fork failed but for "
				"unexpected reason");

		exit(0);
	}

	pop = pmemobj_create(path, LAYOUT, PMEMOBJ_MIN_POOL,
		S_IWUSR | S_IRUSR);
	if (!pop)
		UT_FATAL("!create");

	int status;

	if (waitpid(pid, &status, 0) < 0)
		UT_FATAL("!waitpid failed");

	if (!WIFEXITED(status))
		UT_FATAL("child process failed");

	pmemobj_close(pop);

	UNLINK(path);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pool_lock");

	if (argc < 2)
		UT_FATAL("usage: %s path", argv[0]);

	if (argc == 2) {
		test_reopen(argv[1]);

		test_open_in_different_process(argc, argv, 0);
		for (unsigned i = 1; i < 100000; i *= 2)
			test_open_in_different_process(argc, argv, i);
	} else if (argc == 3) {
		PMEMobjpool *pop;
		/* 2nd arg used by windows for 2 process test */
		pop = pmemobj_open(argv[1], LAYOUT);
		if (pop)
			UT_FATAL("pmemobj_open after create process should "
				"not succeed");

		if (errno != EWOULDBLOCK)
			UT_FATAL("!pmemobj_open after create process failed "
				"but for unexpected reason");
	}

	DONE(NULL);
}
