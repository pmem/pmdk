// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2023, Intel Corporation */

/*
 * obj_tx_alloc_mt.c -- multithreaded test of allocator
 */
#include <stdint.h>

#include "file.h"
#include "obj.h"
#include "pmalloc.h"
#include "sys_util.h"
#include "unittest.h"

#define MAX_THREADS 32
#define MAX_OPS_PER_THREAD 1000
#define ALLOC_SIZE 104

#define CHUNKSIZE (1 << 18)
#define CHUNKS_PER_THREAD 3

static unsigned Threads;
static unsigned Ops_per_thread;
static unsigned Tx_per_thread;

struct root {
	uint64_t offs[MAX_THREADS][MAX_OPS_PER_THREAD];
};

struct worker_args {
	PMEMobjpool *pop;
	struct root *r;
	unsigned idx;
};

static void *
tx_worker(void *arg)
{
	struct worker_args *a = arg;
	PMEMoid oid;

	/*
	 * Allocate objects until exhaustion, once that happens the transaction
	 * will automatically abort and all of the objects will be freed.
	 */
	TX_BEGIN(a->pop) {
		for (unsigned n = 0; ; ++n) { /* this is NOT an infinite loop */
			oid = pmemobj_tx_alloc(ALLOC_SIZE, a->idx);
			UT_ASSERT(!OID_IS_NULL(oid));
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
	PMEMoid oid;

	/*
	 * Allocate N objects, abort, repeat M times. Should reveal issues in
	 * transaction abort handling.
	 */
	for (unsigned n = 0; n < Tx_per_thread; ++n) {
		TX_BEGIN(a->pop) {
			for (unsigned i = 0; i < Ops_per_thread; ++i) {
				oid = pmemobj_tx_alloc(ALLOC_SIZE, a->idx);
				UT_ASSERT(!OID_IS_NULL(oid));
			}
			pmemobj_tx_abort(EINVAL);
		} TX_END
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

	for (unsigned n = 0; n < Tx_per_thread; ++n) {
		PMEMoid oids[OPS_PER_TX];
		TX_BEGIN(a->pop) {
			for (int i = 0; i < OPS_PER_TX; ++i) {
				oids[i] = pmemobj_tx_alloc(ALLOC_SIZE, a->idx);
				for (unsigned j = 0; j < ALLOC_SIZE;
						j += STEP) {
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

	for (unsigned i = 0; i < Threads; ++i)
		THREAD_CREATE(&t[i], NULL, worker_func, &args[i]);

	for (unsigned i = 0; i < Threads; ++i)
		THREAD_JOIN(&t[i], NULL);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_tx_alloc_mt");

	if (argc != 5)
		UT_FATAL("usage: %s <threads> <ops/t> <tx/t> <file>", argv[0]);

	PMEMobjpool *pop;

	Threads = ATOU(argv[1]);
	if (Threads > MAX_THREADS)
		UT_FATAL("Threads %d > %d", Threads, MAX_THREADS);
	Ops_per_thread = ATOU(argv[2]);
	if (Ops_per_thread > MAX_OPS_PER_THREAD)
		UT_FATAL("Ops per thread %d > %d", Threads, MAX_THREADS);
	Tx_per_thread = ATOU(argv[3]);

	int exists = util_file_exists(argv[4]);
	if (exists < 0)
		UT_FATAL("!util_file_exists");

	if (!exists) {
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

	for (unsigned i = 0; i < Threads; ++i) {
		args[i].pop = pop;
		args[i].r = r;
		args[i].idx = i;
	}
	pmemobj_persist(pop, r, sizeof(struct root));

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
