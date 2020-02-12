// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */
/*
 * benchmark_worker.cpp -- benchmark_worker module definitions
 */

#include <cassert>
#include <err.h>

#include "benchmark_worker.hpp"
#include "sys_util.h"

/*
 * worker_state_wait_for_transition -- wait for transition from and to
 * specified states
 */
static void
worker_state_wait_for_transition(struct benchmark_worker *worker,
				 enum benchmark_worker_state state,
				 enum benchmark_worker_state new_state)
{
	while (worker->state == state)
		os_cond_wait(&worker->cond, &worker->lock);
	assert(worker->state == new_state);
}

/*
 * worker_state_transition -- change worker state from and to specified states
 */
static void
worker_state_transition(struct benchmark_worker *worker,
			enum benchmark_worker_state old_state,
			enum benchmark_worker_state new_state)
{
	assert(worker->state == old_state);
	worker->state = new_state;
	os_cond_signal(&worker->cond);
}

/*
 * thread_func -- (internal) callback for os_thread
 */
static void *
thread_func(void *arg)
{
	assert(arg != nullptr);
	auto *worker = (struct benchmark_worker *)arg;

	util_mutex_lock(&worker->lock);

	worker_state_wait_for_transition(worker, WORKER_STATE_IDLE,
					 WORKER_STATE_INIT);

	if (worker->init)
		worker->ret_init = worker->init(worker->bench, worker->args,
						&worker->info);

	worker_state_transition(worker, WORKER_STATE_INIT,
				WORKER_STATE_INITIALIZED);

	if (worker->ret_init) {
		util_mutex_unlock(&worker->lock);
		return nullptr;
	}

	worker_state_wait_for_transition(worker, WORKER_STATE_INITIALIZED,
					 WORKER_STATE_RUN);

	worker->ret = worker->func(worker->bench, &worker->info);

	worker_state_transition(worker, WORKER_STATE_RUN, WORKER_STATE_END);

	worker_state_wait_for_transition(worker, WORKER_STATE_END,
					 WORKER_STATE_EXIT);

	if (worker->exit)
		worker->exit(worker->bench, worker->args, &worker->info);

	worker_state_transition(worker, WORKER_STATE_EXIT, WORKER_STATE_DONE);

	util_mutex_unlock(&worker->lock);
	return nullptr;
}

/*
 * benchmark_worker_alloc -- allocate benchmark worker
 */
struct benchmark_worker *
benchmark_worker_alloc(void)
{
	struct benchmark_worker *w =
		(struct benchmark_worker *)calloc(1, sizeof(*w));

	if (!w)
		return nullptr;

	util_mutex_init(&w->lock);

	if (os_cond_init(&w->cond))
		goto err_destroy_mutex;

	if (os_thread_create(&w->thread, nullptr, thread_func, w))
		goto err_destroy_cond;

	return w;

err_destroy_cond:
	os_cond_destroy(&w->cond);
err_destroy_mutex:
	util_mutex_destroy(&w->lock);
	free(w);
	return nullptr;
}

/*
 * benchmark_worker_free -- release benchmark worker
 */
void
benchmark_worker_free(struct benchmark_worker *w)
{
	os_thread_join(&w->thread, nullptr);
	os_cond_destroy(&w->cond);
	util_mutex_destroy(&w->lock);
	free(w);
}

/*
 * benchmark_worker_init -- call init function for worker
 */
int
benchmark_worker_init(struct benchmark_worker *worker)
{
	util_mutex_lock(&worker->lock);

	worker_state_transition(worker, WORKER_STATE_IDLE, WORKER_STATE_INIT);

	worker_state_wait_for_transition(worker, WORKER_STATE_INIT,
					 WORKER_STATE_INITIALIZED);

	int ret = worker->ret_init;

	util_mutex_unlock(&worker->lock);

	return ret;
}

/*
 * benchmark_worker_exit -- call exit function for worker
 */
void
benchmark_worker_exit(struct benchmark_worker *worker)
{
	util_mutex_lock(&worker->lock);

	worker_state_transition(worker, WORKER_STATE_END, WORKER_STATE_EXIT);

	worker_state_wait_for_transition(worker, WORKER_STATE_EXIT,
					 WORKER_STATE_DONE);

	util_mutex_unlock(&worker->lock);
}

/*
 * benchmark_worker_run -- run benchmark worker
 */
int
benchmark_worker_run(struct benchmark_worker *worker)
{
	int ret = 0;

	util_mutex_lock(&worker->lock);

	worker_state_transition(worker, WORKER_STATE_INITIALIZED,
				WORKER_STATE_RUN);

	util_mutex_unlock(&worker->lock);

	return ret;
}

/*
 * benchmark_worker_join -- join benchmark worker
 */
int
benchmark_worker_join(struct benchmark_worker *worker)
{
	util_mutex_lock(&worker->lock);

	worker_state_wait_for_transition(worker, WORKER_STATE_RUN,
					 WORKER_STATE_END);

	util_mutex_unlock(&worker->lock);

	return 0;
}
