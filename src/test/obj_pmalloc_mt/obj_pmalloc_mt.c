/*
 * Copyright 2015-2018, Intel Corporation
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
 * obj_pmalloc_mt.c -- multithreaded test of allocator
 */
#include <stdint.h>

#include "obj.h"
#include "pmalloc.h"
#include "unittest.h"

#define MAX_THREADS 32
#define MAX_OPS_PER_THREAD 1000
#define ALLOC_SIZE 104
#define REALLOC_SIZE (ALLOC_SIZE * 3)
#define MIX_RERUNS 2

#define CHUNKSIZE (1 << 18)
#define CHUNKS_PER_THREAD 3

int Threads;
int Ops_per_thread;
int Tx_per_thread;

struct root {
	uint64_t offs[MAX_THREADS][MAX_OPS_PER_THREAD];
};

struct worker_args {
	PMEMobjpool *pop;
	struct root *r;
	int idx;
};

static void *
alloc_worker(void *arg)
{
	struct worker_args *a = arg;

	for (int i = 0; i < Ops_per_thread; ++i) {
		pmalloc(a->pop, &a->r->offs[a->idx][i], ALLOC_SIZE, 0, 0);
		UT_ASSERTne(a->r->offs[a->idx][i], 0);
	}

	return NULL;
}

static void *
realloc_worker(void *arg)
{
	struct worker_args *a = arg;

	for (int i = 0; i < Ops_per_thread; ++i) {
		prealloc(a->pop, &a->r->offs[a->idx][i], REALLOC_SIZE, 0, 0);
		UT_ASSERTne(a->r->offs[a->idx][i], 0);
	}

	return NULL;
}

static void *
free_worker(void *arg)
{
	struct worker_args *a = arg;

	for (int i = 0; i < Ops_per_thread; ++i) {
		pfree(a->pop, &a->r->offs[a->idx][i]);
		UT_ASSERTeq(a->r->offs[a->idx][i], 0);
	}

	return NULL;
}

static void *
mix_worker(void *arg)
{
	struct worker_args *a = arg;

	/*
	 * The mix scenario is ran twice to increase the chances of run
	 * contention.
	 */
	for (int j = 0; j < MIX_RERUNS; ++j) {
		for (int i = 0; i < Ops_per_thread; ++i) {
			pmalloc(a->pop, &a->r->offs[a->idx][i],
				ALLOC_SIZE, 0, 0);
			UT_ASSERTne(a->r->offs[a->idx][i], 0);
		}

		for (int i = 0; i < Ops_per_thread; ++i) {
			pfree(a->pop, &a->r->offs[a->idx][i]);
			UT_ASSERTeq(a->r->offs[a->idx][i], 0);
		}
	}

	return NULL;
}

static void *
tx_worker(void *arg)
{
	struct worker_args *a = arg;

	/*
	 * Allocate objects until exhaustion, once that happens the transaction
	 * will automatically abort and all of the objects will be freed.
	 */
	TX_BEGIN(a->pop) {
		for (int n = 0; ; ++n) { /* this is NOT an infinite loop */
			pmemobj_tx_alloc(ALLOC_SIZE, a->idx);
			if (Ops_per_thread != MAX_OPS_PER_THREAD &&
			    n == Ops_per_thread) {
				pmemobj_tx_abort(0);
			}
		}
	} TX_END

	return NULL;
}

static void *
tx3_worker(void *arg)
{
	struct worker_args *a = arg;

	/*
	 * Allocate N objects, abort, repeat M times. Should reveal issues in
	 * transaction abort handling.
	 */
	for (int n = 0; n < Tx_per_thread; ++n) {
		TX_BEGIN(a->pop) {
			for (int i = 0; i < Ops_per_thread; ++i) {
				pmemobj_tx_alloc(ALLOC_SIZE, a->idx);
			}
			pmemobj_tx_abort(EINVAL);
		} TX_END
	}

	return NULL;
}


static void *
alloc_free_worker(void *arg)
{
	struct worker_args *a = arg;

	PMEMoid oid;
	for (int i = 0; i < Ops_per_thread; ++i) {
		int err = pmemobj_alloc(a->pop, &oid, ALLOC_SIZE,
				0, NULL, NULL);
		UT_ASSERTeq(err, 0);
		pmemobj_free(&oid);
	}

	return NULL;
}

