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

#include <pthread.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>

#include "threads.h"

#define	__USE_UNIX98
#include <unistd.h>

#define	STATE_BUF_LEN 32

static int process_data(const void *buf, size_t len, void *arg);
static void *do_thread(void *arg);

/*
 * do_thread -- common thread entry point
 */
void *
do_thread(void *arg)
{
	struct thread_info *thread_info = arg;

	for (int i = thread_info->start; i < thread_info->end; ++i) {
		thread_info->result =
				thread_info->func(thread_info);
	}

	return NULL;
}

/*
 * run_threads -- starts threads execution
 */
int
run_threads(struct prog_args *args, thread_f task, void *arg, double *exec_time)
{
	int ret_val = 0;
	int ops_per_thread;
	pthread_t threads[args->threads_count];
	struct thread_info thread_info[args->threads_count];
	char *rand_statebuf;
	struct timeval task_start;
	struct timeval task_stop;

	rand_statebuf = (char *)calloc(args->threads_count, STATE_BUF_LEN);
	if (rand_statebuf == NULL) {
		return EXIT_FAILURE;
	}

	ops_per_thread = args->ops_count / args->threads_count;

	for (int i = 0; i < args->threads_count; ++i) {
		thread_info[i].hndl = arg;
		thread_info[i].func = task;
		thread_info[i].vec_size = args->vec_size;
		thread_info[i].el_size = args->el_size;
		thread_info[i].start = i * ops_per_thread;
		thread_info[i].end = ops_per_thread + i * ops_per_thread;
		thread_info[i].rand_state = NULL;

		if (args->rand) {
			thread_info[i].rand_state =
				calloc(1, sizeof (*thread_info[i].rand_state));
			if (thread_info[i].rand_state == NULL) {
				return EXIT_FAILURE;
			}

			initstate_r(args->seed, &rand_statebuf[i],
				STATE_BUF_LEN, thread_info[i].rand_state);
		};

		if ((thread_info[i].buf = (char *)
				calloc(args->el_size *
					args->vec_size * args->ops_count,
					sizeof (char))) == NULL) {
			return EXIT_FAILURE;
		}

		thread_info[i].buf_size = args->el_size * args->vec_size
				* args->ops_count;

		if ((thread_info[i].iov = (struct iovec *)calloc(args->vec_size,
				sizeof (struct iovec))) == NULL) {
			return EXIT_FAILURE;
		}
	}

	/* start to measure threads execution time */
	gettimeofday(&task_start, NULL);

	for (int i = 0; i < args->threads_count; ++i) {
		if (pthread_create(&threads[i], NULL,
				do_thread, &thread_info[i]) < 0) {
			return EXIT_FAILURE;
		}
	}

	/* wait until all the threads are finished */
	for (int i = 0; i < args->threads_count; ++i) {
		pthread_join(threads[i], NULL);
		if (thread_info[i].result == EXIT_FAILURE) {
			ret_val++;
		}
	}

	/* stop measuring threads execution time */
	gettimeofday(&task_stop, NULL);

	for (int i = 0; i < args->threads_count; ++i) {
		free(thread_info[i].rand_state);
		free(thread_info[i].iov);
		free(thread_info[i].buf);
	}

	free(rand_statebuf);

	/* calculate the actual threads execution time */
	if (exec_time != NULL) {
		*exec_time = (task_stop.tv_sec - task_start.tv_sec) +
			((task_stop.tv_usec - task_start.tv_usec) /
			1000000.0);
	}

	return ret_val;
}

/*
 * task_pmemlog_append -- appends to the log in the PMEM mode
 */
