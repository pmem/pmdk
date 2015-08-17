/*
 * Copyright (c) 2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * benchmark_worker.c -- benchmark_worker module definitions
 */

#include <err.h>
#include <assert.h>

#include "benchmark_worker.h"

/*
 * benchmark_worker_alloc -- allocate benchmark worker
 */
struct benchmark_worker *
benchmark_worker_alloc(void)
{
	struct benchmark_worker *w = calloc(1, sizeof (*w));
	assert(w != NULL);
	return w;
}

/*
 * benchmark_worker_free -- release benchmark worker
 */
void
benchmark_worker_free(struct benchmark_worker *w)
{
	free(w);
}

/*
 * thread_func -- (internal) callback for pthread
 */
static void *
thread_func(void *arg)
{
	assert(arg != NULL);

	struct benchmark_worker *worker = (struct benchmark_worker *)arg;
	worker->ret = worker->func(worker->bench, worker->args, &worker->info);

	return NULL;
}

/*
 * benchmark_worker_run -- run benchmark worker
 */
int
benchmark_worker_run(struct benchmark_worker *w)
{
	return pthread_create(&w->thread, NULL, thread_func, w);
}

/*
 * benchmark_worker_join -- join benchmark worker
 */
int
benchmark_worker_join(struct benchmark_worker *w)
{
	return pthread_join(w->thread, NULL);
}
