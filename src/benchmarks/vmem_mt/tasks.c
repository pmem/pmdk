/*
 * Copyright (c) 2014, Intel Corporation
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY LOG OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * tasks.c -- thread workers for the VMEM benchmark
 */

#include <tasks.h>
#include <pthread.h>
#include <sys/time.h>

#define	NSEC_IN_SEC 1000000000

struct task_def {
	int start;
	int end;
	task_f task;
	int result;
	void *arg;
	struct random_data *rand_state;
};

static allocation_type_t allocation_type;
static int allocation_min;
static int allocation_max;
allocator_t allocator;

/*
 * do_task -- thread start routine,
 * performs task_f on values between start and end.
 */
void *
do_task(void *arg)
{
	struct task_def *t_def = arg;
	int i;

	for (i = t_def->start; i < t_def->end; ++i) {
		t_def->result = t_def->task(i, t_def->arg, t_def->rand_state);
	}

	return NULL;
}

/*
 * run_threads -- performs runs number of tasks on threads
 */
int
run_threads(arguments_t *arguments, task_f task, int per_thread_arg, void **arg,
	double *elapsed)
{
	int i, n, ret = 0;
	pthread_t threads[arguments->thread_count];
	struct task_def t_def[arguments->thread_count];
	char *random_statebuf;
	struct timespec task_start, task_stop;

	allocation_type = arguments->allocation_type;
	allocation_min = arguments->allocation_size;
	allocation_max = arguments->allocation_size_max;

	random_statebuf = (char *)calloc(arguments->thread_count, 32);

	n = arguments->ops_count / arguments->thread_count;
	clock_gettime(CLOCK_MONOTONIC, &task_start);

	for (i = 0; i < arguments->thread_count; ++i) {
		t_def[i].start = i * n;
		t_def[i].end = n + i * n;
		t_def[i].task = task;
		t_def[i].rand_state =
			(struct random_data *)
			calloc(1, sizeof (struct random_data));

		initstate_r(arguments->seed,
			&random_statebuf[i], 32, t_def[i].rand_state);

		if (per_thread_arg) {
			t_def[i].arg = arg == NULL ? NULL : arg[i];
		} else {
			t_def[i].arg = arg == NULL ? NULL : arg[0];
		}
		if (pthread_create(&threads[i], NULL, do_task, &t_def[i]) < 0) {
			return FAILURE;
		}
	}

	for (i = 0; i < arguments->thread_count; ++i) {
		pthread_join(threads[i], NULL);
		if (t_def[i].result == FAILURE) {
			ret++;
		}
	}

	clock_gettime(CLOCK_MONOTONIC, &task_stop);

	free(random_statebuf);
	for (i = 0; i < arguments->thread_count; ++i) {
		free(t_def[i].rand_state);
	}

	if (elapsed != NULL) {
		*elapsed = (task_stop.tv_sec - task_start.tv_sec) +
				((task_stop.tv_nsec - task_start.tv_nsec) /
				(double)NSEC_IN_SEC);
	}

	return ret;
}

/*
 * task_malloc -- allocates MALLOC_SIZE memory in allocated_mem array
 */
int
task_malloc(int i, void *arg, struct random_data *rand_state)
{
	int size_to_alloc;
	int random_number;

	switch (allocation_type) {
	case ALLOCATION_STATIC:
		size_to_alloc = allocation_max;
		break;

	case ALLOCATION_RANGE:
		random_r(rand_state, &random_number);
		size_to_alloc =  random_number %
			(allocation_max - allocation_min)
			+ allocation_min;
		break;

	/* an unknown allocation is a failure, fall-through intentional */
	case ALLOCATION_UNKNOWN:
	default:
		return FAILURE;
	}

	if (allocator == ALLOCATOR_VMEM) {
		VMEM *pool = arg;
		allocated_mem[i] = vmem_malloc(pool, size_to_alloc);
	} else {
		allocated_mem[i] = malloc(size_to_alloc);
	}

	if (allocated_mem[i] == NULL) {
		return FAILURE;
	}
	return SUCCESS;
}

/*
 * task_free -- frees memory located in allocated_mem
 */
int
task_free(int i, void *arg, struct random_data *rand_state)
{
	if (allocated_mem[i] != NULL) {
		if (allocator == ALLOCATOR_VMEM) {
			VMEM *pool = arg;
			vmem_free(pool, allocated_mem[i]);
		} else {
			free(allocated_mem[i]);
		}
	}
	return SUCCESS;
}