int
task_pmemlog_append(void *arg)
{
	int32_t rand_number;
	struct thread_info *thread_info = arg;
	size_t vec_size = thread_info->vec_size;
	size_t el_size = thread_info->el_size;
	PMEMlogpool *plp = (PMEMlogpool *)thread_info->hndl;

	if (thread_info->rand_state != NULL) {
		random_r(thread_info->rand_state, &rand_number);
		vec_size = rand_number % vec_size + MIN_VEC_SIZE;
		el_size = rand_number % el_size + MIN_EL_SIZE;
	};

	/* check vector size */
	if (vec_size > 1) {
		for (int i = 0; i < vec_size; ++i) {
			thread_info->iov[i].iov_base =
					&thread_info->buf[i * el_size];
			thread_info->iov[i].iov_len = el_size;
		}

		if (pmemlog_appendv(plp, thread_info->iov, vec_size) < 0) {
			return EXIT_FAILURE;
		}
	} else {
		if (pmemlog_append(plp, thread_info->buf, el_size) < 0) {
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

/*
 * task_fileiolog_append -- appends to the log in the FILEIO mode
 */
int
task_fileiolog_append(void *arg)
{
	int32_t rand_number;
	struct thread_info *thread_info = arg;
	size_t vec_size = thread_info->vec_size;
	size_t el_size = thread_info->el_size;
	int fd = *(int *)thread_info->hndl;

	if (thread_info->rand_state != NULL) {
		random_r(thread_info->rand_state, &rand_number);
		vec_size = rand_number % vec_size + MIN_VEC_SIZE;
		el_size = rand_number % el_size + MIN_EL_SIZE;
	};

	/* check vector size */
	if (vec_size > 1) {
		for (int i = 0; i < vec_size; ++i) {
			thread_info->iov[i].iov_base =
					&thread_info->buf[i * el_size];
			thread_info->iov[i].iov_len = el_size;
		}

		if ((writev(fd, thread_info->iov, vec_size)) == -1) {
			return EXIT_FAILURE;
		}

	} else {
		if ((write(fd, thread_info->buf, el_size)) == -1) {
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

/*
 * task_pmemlog_read -- reads from the log in the PMEM mode
 */
int
task_pmemlog_read(void *arg)
{
	int32_t rand_number;
	struct thread_info *thread_info = arg;
	size_t vec_size = thread_info->vec_size;
	size_t el_size = thread_info->el_size;
	PMEMlogpool *plp = (PMEMlogpool *)thread_info->hndl;

	thread_info->buf_ptr = 0;

	if (thread_info->rand_state != NULL) {
		random_r(thread_info->rand_state, &rand_number);
		vec_size = rand_number % vec_size + MIN_VEC_SIZE;
		el_size = rand_number % el_size + MIN_EL_SIZE;
	}

	pmemlog_walk(plp, vec_size * el_size, process_data, arg);

	return EXIT_SUCCESS;
}

/*
 * task_fileiolog_read -- reads from the log in the FILEIO mode
 */
int
task_fileiolog_read(void *arg)
{
	int32_t rand_number;
	struct thread_info *thread_info = arg;
	size_t vec_size = thread_info->vec_size;
	size_t el_size = thread_info->el_size;
	int fd = *(int *)thread_info->hndl;

	if (thread_info->rand_state != NULL) {
		random_r(thread_info->rand_state, &rand_number);
		vec_size = rand_number % vec_size + MIN_VEC_SIZE;
		el_size = rand_number % el_size + MIN_EL_SIZE;
	}

	thread_info->buf_ptr = 0;
	char buf[vec_size * el_size];
	while (pread(fd, buf, vec_size * el_size,
			thread_info->buf_ptr) != 0) {
		process_data(buf, vec_size * el_size, thread_info);
	}

	return EXIT_SUCCESS;
}

/*
 * process_data -- callback function for the pmemlog_walk()
 */
int
process_data(const void *buf, size_t len, void *arg)
{
	struct thread_info *thread_info = arg;

	if ((thread_info->buf_ptr + len) <= thread_info->buf_size) {
		memcpy(&thread_info->buf[thread_info->buf_ptr], buf, len);
		thread_info->buf_ptr += len;

		return 1;
	}
	return 0;
}
