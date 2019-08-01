/*
 * Copyright 2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * obj_pool_open_mt.c -- multithreaded unit test for pool_open
 */

#include <errno.h>

#include "unittest.h"

/* more concurrency = good */
#define NTHREADS 64

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
		PTHREAD_CREATE(&th[i], 0, thread_oc, (void *)(uint64_t)i);

	/* The threads work here... */

	for (int i = 0; i < NTHREADS; i++) {
		void *retval;
		PTHREAD_JOIN(&th[i], &retval);
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
