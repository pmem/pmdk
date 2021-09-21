// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

#include "runtime.h"
#include <emmintrin.h>
#include <stdlib.h>
#include <pthread.h>

struct runtime_waker_data {
	pthread_cond_t *cond;
	pthread_mutex_t *lock;
};

static void
runtime_waker_wake(void *fdata)
{
	struct runtime_waker_data *data = fdata;
	pthread_mutex_lock(data->lock);
	pthread_cond_signal(data->cond);
	pthread_mutex_unlock(data->lock);
}

struct runtime {
	pthread_cond_t cond;
	pthread_mutex_t lock;

	uint64_t spins_before_sleep;
	struct timespec cond_wait_time;
};

struct runtime *
runtime_new(void)
{
	struct runtime *runtime = malloc(sizeof(struct runtime));
	pthread_cond_init(&runtime->cond, NULL);
	pthread_mutex_init(&runtime->lock, NULL);

	runtime->spins_before_sleep = 1000;
	runtime->cond_wait_time = (struct timespec){0, 1000000};

	return runtime;
}

void
runtime_delete(struct runtime *runtime)
{
	free(runtime);
}

static void
runtime_sleep(struct runtime *runtime)
{
	pthread_mutex_lock(&runtime->lock);
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	static const size_t nsec_in_sec = 1000000000ULL;
	ts.tv_nsec += runtime->cond_wait_time.tv_nsec;
	uint64_t secs = ts.tv_nsec / nsec_in_sec;
	ts.tv_nsec -= secs * nsec_in_sec;
	ts.tv_sec += runtime->cond_wait_time.tv_sec + secs;

	pthread_cond_timedwait(&runtime->cond, &runtime->lock, &ts);
	pthread_mutex_unlock(&runtime->lock);
}

void
runtime_wait_multiple(struct runtime *runtime, struct future *futs[],
		      size_t nfuts)
{
	struct runtime_waker_data waker_data;
	waker_data.cond = &runtime->cond;
	waker_data.lock = &runtime->lock;

	struct future_waker waker = {&waker_data, runtime_waker_wake};

	size_t ndone = 0;
	for (;;) {
		for (uint64_t i = 0; i < runtime->spins_before_sleep; ++i) {
			for (uint64_t f = 0; f < nfuts; ++f) {
				struct future *fut = futs[f];
				if (fut->context.state == FUTURE_STATE_COMPLETE)
					continue;

				if (future_poll(fut, waker) ==
				    FUTURE_STATE_COMPLETE) {
					ndone++;
				}
			}
			if (ndone == nfuts)
				return;

			_mm_pause();
		}
		runtime_sleep(runtime);
	}
}

void
runtime_wait(struct runtime *runtime, struct future *fut)
{
	runtime_wait_multiple(runtime, &fut, 1);
}
