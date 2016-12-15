/*
 * Copyright 2014-2017, Intel Corporation
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
 * cto_multiple_pools.c -- unit test for cto_multiple_pools
 *
 * usage: cto_multiple_pools directory npools [nthreads]
 */

#include "unittest.h"

#define NREPEATS 10

static PMEMctopool **Pools;
static int Npools;
static const char *Dir;

static void *
thread_func(void *arg)
{
	int start_idx = *(int *)arg;
	char filename[PATH_MAX + 50]; /* reserve some space for pool id */

	for (int repeat = 0; repeat < NREPEATS; ++repeat) {
		for (int idx = 0; idx < Npools; ++idx) {
			int pool_id = start_idx + idx;

			/* XXX - buffer overflow */
			sprintf(filename, "%s/pool%d", Dir, pool_id);

			/* delete old pool with the same id if exists */
			if (Pools[pool_id] != NULL) {
				pmemcto_close(Pools[pool_id]);
				Pools[pool_id] = NULL;
				UNLINK(filename);
			}

			Pools[pool_id] = pmemcto_create(filename, "test",
				PMEMCTO_MIN_POOL, 0600);
			UT_ASSERTne(Pools[pool_id], NULL);

			void *ptr = pmemcto_malloc(Pools[pool_id], sizeof(int));
			UT_ASSERTne(ptr, NULL);

			pmemcto_free(Pools[pool_id], ptr);
		}
	}

	return NULL;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "cto_multiple_pools");

	if (argc < 4)
		UT_FATAL("usage: %s directory npools nthreads", argv[0]);

	Dir = argv[1];
	Npools = atoi(argv[2]);

	int nthreads = atoi(argv[3]);

	UT_OUT("create %d pools in %d thread(s)", Npools, nthreads);

	Pools = CALLOC(Npools * nthreads, sizeof(VMEM *));
	os_thread_t *threads = CALLOC(nthreads, sizeof(os_thread_t));

	int *pool_idx = CALLOC(nthreads, sizeof(int));
	UT_ASSERTne(pool_idx, NULL);

	/* create and destroy pools multiple times */
	for (int t = 0; t < nthreads; t++) {
		pool_idx[t] = Npools * t;
		PTHREAD_CREATE(&threads[t], NULL, thread_func, &pool_idx[t]);
	}

	for (int t = 0; t < nthreads; t++)
		PTHREAD_JOIN(&threads[t], NULL);

	for (int i = 0; i < Npools; ++i) {
		if (Pools[i] != NULL) {
			pmemcto_close(Pools[i]);
			Pools[i] = NULL;
		}
	}

	FREE(Pools);
	FREE(threads);
	FREE(pool_idx);

	DONE(NULL);
}
