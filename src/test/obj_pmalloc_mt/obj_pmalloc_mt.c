// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2023, Intel Corporation */

/*
 * obj_pmalloc_mt.c -- multithreaded test of allocator
 */
#include <stdint.h>

#include "file.h"
#include "obj.h"
#include "pmalloc.h"
#include "sys_util.h"
#include "unittest.h"
#include "ut_mt.h"

#define MAX_THREADS 32
#define MAX_OPS_PER_THREAD 1000
#define ALLOC_SIZE 104
#define REALLOC_SIZE (ALLOC_SIZE * 3)
#define MIX_RERUNS 2

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
alloc_worker(void *arg)
{
	struct worker_args *a = arg;

	for (unsigned i = 0; i < Ops_per_thread; ++i) {
		pmalloc(a->pop, &a->r->offs[a->idx][i], ALLOC_SIZE, 0, 0);
		UT_ASSERTne(a->r->offs[a->idx][i], 0);
	}

	return NULL;
}

static void *
realloc_worker(void *arg)
{
	struct worker_args *a = arg;

	for (unsigned i = 0; i < Ops_per_thread; ++i) {
		prealloc(a->pop, &a->r->offs[a->idx][i], REALLOC_SIZE, 0, 0);
		UT_ASSERTne(a->r->offs[a->idx][i], 0);
	}

	return NULL;
}

static void *
free_worker(void *arg)
{
	struct worker_args *a = arg;

	for (unsigned i = 0; i < Ops_per_thread; ++i) {
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
	for (unsigned j = 0; j < MIX_RERUNS; ++j) {
		for (unsigned i = 0; i < Ops_per_thread; ++i) {
			pmalloc(a->pop, &a->r->offs[a->idx][i],
				ALLOC_SIZE, 0, 0);
			UT_ASSERTne(a->r->offs[a->idx][i], 0);
		}

		for (unsigned i = 0; i < Ops_per_thread; ++i) {
			pfree(a->pop, &a->r->offs[a->idx][i]);
			UT_ASSERTeq(a->r->offs[a->idx][i], 0);
		}
	}

	return NULL;
}

static void *
alloc_free_worker(void *arg)
{
	struct worker_args *a = arg;

	PMEMoid oid;
	for (unsigned i = 0; i < Ops_per_thread; ++i) {
		int err = pmemobj_alloc(a->pop, &oid, ALLOC_SIZE,
				0, NULL, NULL);
		UT_ASSERTeq(err, 0);
		pmemobj_free(&oid);
	}

	return NULL;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pmalloc_mt");

	if (argc != 5)
		UT_FATAL("usage: %s <threads> <ops/t> <tx/t> [file]", argv[0]);

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
	void *ut_args[MAX_THREADS];

	for (unsigned i = 0; i < Threads; ++i) {
		args[i].pop = pop;
		args[i].r = r;
		args[i].idx = i;
		ut_args[i] = &args[i];
	}

	run_workers(alloc_worker, Threads, ut_args);
	run_workers(realloc_worker, Threads, ut_args);
	run_workers(free_worker, Threads, ut_args);
	run_workers(mix_worker, Threads, ut_args);
	run_workers(alloc_free_worker, Threads, ut_args);

	pmemobj_close(pop);

	DONE(NULL);
}
