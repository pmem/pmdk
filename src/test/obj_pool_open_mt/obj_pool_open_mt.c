// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * obj_pool_open_mt.c -- multithreaded unit test for pool_open
 */

#include <errno.h>

#include "unittest.h"

/* more concurrency = good */
#define NTHREADS 16

#define POOLSIZE (16 * 1048576)

static unsigned long niter;
static const char *path;

static void *
thread_oc(void *arg)
{
	unsigned tid = (unsigned)(uint64_t)arg;

	char pname[PATH_MAX];
	snprintf(pname, sizeof(pname), "%s/open_mt_%02u", path, tid);

	PMEMobjpool *p = pmemobj_create(pname, "", POOLSIZE, 0666);
	UT_ASSERT(p);

	/* use the new pool */
	PMEMoid o;
	UT_ASSERT(!pmemobj_zalloc(p, &o, 64, 0));
	int *ptr = pmemobj_direct(o);
	UT_ASSERT(ptr);
	UT_ASSERT(! *ptr);

	pmemobj_close(p);

	for (uint64_t count = 0; count < niter; count++) {
		p = pmemobj_open(pname, "");
		UT_ASSERT(p);
		pmemobj_close(p);
	}

	UNLINK(pname);

	return NULL;
}

static void
test()
{
	os_thread_t th[NTHREADS];

	for (int i = 0; i < NTHREADS; i++)
		THREAD_CREATE(&th[i], 0, thread_oc, (void *)(uint64_t)i);

	/* The threads work here... */

	for (int i = 0; i < NTHREADS; i++) {
		void *retval;
		THREAD_JOIN(&th[i], &retval);
	}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pool_open_mt");

	if (argc != 3)
		UT_FATAL("usage: %s path niter", argv[0]);

	path = argv[1];
	niter = ATOUL(argv[2]);
	if (!niter || niter == ULONG_MAX)
		UT_FATAL("%s: bad iteration count '%s'", argv[0], argv[2]);

	test();

	DONE(NULL);
}
