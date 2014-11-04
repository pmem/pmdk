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
 * log_mt.c -- multi-threaded pmemlog benchmark
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <argp.h>
#include <fcntl.h>
#include <errno.h>
#include <libpmemlog.h>
#include <sys/mman.h>
#include <unistd.h>

#include "threads.h"

/* command line arguments parsing function */
static error_t parse_opt(int key, char *arg, struct argp_state *state);

/* program name */
const char *argp_program_version = "pmemlog_benchmark 1.0";

/* general program description */
static char doc[] = "Multi-threaded benchmark for PMEMLOG";

/* non-optional arguments */
static char args_doc[] = "THREADS_COUNT OPS_COUNT FILE_NAME";

/* options program shall understand */
static struct argp_option options[] = {
	{"seed",         's', "VALUE", 0, "Random mode"},
	{"file-io-mode", 'i', 0,       0, "File I/O mode"},
	{"vector",       'v', "SIZE",  0, "Vector size "
			"(default: 1)"},
	{"element",      'e', "SIZE",  0, "Element size "
			"(default: 512 bytes)"},
	{0}
};

/* argp parser */
static struct argp argp = { options, parse_opt, args_doc, doc };

#define	TASKS_COUNT_MAX 2

/* benchmark functions to be executed in the FILEIO mode */
thread_f Tasks_fileiolog[TASKS_COUNT_MAX] = {
	task_fileiolog_append,
	task_fileiolog_read
};

/* benchmark functions to be executed in the PMEMLOG mode */
thread_f Tasks_pmemlog[TASKS_COUNT_MAX] = {
	task_pmemlog_append,
	task_pmemlog_read
};

thread_f *Tasks;

int
main(int argc, char *argv[])
{
	int fd;
	int flags;
	void *arg;
	size_t psize;
	int fails = 0;
	double exec_time;
	PMEMlogpool *plp = NULL;

	/* default program settings */
	struct prog_args args = {
		.seed = 0,
		.rand = false,
		.vec_size = DEF_VEC_SIZE,
		.el_size = DEF_EL_SIZE,
		.fileio_mode = false
	};

	/* parse command line arguments */
	if (argp_parse(&argp, argc, argv, 0, 0, &args) != 0) {
		exit(1);
	}

	flags = O_CREAT | O_RDWR;
	if (args.fileio_mode) {
		flags |= O_APPEND | O_SYNC;
	}

	/* create a file if it does not exist */
	if ((fd = open(args.file_name, flags, 0666)) < 0) {
		perror(args.file_name);
		exit(1);
	}

	if (!args.fileio_mode) {
		/* Calculate a required pool size */
		psize = args.ops_count * args.vec_size * args.el_size;
		if (psize < PMEMLOG_MIN_POOL) {
			psize = PMEMLOG_MIN_POOL;
		}

		/* pre-allocate memory */
		errno = posix_fallocate(fd, (off_t)0, psize);
		close(fd);
		if (errno != 0) {
			perror("posix_fallocate");
			exit(1);
		}

		if ((plp = pmemlog_pool_open(args.file_name)) == NULL) {
			perror("pmemlog_pool_open");
			exit(1);
		}
		Tasks = Tasks_pmemlog;
		arg = plp;

		/* pages table warm up */
		for (int i = 0; i < TASKS_COUNT_MAX; ++i) {
			fails = run_threads(&args, Tasks[i], arg, &exec_time);
		}
	} else {
		Tasks = Tasks_fileiolog;
		arg = &fd;
	}

	/* rewind a log pool */
	if (!args.fileio_mode)
		pmemlog_rewind(plp);

	/* actual benchmark execution */
	for (int i = 0; i < TASKS_COUNT_MAX; ++i) {
		fails = run_threads(&args, Tasks[i], arg, &exec_time);
		printf("%f;%f;",
				exec_time, args.ops_count / exec_time);
	}

	printf("\n");

	if (args.fileio_mode) {
		close(fd);
	} else {
		pmemlog_pool_close(plp);
	}

	(fails == 0) ? exit(0) : exit(1);
}

/*
 * parse_opt -- command line arguments parsing function
 */
static error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
	struct prog_args *args = state->input;
	char *tailptr;

	switch (key) {
	case 's':
		args->seed = strtol(arg, &tailptr, 10);
		args->rand = true;
		if (*tailptr != 0) {
			fprintf(stderr,
				"Invalid seed: %s\n",
				tailptr);
			argp_usage(state);
			return EXIT_FAILURE;
		}
		break;

	case 'v':
		args->vec_size = strtol(arg, &tailptr, 10);
		if (*tailptr != 0) {
			fprintf(stderr,
				"Invalid vector size: %s\n",
				tailptr);
			argp_usage(state);
			return EXIT_FAILURE;
		}
		break;

	case 'e':
		args->el_size = strtol(arg, &tailptr, 10);
		if (*tailptr != 0) {
			fprintf(stderr,
				"Invalid element size: %s\n",
				tailptr);
			argp_usage(state);
			return EXIT_FAILURE;
		}
		break;
	case 'i':
		args->fileio_mode = true;
		break;
	case ARGP_KEY_ARG:
		switch (state->arg_num) {
		case 0:
			args->threads_count = strtol(arg, &tailptr, 10);
			if (*tailptr != 0) {
				fprintf(stderr,
					"Invalid threads count: %s\n",
					tailptr);
				argp_usage(state);
				return EXIT_FAILURE;
			}
			break;
		case 1:
			args->ops_count = strtol(arg, &tailptr, 10);
			if (*tailptr != 0) {
				fprintf(stderr,
					"Invalid operations count: %s\n",
					tailptr);
				argp_usage(state);
				return EXIT_FAILURE;
			}
			break;
		case 2:
			args->file_name = arg;
			break;
		default:
			argp_usage(state);
			return ARGP_ERR_UNKNOWN;
		}
		break;
	case ARGP_KEY_END:
		if (state->arg_num < 3)
			argp_usage(state);

		break;
	default:
		return ARGP_ERR_UNKNOWN;
		break;
	}

	return 0;
}
