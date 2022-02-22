// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include "libminiasync/runtime.h"
#include "core/os_thread.h"
#include "core/os.h"
#include <emmintrin.h>

struct runtime_waker_data {
	os_cond_t *cond;
	os_mutex_t *lock;
};

static void
runtime_waker_wake(void *fdata)
{
	struct runtime_waker_data *data = fdata;
	os_mutex_lock(data->lock);
	os_cond_signal(data->cond);
	os_mutex_unlock(data->lock);
}

struct runtime {
	os_cond_t cond;
	os_mutex_t lock;

	uint64_t spins_before_sleep;
	struct timespec cond_wait_time;
};

struct runtime *
runtime_new(void)
{
	struct runtime *runtime = malloc(sizeof(struct runtime));
	os_cond_init(&runtime->cond);
	os_mutex_init(&runtime->lock);
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
	os_mutex_lock(&runtime->lock);
	struct timespec ts;
	os_clock_gettime(CLOCK_REALTIME, &ts);
	static const size_t nsec_in_sec = 1000000000ULL;
	ts.tv_nsec += runtime->cond_wait_time.tv_nsec;
	uint64_t secs = (uint64_t)ts.tv_nsec / nsec_in_sec;
	ts.tv_nsec -= (long)(secs * nsec_in_sec);
	ts.tv_sec += (long)(runtime->cond_wait_time.tv_sec + (long)secs);

	os_cond_timedwait(&runtime->cond, &runtime->lock, &ts);
	os_mutex_unlock(&runtime->lock);
}

void
runtime_wait_multiple(struct runtime *runtime, struct future *futs[],
						size_t nfuts)
{
	struct runtime_waker_data waker_data;
	waker_data.cond = &runtime->cond;
	waker_data.lock = &runtime->lock;

	struct future_notifier notifier;
	notifier.waker = (struct future_waker){&waker_data, runtime_waker_wake};
	notifier.poller.ptr_to_monitor = NULL;
	size_t ndone = 0;
	for (;;) {
		for (uint64_t i = 0; i < runtime->spins_before_sleep; ++i) {
			for (uint64_t f = 0; f < nfuts; ++f) {
				struct future *fut = futs[f];
				if (fut->context.state == FUTURE_STATE_COMPLETE)
					continue;

				if (future_poll(fut, &notifier) ==
				    FUTURE_STATE_COMPLETE) {
					ndone++;
				}
				switch (notifier.notifier_used) {
					case FUTURE_NOTIFIER_POLLER:
					/*
					 * TODO: if this is the only future
					 * being polled, use umwait/umonitor
					 * for power-optimized polling.
					 */
					break;
					case FUTURE_NOTIFIER_WAKER:
					case FUTURE_NOTIFIER_NONE:
					/* nothing to do for wakers or none */
					break;
				};
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
