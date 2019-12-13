/*
 * Copyright 2015-2019, Intel Corporation
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
 * obj_pool.c -- unit test for pmemobj_create() and pmemobj_open()
 *
 * usage: obj_pool op path layout [poolsize mode]
 *
 * op can be:
 *   c - create
 *   o - open
 *
 * "poolsize" and "mode" arguments are ignored for "open"
 */
#include "unittest.h"
#include "../libpmemobj/obj.h"

#define MB ((size_t)1 << 20)

#define USER_DATA_V 123456789ULL

static void
pool_create(const char *path, const char *layout, size_t poolsize,
	unsigned mode)
{
	PMEMobjpool *pop = pmemobj_create(path, layout, poolsize, mode);

	if (pop == NULL)
		UT_OUT("!%s: pmemobj_create: %s", path, pmemobj_errormsg());
	else {
		/* Test pmemobj_(get/set)_user data */
		pmemobj_set_user_data(pop, (void *) USER_DATA_V);
		UT_ASSERT((void *) USER_DATA_V == pmemobj_get_user_data(pop));

		os_stat_t stbuf;
		STAT(path, &stbuf);

		UT_OUT("%s: file size %zu mode 0%o",
				path, stbuf.st_size,
				stbuf.st_mode & 0777);

		pmemobj_close(pop);

		int result = pmemobj_check(path, layout);

		if (result < 0)
			UT_OUT("!%s: pmemobj_check", path);
		else if (result == 0)
			UT_OUT("%s: pmemobj_check: not consistent", path);
	}
}

static void
pool_open(const char *path, const char *layout)
{
	PMEMobjpool *pop = pmemobj_open(path, layout);
	if (pop == NULL)
		UT_OUT("!%s: pmemobj_open: %s", path, pmemobj_errormsg());
	else {
		UT_OUT("%s: pmemobj_open: Success", path);
		pmemobj_close(pop);
	}
}

static void
test_fault_injection(const char *path, const char *layout, size_t poolsize,
		unsigned mode)
{
	if (!pmemobj_fault_injection_enabled())
		return;

	pmemobj_inject_fault_at(PMEM_MALLOC, 1, "tx_params_new");
	PMEMobjpool *pop = pmemobj_create(path, layout, poolsize, mode);
	UT_ASSERTeq(pop, NULL);
	UT_ASSERTeq(errno, ENOMEM);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pool");

	if (argc < 4)
		UT_FATAL("usage: %s op path layout [poolsize mode]", argv[0]);

	char *layout = NULL;
	size_t poolsize;
	unsigned mode;

	if (strcmp(argv[3], "EMPTY") == 0)
		layout = "";
	else if (strcmp(argv[3], "NULL") != 0)
		layout = argv[3];

	switch (argv[1][0]) {
	case 'c':
		poolsize = strtoull(argv[4], NULL, 0) * MB; /* in megabytes */
		mode = strtoul(argv[5], NULL, 8);

		pool_create(argv[2], layout, poolsize, mode);
		break;

	case 'o':
		pool_open(argv[2], layout);
		break;
	case 'f':
		os_setenv("PMEMOBJ_CONF", "invalid-query", 1);
		pool_open(argv[2], layout);
		os_unsetenv("PMEMOBJ_CONF");
		pool_open(argv[2], layout);
		break;
	case 't':
		poolsize = strtoull(argv[4], NULL, 0) * MB; /* in megabytes */
		mode = strtoul(argv[5], NULL, 8);

		test_fault_injection(argv[2], layout, poolsize, mode);
		break;
	default:
		UT_FATAL("unknown operation");
	}

	DONE(NULL);
}
