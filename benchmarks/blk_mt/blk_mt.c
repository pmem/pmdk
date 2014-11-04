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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * blk_mt.c -- simple multi-threaded performance test for PMEMBLK
 *
 * usage: For usage type blk_mt --help.
 *
 */

#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <argp.h>

#include <libpmem.h>
#include "blk_mt.h"
#include "workers.h"

#define	NSEC_IN_SEC 1000000000
#define	WORKER_COUNT_MAX 2
#define	SUCCESS 0
#define	FAILURE 1

/* typedef for the worker function */
typedef void *(*worker)(void *);

static void calculate_stats(struct measurements *data);

static int run_threads(worker thread_worker, uint32_t Nthreads,
		struct worker_info *worker_params);

static error_t parse_opt(int key, char *arg, struct argp_state *state);

const char *argp_program_version = "blk_mt_benchmark 1.0";
static char doc[] = "PMEMBLK multi-threaded benchmark";
static char args_doc[] = "THREAD_COUNT FILE_PATH";

static struct argp_option options[] = {
		{ "block-size", 'b', "SIZE", 0, "Block size in bytes. "
			"Use at least 512b. Default 512b." },
		{ "file-size", 's', "SIZE", 0, "File size in MB. Use at "
			"least 1024MB. Default 1024MB." },
		{ "file-io", 'i', 0, 0, "Run a simple file io "
			"benchmark" },
		{ "create-blk-file", 'c', 0, 0, "Prepare a fully written "
			"file for PMEMBLK benchmarks" },
		{ "ops-per-thread", 'o', "OPS", 0, "Number of "
			"operations performed in each thread. Use "
			"at least 50. Default 100" },
		{ 0 }
};

static struct argp argp = { options, parse_opt, args_doc, doc };

static worker pmem_workers[WORKER_COUNT_MAX] = { w_worker, r_worker };

static worker file_workers[WORKER_COUNT_MAX] = { wf_worker, rf_worker };

int
main(int argc, char *argv[])
{
	struct blk_arguments arguments;

	/* set the random seed value */
	srand(time(NULL));

	/* set default values */
	memset(&arguments, 0, sizeof (struct blk_arguments));
	arguments.block_size = 512;
	arguments.num_ops = 100;
	arguments.file_size = (PMEMBLK_MIN_POOL / 1024) / 1024;

	if (argp_parse(&argp, argc, argv, 0, 0, &arguments) != 0) {
		exit(1);
	}

	/* prepare open flags */
	int flags = O_RDWR | O_CREAT;
	if (arguments.file_io) {
		flags |= O_SYNC;
	}

	struct worker_info worker_params[arguments.thread_count];
	memset(worker_params, 0, sizeof (struct worker_info));

	/* create file on PMEM-aware file system */
	if ((worker_params[0].file_desc = open(arguments.file_path, flags,
			0666)) < 0) {
		perror(arguments.file_path);
		exit(1);
	}

	/* file_size is provided in MB */
	unsigned long long file_size_bytes = arguments.file_size * 1024 * 1024;

	/* pre-allocate file_size MB of persistent memory */
	if ((errno = posix_fallocate(worker_params[0].file_desc, (off_t)0,
			(off_t)file_size_bytes)) != 0) {
		perror("posix_fallocate\n");
		close(worker_params[0].file_desc);
		exit(1);
	}

	/* set common values */
	worker_params[0].block_size = arguments.block_size;
	worker_params[0].num_ops = arguments.num_ops;
	worker_params[0].file_lanes = arguments.thread_count;

	/* the pmem workers are the default workers */
	worker *thread_workers = pmem_workers;

	/* prepare parameters specific for file/pmem */
	if (arguments.file_io) {
		worker_params[0].num_blocks = file_size_bytes
				/ worker_params[0].block_size;
		thread_workers = file_workers;
	} else {
		close(worker_params[0].file_desc);
		worker_params[0].file_desc = -1;
		if ((worker_params[0].handle = pmemblk_pool_open(
				arguments.file_path,
				worker_params[0].block_size)) == NULL) {
			fprintf(stderr, "!%s: pmemblk_pool_open\n", argv[2]);
			exit(1);
		}
		worker_params[0].num_blocks = pmemblk_nblock(
				worker_params[0].handle);
	}

	/* propagate params to each info_t */
	for (int i = 1; i < arguments.thread_count; ++i) {
		memcpy(&worker_params[i], &worker_params[0],
				sizeof (struct worker_info));
		worker_params[i].thread_index = i;
		worker_params[i].seed = rand();
	}

	/* The blk mode file prep */
	if (arguments.prep_blk_file) {
		if (worker_params[0].file_desc >= 0)
			close(worker_params[0].file_desc);
		return run_threads(prep_worker, arguments.thread_count,
				worker_params);
	}

	struct measurements perf_meas;
	perf_meas.total_ops = arguments.thread_count
			* worker_params[0].num_ops;

	/* perform PMEMBLK warmup */
	if (!arguments.file_io) {
		if (run_threads(warmup_worker, arguments.thread_count,
				worker_params) != 0) {
			if (worker_params[0].file_desc >= 0)
				close(worker_params[0].file_desc);
			exit(1);
		}
	}

	for (int i = 0; i < WORKER_COUNT_MAX; ++i) {
		clock_gettime(CLOCK_MONOTONIC, &perf_meas.start_time);
		if (run_threads(thread_workers[i], arguments.thread_count,
				worker_params) != 0) {
			if (worker_params[0].file_desc >= 0)
				close(worker_params[0].file_desc);
			exit(1);
		}
		clock_gettime(CLOCK_MONOTONIC, &perf_meas.stop_time);

		calculate_stats(&perf_meas);
		printf("%f;%f;", perf_meas.total_run_time,
				perf_meas.ops_per_second);
	}

	printf("\n");

	if (worker_params[0].file_desc >= 0)
		close(worker_params[0].file_desc);


	/* cleanup and check pmem file */
	if (!arguments.file_io) {
		pmemblk_pool_close(worker_params[0].handle);

		/* not really necessary, but check consistency */
		int result = pmemblk_pool_check(arguments.file_path);
		if (result < 0) {
			fprintf(stderr, "!%s: pmemblk_pool_check\n",
					arguments.file_path);
		} else if (result == 0) {
			fprintf(stderr, "%s: pmemblk_pool_check: not "
					"consistent\n", arguments.file_path);
		}
	}

	exit(0);
}

