// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

#include "mover.h"
#include "libpmem2/async/future.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

struct mover {
	struct mover_runner *runner;
};

struct mover *
mover_new(struct mover_runner *runner)
{
	struct mover *mover = malloc(sizeof(struct mover));
	mover->runner = runner;

	return mover;
}

void
mover_delete(struct mover *mover)
{
	free(mover);
}

static void
mover_memcpy_cb(struct future_context *context)
{
	struct mover_memcpy_data *data = future_context_get_data(context);
	atomic_store(&data->complete, 1);
	FUTURE_WAKER_WAKE(&data->waker);
}

static enum future_state
mover_memcpy_impl(struct future_context *context, struct future_waker waker)
{
	struct mover_memcpy_data *data = future_context_get_data(context);
	if (context->state == FUTURE_STATE_IDLE) {
		data->waker = waker;
		data->mover_cb = mover_memcpy_cb;
		data->mover->runner->memcpy(data->mover->runner, context);
	}
	return atomic_load(&data->complete) ? FUTURE_STATE_COMPLETE
					    : FUTURE_STATE_RUNNING;
}

struct mover_memcpy_future
mover_memcpy(struct mover *mover, void *dest, void *src, size_t n)
{
	struct mover_memcpy_future future;
	future.data.mover = mover;
	future.data.dest = dest;
	future.data.src = src;
	future.data.n = n;
	future.data.complete = 0;
	FUTURE_INIT(&future, mover_memcpy_impl);

	return future;
}

static void
memcpy_sync(void *runner, struct future_context *context)
{
	struct mover_memcpy_data *data = future_context_get_data(context);
	struct mover_memcpy_output *output = future_context_get_output(context);
	output->dest = memcpy(data->dest, data->src, data->n);
	data->mover_cb(context);
}

struct mover_runner synchronous_runner = {
	.runner_data = NULL,
	.memcpy = memcpy_sync,
};

struct mover_runner *
mover_runner_synchronous(void)
{
	return &synchronous_runner;
}

static void *
async_memcpy_pthread(void *arg)
{
	memcpy_sync(NULL, arg);

	return NULL;
}

static void
memcpy_pthreads(void *runner, struct future_context *context)
{
	pthread_t thread;
	pthread_create(&thread, NULL, async_memcpy_pthread, context);
}

struct mover_runner pthreads_runner = {
	.runner_data = NULL,
	.memcpy = memcpy_pthreads,
};

struct mover_runner *
mover_runner_pthreads(void)
{
	return &pthreads_runner;
}
