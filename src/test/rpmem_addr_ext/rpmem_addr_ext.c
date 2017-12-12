/*
 * Copyright 2017, Intel Corporation
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
 * rpmem_addr_ext.c -- advanced unittest for invalid target formats
 */

#include "unittest.h"

#include "librpmem.h"
#include "pool_hdr.h"
#include "set.h"
#include "util.h"
#include "out.h"

#include "rpmem_common.h"
#include "rpmem_fip_common.h"

#define POOL_SIZE (8 * 1024 * 1024) /* 8 MiB */

#define NLANES	32

#define MAX_TARGET_LENGTH 256

/*
 * test_prepare -- prepare test environment
 */
static void
test_prepare()
{
	/*
	 * Till fix introduced to libfabric in pull request
	 * https://github.com/ofiwg/libfabric/pull/2551 misuse of errno value
	 * lead to SIGSEGV.
	 */
	errno = 0;
}

/*
 * test_create -- test case for creating remote pool
 */
static int
test_create(const char *target, void *pool)
{
	const char *pool_set = "invalid.poolset";
	unsigned nlanes = NLANES;
	struct rpmem_pool_attr pool_attr;
	memset(&pool_attr, 0, sizeof(pool_attr));

	RPMEMpool *rpp = rpmem_create(target, pool_set, pool, POOL_SIZE,
			&nlanes, &pool_attr);

	UT_ASSERTeq(rpp, NULL);

	return 0;
}

/*
 * test_open -- test case for opening remote pool
 */
static int
test_open(const char *target, void *pool)
{
	const char *pool_set = "invalid.poolset";

	unsigned nlanes = NLANES;
	struct rpmem_pool_attr pool_attr;
	memset(&pool_attr, 0, sizeof(pool_attr));

	RPMEMpool *rpp = rpmem_open(target, pool_set, pool, POOL_SIZE, &nlanes,
			&pool_attr);

	UT_ASSERTeq(rpp, NULL);

	return 0;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "rpmem_addr_ext");

	if (argc < 2)
		UT_FATAL("usage: rpmem_addr_ext <targets>");

	const char *targets_file_name = argv[1];
	char target[MAX_TARGET_LENGTH];
	void *pool = PAGEALIGNMALLOC(POOL_SIZE);

	FILE *targets_file = FOPEN(targets_file_name, "r");

	while (fgets(target, sizeof(target), targets_file)) {
		/* assume each target has new line at the end and remove it */
		target[strlen(target) - 1] = '\0';

		test_prepare();
		test_create(target, pool);

		test_prepare();
		test_open(target, pool);
	}

	FCLOSE(targets_file);
	FREE(pool);

	DONE(NULL);
}