#define OPS_PER_TX 10
#define STEP 8
#define TEST_LANES 4

static void *
tx2_worker(void *arg)
{
	struct worker_args *a = arg;

	for (int n = 0; n < Tx_per_thread; ++n) {
		PMEMoid oids[OPS_PER_TX];
		TX_BEGIN(a->pop) {
			for (int i = 0; i < OPS_PER_TX; ++i) {
				oids[i] = pmemobj_tx_alloc(ALLOC_SIZE, a->idx);
				for (int j = 0; j < ALLOC_SIZE; j += STEP) {
					pmemobj_tx_add_range(oids[i], j, STEP);
				}
			}
		} TX_END

		TX_BEGIN(a->pop) {
			for (int i = 0; i < OPS_PER_TX; ++i)
				pmemobj_tx_free(oids[i]);
		} TX_ONABORT {
			UT_ASSERT(0);
		} TX_END
	}

	return NULL;
}

static void
run_worker(void *(worker_func)(void *arg), struct worker_args args[])
{
	os_thread_t t[MAX_THREADS];

	for (int i = 0; i < Threads; ++i)
		os_thread_create(&t[i], NULL, worker_func, &args[i]);

	for (int i = 0; i < Threads; ++i)
		os_thread_join(&t[i], NULL);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pmalloc_mt");

	if (argc != 5)
		UT_FATAL("usage: %s <threads> <ops/t> <tx/t> [file]", argv[0]);

	PMEMobjpool *pop;

	Threads = atoi(argv[1]);
	if (Threads > MAX_THREADS)
		UT_FATAL("Threads %d > %d", Threads, MAX_THREADS);
	Ops_per_thread = atoi(argv[2]);
	if (Ops_per_thread > MAX_OPS_PER_THREAD)
		UT_FATAL("Ops per thread %d > %d", Threads, MAX_THREADS);
	Tx_per_thread = atoi(argv[3]);

	if (os_access(argv[4], F_OK) != 0) {
		pop = pmemobj_create(argv[4], "TEST", (PMEMOBJ_MIN_POOL) +
			(MAX_THREADS * CHUNKSIZE * CHUNKS_PER_THREAD),
		0666);

		if (pop == NULL)
			UT_FATAL("!pmemobj_create");
	} else {
		pop = pmemobj_open(argv[4], "TEST");

		if (pop == NULL)
			UT_FATAL("!pmemobj_open");
	}

	PMEMoid oid = pmemobj_root(pop, sizeof(struct root));
	struct root *r = pmemobj_direct(oid);
	UT_ASSERTne(r, NULL);

	struct worker_args args[MAX_THREADS];

	for (int i = 0; i < Threads; ++i) {
		args[i].pop = pop;
		args[i].r = r;
		args[i].idx = i;
	}

	run_worker(alloc_worker, args);
	run_worker(realloc_worker, args);
	run_worker(free_worker, args);
	run_worker(mix_worker, args);
	run_worker(alloc_free_worker, args);

	/*
	 * Reduce the number of lanes to a value smaller than the number of
	 * threads. This will ensure that at least some of the state of the lane
	 * will be shared between threads. Doing this might reveal bugs related
	 * to runtime race detection instrumentation.
	 */
	unsigned old_nlanes = pop->lanes_desc.runtime_nlanes;
	pop->lanes_desc.runtime_nlanes = TEST_LANES;
	run_worker(tx2_worker, args);
	pop->lanes_desc.runtime_nlanes = old_nlanes;

	/*
	 * This workload might create many allocation classes due to pvector,
	 * keep it last.
	 */
	if (Threads == MAX_THREADS) /* don't run for short tests */
		run_worker(tx_worker, args);

	run_worker(tx3_worker, args);

	pmemobj_close(pop);

	DONE(NULL);
}

#ifdef _MSC_VER
/*
 * Since libpmemobj is linked statically, we need to invoke its ctor/dtor.
 */
MSVC_CONSTR(libpmemobj_init)
MSVC_DESTR(libpmemobj_fini)
#endif
