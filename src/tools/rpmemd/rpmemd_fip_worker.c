/*
 * Copyright 2016, Intel Corporation
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
 * rpmemd_fip_worker.c -- worker thread with ring buffer source file
 */
#include <stdint.h>
#include <pthread.h>

#include "rpmemd_log.h"
#include "rpmemd_fip_ring.h"
#include "rpmemd_fip_worker.h"
#define FATAL RPMEMD_FATAL
#include "sys_util.h"

/*
 * rpmemd_fip_worker -- worker handle
 */
struct rpmemd_fip_worker {
	volatile int *stop;
	struct rpmemd_fip *arg;
	struct rpmemd_fip_ring *ring;
	pthread_t thread;
	pthread_cond_t cond;
	pthread_mutex_t lock;
	rpmemd_fip_worker_fn func;
};

/*
 * rpmemd_fip_worker_thread_func -- worker thread function callback
 */
static void *
rpmemd_fip_worker_thread_func(void *arg)
{
	struct rpmemd_fip_worker *worker = arg;
	int ret = 0;

	while (!(*worker->stop)) {
		/*
		 * Wait on conditional variable for incoming entries
		 * in ring buffer.
		 */
		util_mutex_lock(&worker->lock);
		while (!(*worker->stop) &&
			rpmemd_fip_ring_is_empty(worker->ring)) {
			pthread_cond_wait(&worker->cond, &worker->lock);
		}

		void *data = rpmemd_fip_ring_pop(worker->ring);

		util_mutex_unlock(&worker->lock);

		/*
		 * After setting stop flag the signal can be send to
		 * stop the worker thread.
		 */
		if ((*worker->stop))
			break;

		/* process the data */
		ret = worker->func(worker->arg, data);
		if (ret)
			break;
	}

	return (void *)(uintptr_t)ret;
}

/*
 * rpmemd_fip_worker_init -- initialize worker thread
 */
struct rpmemd_fip_worker *
rpmemd_fip_worker_init(void *arg, volatile int *stop,
	size_t size, rpmemd_fip_worker_fn func)
{
	struct rpmemd_fip_worker *worker = malloc(sizeof(*worker));
	if (!worker) {
		RPMEMD_LOG(ERR, "!allocating worker");
		goto err_alloc;
	}

	worker->stop = stop;
	worker->arg = arg;
	worker->func = func;

	/* allocate a ring buffer */
	worker->ring = rpmemd_fip_ring_alloc(size);
	if (!worker->ring) {
		RPMEMD_LOG(ERR, "!allocating ring buffer");
		goto err_ring;
	}

	errno = pthread_mutex_init(&worker->lock, NULL);
	if (errno) {
		RPMEMD_LOG(ERR, "!creating worker's lock");
		goto err_lock;
	}

	errno = pthread_cond_init(&worker->cond, NULL);
	if (errno) {
		RPMEMD_LOG(ERR, "!creating worker's conditional variable");
		goto err_cond;
	}

	errno = pthread_create(&worker->thread, NULL,
			rpmemd_fip_worker_thread_func, worker);
	if (errno) {
		RPMEMD_LOG(ERR, "!creating worker's thread");
		goto err_thread;
	}

	return worker;
err_thread:
	pthread_cond_destroy(&worker->cond);
err_cond:
	pthread_mutex_destroy(&worker->lock);
err_lock:
	rpmemd_fip_ring_free(worker->ring);
err_ring:
	free(worker);
err_alloc:
	return NULL;
}

/*
 * rpmemd_fip_fini -- deinitialize worker thread
 */
int
rpmemd_fip_worker_fini(struct rpmemd_fip_worker *worker)
{
	int ret = 0;
	*worker->stop = 1;

	errno = pthread_cond_signal(&worker->cond);
	if (errno) {
		RPMEMD_LOG(ERR, "!sending signal to worker");
		ret = -1;
	}

	void *tret;
	errno = pthread_join(worker->thread, &tret);
	if (errno) {
		RPMEMD_LOG(ERR, "!joining worker's thread");
		ret = -1;
	} else {
		ret = (int)(uintptr_t)tret;
		if (ret) {
			RPMEMD_LOG(ERR, "worker thread faile with "
					"code -- %d", ret);
		}
	}

	errno = pthread_mutex_destroy(&worker->lock);
	if (errno) {
		RPMEMD_LOG(ERR, "!destroying worker's lock");
		ret = -1;
	}

	errno = pthread_cond_destroy(&worker->cond);
	if (errno) {
		RPMEMD_LOG(ERR, "!destroying worker's conditional variable");
		ret = -1;
	}

	rpmemd_fip_ring_free(worker->ring);

	free(worker);

	return ret;
}

/*
 * rpmemd_fip_worker_push -- push data for worker thread
 */
int
rpmemd_fip_worker_push(struct rpmemd_fip_worker *worker, void *data)
{
	int ret;

	util_mutex_lock(&worker->lock);
	ret = rpmemd_fip_ring_push(worker->ring, data);
	if (ret)
		goto unlock;

	errno = pthread_cond_signal(&worker->cond);
	if (errno) {
		ret = -1;
	}

unlock:
	util_mutex_unlock(&worker->lock);
	return ret;
}
