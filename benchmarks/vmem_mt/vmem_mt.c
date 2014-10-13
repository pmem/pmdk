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
 * vmem_mt.c -- multi-threaded malloc benchmark
 */

#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <argp.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "tasks.h"

#define	MAX_THREADS 8
#define	KB 1024
#define	MB 1024 * KB
#define	DEF_ALLOC 512

void **allocated_mem;

const char *argp_program_version = "mt_benchmark 1.2";
static char doc[] = "Multithreaded allocator benchmark";
static char args_doc[] = "THREAD_COUNT OPS_COUNT";

static struct argp_option options[] = {
	{"pool-per-thread", 'p', 0, 0,
		"Create a pool for each worker thread"},
	{"seed", 'r', "SEED", 0,
		"Seed for random size allocator"},
	{"size", 's', "SIZE", 0, "Allocation size in bytes "
		"(default: 512b); single number for static allocator;"
		" comma separated min and max allocation size for ranged"},
	{"allocator", 'e', "NAME", 0, "Allocator to benchmark\n"
		"Valid arguments: vmem (default), malloc"},
	{"directory", 'd', "PATH", 0,
			"Create vmem pools in the given directory"},
	{ 0 }
};

#define	ALLOCATOR_NAME_MAX_LEN	10
static const char *allocator_names[] = {"vmem", "malloc"};

task_f tasks[MAX_TASK] = {
	task_malloc,
	task_free
};

/*
 * parse_range -- parses allocation size provided by user and sets appropriate
 * fields in arguments_t structure
 *
 * Syntax of input:
 * - ALLOCATION_STATIC: <number>
 * - ALLOCATION_RANGE:  <number,number>
 */
int
parse_range(arguments_t *arguments, char *allocation_size)
{
	char *endptr;

	int size_param;

	char *size = strtok(allocation_size, ",");
	if (size == NULL)
		return FAILURE;

	size_param = strtol(size, &endptr, 10);
	if (*endptr != 0) {
		return FAILURE;
	}

	size = strtok(NULL, ",");
	if (size == NULL) {
		arguments->allocation_size_max = size_param;
		arguments->allocation_type = ALLOCATION_STATIC;
	} else {
		arguments->allocation_size = size_param;
		arguments->allocation_type = ALLOCATION_RANGE;

		arguments->allocation_size_max = strtol(size, &endptr, 10);
		size_param = strtol(size, &endptr, 10);
		if (*endptr != 0) {
			return FAILURE;
		}

		if (size_param < arguments->allocation_size) {
			fprintf(stderr,
				"Range param: min > max!\n");
			return FAILURE;
		} else {
			arguments->allocation_size_max = size_param;
		}
	}

	return SUCCESS;
}

char *allocation_size_str = NULL;

/*
 * parse_opt -- argp parsing function
 */
