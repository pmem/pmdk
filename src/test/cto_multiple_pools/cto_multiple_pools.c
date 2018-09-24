/*
 * Copyright 2014-2018, Intel Corporation
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
 * usage: cto_multiple_pools directory mode npools nthreads
 */

#include "unittest.h"

#define NREPEATS 10

static PMEMctopool **Pools;
static unsigned Npools;
static const char *Dir;
static unsigned *Pool_idx;
static os_thread_t *Threads;

static void *
thread_func_open(void *arg)
{
	unsigned start_idx = *(unsigned *)arg;

	size_t len = strlen(Dir) + 50;	/* reserve some space for pool id */
	char *filename = MALLOC(sizeof(*filename) * len);

	for (int repeat = 0; repeat < NREPEATS; ++repeat) {
		for (unsigned idx = 0; idx < Npools; ++idx) {
			unsigned pool_id = start_idx + idx;

			snprintf(filename, len, "%s" OS_DIR_SEP_STR "pool%d",
				Dir, pool_id);
			UT_OUT("%s", filename);

			Pools[pool_id] = pmemcto_open(filename, "test");
			UT_ASSERTne(Pools[pool_id], NULL);

			void *ptr = pmemcto_malloc(Pools[pool_id], sizeof(int));
			UT_OUT("pcp %p ptr %p", Pools[pool_id], ptr);
			UT_ASSERTne(ptr, NULL);

			pmemcto_free(Pools[pool_id], ptr);

			pmemcto_close(Pools[pool_id]);
		}
	}

	FREE(filename);
	return NULL;
}

static void *
thread_func_create(void *arg)
{
	unsigned start_idx = *(unsigned *)arg;

	size_t len = strlen(Dir) + 50;	/* reserve some space for pool id */
	char *filename = MALLOC(sizeof(*filename) * len);

	for (int repeat = 0; repeat < NREPEATS; ++repeat) {
		for (unsigned idx = 0; idx < Npools; ++idx) {
			unsigned pool_id = start_idx + idx;

			snprintf(filename, len, "%s" OS_DIR_SEP_STR "pool%d",
				Dir, pool_id);
			UT_OUT("%s", filename);

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

	FREE(filename);
	return NULL;
}

static void
test_open(unsigned nthreads)
{
	size_t len = strlen(Dir) + 50;	/* reserve some space for pool id */
	char *filename = MALLOC(sizeof(*filename) * len);

	/* create all the pools */
	for (unsigned pool_id = 0; pool_id < Npools * nthreads; ++pool_id) {
		snprintf(filename, len, "%s" OS_DIR_SEP_STR "pool%d",
				Dir, pool_id);
		UT_OUT("%s", filename);

		Pools[pool_id] = pmemcto_create(filename, "test",
			PMEMCTO_MIN_POOL, 0600);
		UT_ASSERTne(Pools[pool_id], NULL);
	}

	for (unsigned pool_id = 0; pool_id < Npools * nthreads; ++pool_id)
		pmemcto_close(Pools[pool_id]);

	for (unsigned t = 0; t < nthreads; t++) {
		Pool_idx[t] = Npools * t;
		PTHREAD_CREATE(&Threads[t], NULL, thread_func_open,
				&Pool_idx[t]);
	}

	for (unsigned t = 0; t < nthreads; t++)
		PTHREAD_JOIN(&Threads[t], NULL);

	FREE(filename);
}

static void
test_create(unsigned nthreads)
{
	/* create and destroy pools multiple times */
	for (unsigned t = 0; t < nthreads; t++) {
		Pool_idx[t] = Npools * t;
		PTHREAD_CREATE(&Threads[t], NULL, thread_func_create,
				&Pool_idx[t]);
	}

	for (unsigned t = 0; t < nthreads; t++)
		PTHREAD_JOIN(&Threads[t], NULL);

	for (unsigned i = 0; i < Npools * nthreads; ++i) {
		if (Pools[i] != NULL) {
			pmemcto_close(Pools[i]);
			Pools[i] = NULL;
		}
	}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "cto_multiple_pools");

	if (argc < 4)
		UT_FATAL("usage: %s directory mode npools nthreads", argv[0]);

	Dir = argv[1];
	char mode = argv[2][0];
	Npools = ATOU(argv[3]);
	unsigned nthreads = ATOU(argv[4]);

	UT_OUT("create %d pools in %d thread(s)", Npools, nthreads);

	Pools = CALLOC(Npools * nthreads, sizeof(Pools[0]));
	Threads = CALLOC(nthreads, sizeof(Threads[0]));
	Pool_idx = CALLOC(nthreads, sizeof(Pool_idx[0]));

	switch (mode) {
	case 'o':
		test_open(nthreads);
		break;

	case 'c':
		test_create(nthreads);
		break;

	default:
		UT_FATAL("unknown mode");
	}

	FREE(Pools);
	FREE(Threads);
	FREE(Pool_idx);

	DONE(NULL);
}
