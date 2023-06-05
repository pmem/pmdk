// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2023, Intel Corporation */

/*
 * obj_reserve_mt.c -- multithreaded test of the reserve/publish allocator
 */
#include <stdint.h>

#include "file.h"
#include "sys_util.h"
#include "unittest.h"

#define MAX_THREADS 32
#define MAX_OPS_PER_THREAD 1000
#define ALLOC_SIZE 104

#define CHUNKSIZE (1 << 18)
#define CHUNKS_PER_THREAD 3

static unsigned Threads;
static unsigned Ops_per_thread;

struct action {
	struct pobj_action pact;
	os_mutex_t lock;
	os_cond_t cond;
};

struct root {
	struct action actions[MAX_THREADS][MAX_OPS_PER_THREAD];
};

struct worker_args {
	PMEMobjpool *pop;
	struct root *r;
	unsigned idx;
};

static void *
action_cancel_worker(void *arg)
{
	struct worker_args *a = arg;

	PMEMoid oid;
	for (unsigned i = 0; i < Ops_per_thread; ++i) {
		unsigned arr_id = a->idx / 2;
		struct action *act = &a->r->actions[arr_id][i];
		if (a->idx % 2 == 0) {
			os_mutex_lock(&act->lock);
			oid = pmemobj_reserve(a->pop,
				&act->pact, ALLOC_SIZE, 0);
			UT_ASSERT(!OID_IS_NULL(oid));
			os_cond_signal(&act->cond);
			os_mutex_unlock(&act->lock);
		} else {
			os_mutex_lock(&act->lock);
			while (act->pact.heap.offset == 0)
				os_cond_wait(&act->cond, &act->lock);
			pmemobj_cancel(a->pop, &act->pact, 1);
			os_mutex_unlock(&act->lock);
		}
	}

	return NULL;
}

static void *
action_publish_worker(void *arg)
{
	struct worker_args *a = arg;

	PMEMoid oid;
	for (unsigned i = 0; i < Ops_per_thread; ++i) {
		unsigned arr_id = a->idx / 2;
		struct action *act = &a->r->actions[arr_id][i];
		if (a->idx % 2 == 0) {
			os_mutex_lock(&act->lock);
			oid = pmemobj_reserve(a->pop,
				&act->pact, ALLOC_SIZE, 0);
			UT_ASSERT(!OID_IS_NULL(oid));
			os_cond_signal(&act->cond);
			os_mutex_unlock(&act->lock);
		} else {
			os_mutex_lock(&act->lock);
			while (act->pact.heap.offset == 0)
				os_cond_wait(&act->cond, &act->lock);
			pmemobj_publish(a->pop, &act->pact, 1);
			os_mutex_unlock(&act->lock);
		}
	}

	return NULL;
}

static void *
action_mix_worker(void *arg)
{
	struct worker_args *a = arg;

	PMEMoid oid;
	for (unsigned i = 0; i < Ops_per_thread; ++i) {
		unsigned arr_id = a->idx / 2;
		unsigned publish = i % 2;
		struct action *act = &a->r->actions[arr_id][i];
		if (a->idx % 2 == 0) {
			os_mutex_lock(&act->lock);
			oid = pmemobj_reserve(a->pop,
				&act->pact, ALLOC_SIZE, 0);
			UT_ASSERT(!OID_IS_NULL(oid));
			os_cond_signal(&act->cond);
			os_mutex_unlock(&act->lock);
		} else {
			os_mutex_lock(&act->lock);
			while (act->pact.heap.offset == 0)
				os_cond_wait(&act->cond, &act->lock);
			if (publish)
				pmemobj_publish(a->pop, &act->pact, 1);
			else
				pmemobj_cancel(a->pop, &act->pact, 1);
			os_mutex_unlock(&act->lock);
		}
		pmemobj_persist(a->pop, act, sizeof(*act));
	}

	return NULL;
}

static void
actions_clear(PMEMobjpool *pop, struct root *r)
{
	for (unsigned i = 0; i < Threads; ++i) {
		for (unsigned j = 0; j < Ops_per_thread; ++j) {
			struct action *a = &r->actions[i][j];
			util_mutex_destroy(&a->lock);
			util_mutex_init(&a->lock);
			util_cond_destroy(&a->cond);
			util_cond_init(&a->cond);
			memset(&a->pact, 0, sizeof(a->pact));
			pmemobj_persist(pop, a, sizeof(*a));
		}
	}
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
	START(argc, argv, "obj_reserve_mt");

	if (argc != 4)
		UT_FATAL("usage: %s <threads> <ops/t> [file]", argv[0]);

	PMEMobjpool *pop;

	Threads = ATOU(argv[1]);
	if (Threads > MAX_THREADS)
		UT_FATAL("Threads %d > %d", Threads, MAX_THREADS);
	Ops_per_thread = ATOU(argv[2]);
	if (Ops_per_thread > MAX_OPS_PER_THREAD)
		UT_FATAL("Ops per thread %d > %d", Ops_per_thread,
			MAX_OPS_PER_THREAD);

	int exists = util_file_exists(argv[3]);
	if (exists < 0)
		UT_FATAL("!util_file_exists");

	if (!exists) {
		pop = pmemobj_create(argv[3], "TEST", (PMEMOBJ_MIN_POOL) +
			(MAX_THREADS * CHUNKSIZE * CHUNKS_PER_THREAD),
		0666);

		if (pop == NULL)
			UT_FATAL("!pmemobj_create");
	} else {
		pop = pmemobj_open(argv[3], "TEST");

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
		for (unsigned j = 0; j < Ops_per_thread; ++j) {
			struct action *a = &r->actions[i][j];
			util_mutex_init(&a->lock);
			util_cond_init(&a->cond);
		}
	}

	run_worker(action_cancel_worker, args);
	actions_clear(pop, r);
	run_worker(action_publish_worker, args);
	actions_clear(pop, r);
	run_worker(action_mix_worker, args);

	pmemobj_close(pop);

	DONE(NULL);
}