/*
 * run_threads -- Runs a specified number of threads
 */
int
run_threads(worker thread_worker, uint32_t Nthreads,
		struct worker_info *worker_params)
{
	pthread_t threads[Nthreads];

	/* kick off nthread threads */
	for (int i = 0; i < Nthreads; ++i) {
		if (pthread_create(&threads[i], NULL, thread_worker,
				&worker_params[i]) != 0) {
			fprintf(stderr, "pthread_create failed\n");
			return FAILURE;
		}
	}

	/* wait for all the threads to complete */
	for (int i = 0; i < Nthreads; ++i) {
		if (pthread_join(threads[i], NULL) != 0) {
			fprintf(stderr, "pthread_join failed\n");
			return FAILURE;
		}
	}
	return SUCCESS;
}

/*
 * calculate_stats -- measurement's statistics calculating function
 */
void
calculate_stats(struct measurements *data)
{
	/* calculate the run time in seconds */
	data->total_run_time = (data->stop_time.tv_sec
			+ (double)data->stop_time.tv_nsec / NSEC_IN_SEC)
			- (data->start_time.tv_sec
					+ (double)data->start_time.tv_nsec
					/ NSEC_IN_SEC);

	if (data->total_run_time > 0) {
		data->ops_per_second = data->total_ops / data->total_run_time;
	}

	if (data->total_ops != 0) {
		data->mean_ops_time = data->total_run_time / data->total_ops;
	}
}

/*
 * parse_opt -- argp parsing function
 */
static error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
	struct blk_arguments *arguments = state->input;
	unsigned int min_pool_mb = (PMEMBLK_MIN_POOL / 1024) / 1024;
	int ret = SUCCESS;
	switch (key) {
	case 'b':
		arguments->block_size = strtoul(arg, NULL, 0);
		if (arguments->block_size < 512) {
			fprintf(stderr, "The provided block size is to "
					"small(min 512)\n");
			ret = FAILURE;
		}
		break;
	case 's':
		arguments->file_size = strtoul(arg, NULL, 0);
		if (arguments->file_size < min_pool_mb) {
			fprintf(stderr, "The provided file size is to "
					"small(min 1024)\n");
			ret = FAILURE;
		}
		break;
	case 'i':
		arguments->file_io = 1;
		if (arguments->prep_blk_file) {
			fprintf(stderr, "The -c and -i options cannot "
					"be chosen simultaneously\n");
			ret = FAILURE;
		}
		break;
	case 'c':
		arguments->prep_blk_file = 1;
		if (arguments->file_io) {
			fprintf(stderr, "The -c and -i options cannot "
					"be chosen simultaneously\n");
			ret = FAILURE;
		}
		break;
	case 'o':
		arguments->num_ops = strtoul(arg, NULL, 0);
		if (arguments->num_ops < 50) {
			fprintf(stderr, "The provided number of "
					"operations is to small(min 50)\n");
			ret = FAILURE;
		}
		break;
	case ARGP_KEY_ARG:
		switch (state->arg_num) {
		case 0:
			arguments->thread_count = strtoul(arg, NULL, 0);
			if (arguments->thread_count == 0) {
				fprintf(stderr, "The provided number of"
						" threads is invalid\n");
				ret = FAILURE;
			}
			break;
		case 1:
			arguments->file_path = arg;
			break;
		default:
			argp_usage(state);
			ret = ARGP_ERR_UNKNOWN;
			break;
		}
		break;
	case ARGP_KEY_END:
		if (state->arg_num < 2) {
			argp_usage(state);
		}
		break;
	default:
		ret = ARGP_ERR_UNKNOWN;
		break;
	}

	return ret;
}