static error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
	arguments_t *arguments = state->input;
	char *endptr;
	int i;
	struct stat dir_stat;

	switch (key) {
	case 'p':
		arguments->pool_per_thread = 1;
		break;

	case 's':
		allocation_size_str = arg;
		break;

	case 'r':
		arguments->seed = strtol(arg, &endptr, 10);
		if (*endptr != 0) {
			fprintf(stderr,
				"Invalid seed count: %s\n",
				endptr);
			argp_usage(state);
			return EXIT_FAILURE;
		}
		break;

	case 'e':
		for (i = 0; i < MAX_ALLOCATOR; ++i) {
			if (strncmp(arg, allocator_names[i],
				ALLOCATOR_NAME_MAX_LEN) == 0) {
				arguments->allocator = i;
				break;
			}
		}
		allocator = arguments->allocator;
		if (i == MAX_ALLOCATOR) {
			fprintf(stderr,
				"Unknown allocator %s, using default\n",
				arg);
		}
		break;

	case 'd':
		if (stat(arg, &dir_stat)) {
			perror("stat");
			return EXIT_FAILURE;
		}
		if (S_ISDIR(dir_stat.st_mode)) {
			arguments->dir_path = arg;
		} else {
			fprintf(stderr,
				"The given path is not a valid directory\n");
			return EXIT_FAILURE;
		}
		break;

	case ARGP_KEY_ARG:
		switch (state->arg_num) {
		case 0:
			arguments->thread_count = strtol(arg, &endptr, 10);
			if (*endptr != 0) {
				fprintf(stderr,
					"Invalid thread count: %s\n",
					endptr);
				argp_usage(state);
				return EXIT_FAILURE;
			}
			break;
		case 1:
			arguments->ops_count = strtol(arg, &endptr, 10);
			if (*endptr != 0) {
				fprintf(stderr,
					"Invalid operation count: %s\n",
					endptr);
				argp_usage(state);
				return EXIT_FAILURE;
			}
			break;
		default:
			argp_usage(state);
			return ARGP_ERR_UNKNOWN;
		}
		break;

	case ARGP_KEY_END:
		if (state->arg_num < 2)
			argp_usage(state);

			if (allocation_size_str == NULL ||
				parse_range(arguments, allocation_size_str)
				== FAILURE) {
				arguments->allocation_size_max = DEF_ALLOC;
				arguments->allocation_type = ALLOCATION_STATIC;
			}
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

/*
 * main -- entry point, initializes allocated_mem and runs the tasks
 */
int
main(int argc, char *argv[])
{
	int i, fails = 0;
	double task_duration;
	void **arg = NULL;
	uint64_t pool_size = 0;
	arguments_t arguments;
	int per_thread_args = 0;
	const int min_pool_size = 200;
	arguments.pool_per_thread = 0;
	arguments.allocator = ALLOCATOR_VMEM;
	arguments.dir_path = NULL;
	if (argp_parse(&argp, argc, argv, 0, 0, &arguments)) {
		fprintf(stderr, "argp_parse error");
		return EXIT_FAILURE;
	}
	int pools_count = arguments.pool_per_thread ?
			arguments.thread_count : 1;
	VMEM *pools[pools_count];
	void *pools_data[pools_count];
	allocated_mem = calloc(arguments.ops_count, sizeof (void*));

	if (allocated_mem == NULL) {
		perror("calloc");
		return EXIT_FAILURE;
	}

	if (arguments.allocator == ALLOCATOR_VMEM) {
		if (arguments.pool_per_thread &&
			arguments.thread_count > MAX_THREADS) {
			fprintf(stderr, "Maximum allowed thread count"
				" with pool per thread option enabled is %u\n",
				MAX_THREADS);
			return EXIT_FAILURE;
		}

		pools_count = arguments.pool_per_thread ?
			arguments.thread_count : 1;
		per_thread_args = arguments.pool_per_thread;
		pool_size = arguments.ops_count *
			arguments.allocation_size_max * 2u;

		pool_size /= pools_count;

		if (pool_size < min_pool_size * MB) {
			pool_size = min_pool_size * MB;
		}
		for (i = 0; i < pools_count; ++i) {
			if (arguments.dir_path == NULL) {
				pools_data[i] = mmap(NULL, pool_size,
					PROT_READ|PROT_WRITE,
					MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
				if (pools_data[i] == NULL) {
					free(allocated_mem);
					perror("malloc");
					return EXIT_FAILURE;
				}
				/* suppress valgrind warnings */
				memset(pools_data[i], 0xFF, pool_size);
				pools[i] = vmem_pool_create_in_region(
						pools_data[i], pool_size);
			} else {
				pools[i] = vmem_pool_create(arguments.dir_path,
						pool_size);
			}
			if (pools[i] == NULL) {
				perror("vmem_pool_create");
				free(allocated_mem);
				return EXIT_FAILURE;
			}
		}
		arg = (void **)pools;
	}

	/* Cache warmup. */
	for (i = 0; i < MAX_TASK; ++i) {
		fails += run_threads(&arguments, tasks[i],
			per_thread_args, arg, &task_duration);
	}

	for (i = 0; i < MAX_TASK; ++i) {
		fails += run_threads(&arguments, tasks[i],
			per_thread_args, arg, &task_duration);
		printf("%f;%f;",
			task_duration, arguments.ops_count/task_duration);
	}

	printf("\n");

	if (arguments.allocator == ALLOCATOR_VMEM) {
		for (i = 0; i < pools_count; ++i) {
			vmem_pool_delete(pools[i]);
			if (arguments.dir_path == NULL) {
				if (pools_data[i] != NULL)
					munmap(pools_data[i], pool_size);
			}
		}
	}

	free(allocated_mem);

	return (fails == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
