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
 * pmembench.c -- main source file for benchmark framework
 */

#include <stdio.h>
#include <string.h>
#include <err.h>
#include <assert.h>
#include <getopt.h>
#include <unistd.h>
#include <math.h>
#include <sys/queue.h>

#include "benchmark.h"
#include "benchmark_worker.h"
#include "scenario.h"
#include "clo_vec.h"
#include "clo.h"
#include "config_reader.h"

/*
 * struct pmembench -- main context
 */
struct pmembench
{
	int argc;
	char **argv;
	struct scenario *scenario;
	struct clo_vec *clovec;
};

/*
 * struct benchmark -- benchmark's context
 */
struct benchmark
{
	LIST_ENTRY(benchmark) next;
	struct benchmark_info *info;
	void *priv;
	struct benchmark_clo *clos;
	size_t nclos;
	size_t args_size;
};

/*
 * struct latency_stats -- statistics for latency measurements
 */
struct latency_stats
{
	uint64_t min;
	uint64_t max;
	double avg;
	double std_dev;
};

/*
 * struct bench_list -- list of available benchmarks
 */
struct bench_list
{
	LIST_HEAD(benchmarks_head, benchmark) head;
	bool initialized;
};

/*
 * struct benchmark_opts -- arguments for pmembench
 */
struct benchmark_opts
{
	bool help;
	bool version;
	const char *file_name;
};

static struct version_s
{
	unsigned int major;
	unsigned int minor;
} version = {1, 0};


/* benchmarks list initialization */
static struct bench_list benchmarks = {
	.initialized = false,
};

/* list of arguments for pmembench */
static struct benchmark_clo pmembench_opts[] = {
	{
		.opt_short	= 'h',
		.opt_long	= "help",
		.descr		= "Print help",
		.type		= CLO_TYPE_FLAG,
		.off		= clo_field_offset(struct benchmark_opts,
							help),
		.ignore_in_res	= true,
	},
	{
		.opt_short	= 'v',
		.opt_long	= "version",
		.descr		= "Print version",
		.type		= CLO_TYPE_FLAG,
		.off		= clo_field_offset(struct benchmark_opts,
						version),
		.ignore_in_res	= true,
	},
};

/* common arguments for benchmarks */
static struct benchmark_clo pmembench_clos[] = {
	{
		.opt_short	= 'h',
		.opt_long	= "help",
		.descr		= "Print help for single benchmark",
		.type		= CLO_TYPE_FLAG,
		.off		= clo_field_offset(struct benchmark_args,
						help),
		.ignore_in_res	= true,
	},
	{
		.opt_short	= 't',
		.opt_long	= "threads",
		.type		= CLO_TYPE_UINT,
		.descr		= "Number of working threads",
		.off		= clo_field_offset(struct benchmark_args,
						n_threads),
		.def		= "1",
		.type_uint	= {
			.size	= clo_field_size(struct benchmark_args,
						n_threads),
			.base	= CLO_INT_BASE_DEC,
			.min	= 1,
			.max	= 32,
		},
	},
	{
		.opt_short	= 'n',
		.opt_long	= "ops-per-thread",
		.type		= CLO_TYPE_UINT,
		.descr		= "Number of operations per thread",
		.off		= clo_field_offset(struct benchmark_args,
						n_ops_per_thread),
		.def		= "1",
		.type_uint	= {
			.size	= clo_field_size(struct benchmark_args,
						n_ops_per_thread),
			.base	= CLO_INT_BASE_DEC,
			.min	= 1,
			.max	= ULLONG_MAX,
		},
	},
	{
		.opt_short	= 'd',
		.opt_long	= "data-size",
		.type		= CLO_TYPE_UINT,
		.descr		= "IO data size",
		.off		= clo_field_offset(struct benchmark_args,
						dsize),
		.def		= "1",
		.type_uint	= {
			.size	= clo_field_size(struct benchmark_args,
						dsize),
			.base	= CLO_INT_BASE_DEC|CLO_INT_BASE_HEX,
			.min	= 1,
			.max	= ULONG_MAX,
		},
	},
	{
		.opt_short	= 'f',
		.opt_long	= "file",
		.type		= CLO_TYPE_STR,
		.descr		= "File name",
		.off		= clo_field_offset(struct benchmark_args,
						fname),
		.def		= "/mnt/pmem/testfile",
		.ignore_in_res	= true,
	},
};

/*
 * pmembench_get_priv -- return private structure of benchmark
 */
void *
pmembench_get_priv(struct benchmark *bench)
{
	return bench->priv;
}

/*
 * pmembench_set_priv -- set private structure of benchmark
 */
void
pmembench_set_priv(struct benchmark *bench, void *priv)
{
	bench->priv = priv;
}

/*
 * pmembench_register -- register benchmark
 */
int
pmembench_register(struct benchmark_info *bench_info)
{
	struct benchmark *bench = calloc(1, sizeof (*bench));
	assert(bench != NULL);

	bench->info = bench_info;

	if (!benchmarks.initialized) {
		LIST_INIT(&benchmarks.head);
		benchmarks.initialized = true;
	}

	LIST_INSERT_HEAD(&benchmarks.head, bench, next);

	return 0;
}

/*
 * pmembench_get_info -- return structure with information about benchmark
 */
struct benchmark_info *
pmembench_get_info(struct benchmark *bench)
{
	return bench->info;
}

/*
 * pmembench_release_clos -- release CLO structure
 */
static void
pmembench_release_clos(struct benchmark *bench)
{
	free(bench->clos);
}

/*
 * pmembench_merge_clos -- merge benchmark's CLOs with common CLOs
 */
static void
pmembench_merge_clos(struct benchmark *bench)
{
	size_t size = sizeof (struct benchmark_args);
	size_t pb_nclos = ARRAY_SIZE(pmembench_clos);
	size_t nclos = pb_nclos;
	size_t i;

	if (bench->info->clos) {
		size += bench->info->opts_size;
		nclos += bench->info->nclos;
	}

	struct benchmark_clo *clos = calloc(nclos,
			sizeof (struct benchmark_clo));
	assert(clos != NULL);

	memcpy(clos, pmembench_clos, pb_nclos * sizeof (struct benchmark_clo));

	if (bench->info->clos) {
		memcpy(&clos[pb_nclos], bench->info->clos, bench->info->nclos *
				sizeof (struct benchmark_clo));

		for (i = 0; i < bench->info->nclos; i++) {
			clos[pb_nclos + i].off +=
				sizeof (struct benchmark_args);
		}
	}

	bench->clos = clos;
	bench->nclos = nclos;
	bench->args_size = size;
}

/*
 * pmembench_run_worker -- run worker with benchmark operation
 */
static int
pmembench_run_worker(struct benchmark *bench, struct benchmark_args *args,
		struct worker_info *winfo)
{
	uint64_t i;
	uint64_t ops = args->n_ops_per_thread;

	for (i = 0; i < ops; i++) {
		benchmark_time_get(&winfo->opinfo[i].t_start);
		if (bench->info->operation(bench, &winfo->opinfo[i]))
			return -1;
		benchmark_time_get(&winfo->opinfo[i].t_end);
	}

	return 0;
}

/*
 * pmembench_print_header -- print header of benchmark's results
 */
static void
pmembench_print_header(struct pmembench *pb, struct benchmark *bench,
		struct clo_vec *clovec)
{
	if (pb->scenario) {
		printf("%s: %s [%ld]\n", pb->scenario->name,
			bench->info->name, clovec->nargs);
	} else {
		printf("%s [%ld]\n", bench->info->name, clovec->nargs);
	}
	printf("total-time;"
		"ops-per-second;"
		"latency-avg;"
		"latency-min;"
		"latency-max;"
		"latency-std-dev");

	size_t i;
	for (i = 0; i < bench->nclos; i++) {
		if (!bench->clos[i].ignore_in_res) {
			printf(";%s", bench->clos[i].opt_long);
		}
	}
	printf("\n");
}

/*
 * pmembench_print_results -- print benchmark's results
 */
static void
pmembench_print_results(struct benchmark *bench, struct benchmark_args *args,
		benchmark_time_t time, struct latency_stats *latency)
{
	double total_time = benchmark_time_get_secs(&time);
	double opsps = args->n_threads * args->n_ops_per_thread / total_time;

	printf("%f;%f;%f;%ld;%ld;%f", total_time,
			opsps,
			latency->avg,
			latency->min,
			latency->max,
			latency->std_dev);

	size_t i;
	for (i = 0; i < bench->nclos; i++) {
		if (!bench->clos[i].ignore_in_res)
			printf(";%s",
				benchmark_clo_str(&bench->clos[i], args,
					bench->args_size));
	}
	printf("\n");
}

/*
 * pmembench_parse_clos -- parse command line arguments for benchmark
 */
static int
pmembench_parse_clo(struct pmembench *pb, struct benchmark *bench,
		struct clo_vec *clovec)
{
	if (!pb->scenario)
		return benchmark_clo_parse(pb->argc, pb->argv, bench->clos,
				bench->nclos, clovec);
	else
		return benchmark_clo_parse_scenario(pb->scenario, bench->clos,
				bench->nclos, clovec);
}

/*
 * pmembench_init_workers -- init benchmark's workers
 */
static void
pmembench_init_workers(struct benchmark_worker **workers, size_t nworkers,
		struct benchmark *bench, struct benchmark_args *args)
{
	size_t i;
	for (i = 0; i < nworkers; i++) {
		workers[i] = benchmark_worker_alloc();
		workers[i]->info.index = i;
		workers[i]->info.nops = args->n_ops_per_thread;
		workers[i]->info.opinfo = calloc(args->n_ops_per_thread,
				sizeof (struct operation_info));
		size_t j;
		for (j = 0; j < args->n_ops_per_thread; j++) {
			workers[i]->info.opinfo[j].worker = &workers[i]->info;
			workers[i]->info.opinfo[j].args = args;
			workers[i]->info.opinfo[j].index = j;
		}
		workers[i]->bench = bench;
		workers[i]->args = args;
		workers[i]->func = pmembench_run_worker;
		if (bench->info->init_worker)
			bench->info->init_worker(bench, args,
					&workers[i]->info);
	}
}

/*
 * pmembench_get_latency_stats -- return benchmark's latency results
 */
static void
pmembench_get_latency_stats(struct benchmark_worker **workers, size_t nworkers,
		struct latency_stats *stats)
{
	memset(stats, 0, sizeof (*stats));
	stats->min = ~0;
	uint64_t count = 0;
	size_t i;
	for (i = 0; i < nworkers; i++) {
		size_t j;
		for (j = 0; j < workers[i]->info.nops; j++) {
			benchmark_time_t l_diff;
			benchmark_time_diff(&l_diff,
				&workers[i]->info.opinfo[j].t_start,
				&workers[i]->info.opinfo[j].t_end);

			uint64_t nsecs = benchmark_time_get_nsecs(&l_diff);

			if (nsecs > stats->max)
				stats->max = nsecs;
			if (nsecs < stats->min)
				stats->min = nsecs;

			stats->avg += nsecs;
			count++;
		}
	}

	assert(count != 0);
	if (count > 0)
		stats->avg /= count;


	for (i = 0; i < nworkers; i++) {
		size_t j;
		for (j = 0; j < workers[i]->info.nops; j++) {
			benchmark_time_t l_diff;
			benchmark_time_diff(&l_diff,
				&workers[i]->info.opinfo[j].t_start,
				&workers[i]->info.opinfo[j].t_end);

			uint64_t nsecs = benchmark_time_get_nsecs(&l_diff);

			uint64_t d = nsecs > stats->avg ? nsecs - stats->avg :
							stats->avg - nsecs;
			stats->std_dev += d * d;
		}
	}

	if (count > 0)
		stats->std_dev = sqrt(stats->std_dev / count);
}

/*
 * pmembench_print_args -- print arguments for one benchmark
 */
static void
pmembench_print_args(struct benchmark_clo *clos, size_t nclos)
{
	struct benchmark_clo clo;
	for (size_t i = 0; i < nclos; i++) {
		clo = clos[i];
		if (clo.opt_short != 0)
			printf("\t-%c,", clo.opt_short);
		else
			printf("\t");
		printf("\t--%-15s\t\t%s", clo.opt_long, clo.descr);
		if (clo.type != CLO_TYPE_FLAG)
			printf(" [default: %s]", clo.def);

		if (clo.type == CLO_TYPE_INT) {
			if (clo.type_int.min != LONG_MIN)
				printf(" [min: %jd]", clo.type_int.min);
			if (clo.type_int.max != LONG_MAX)
				printf(" [max: %jd]", clo.type_int.max);
		} else if (clo.type == CLO_TYPE_UINT) {
			if (clo.type_uint.min != 0)
				printf(" [min: %ju]", clo.type_uint.min);
			if (clo.type_uint.max != ULONG_MAX)
				printf(" [max: %ju]", clo.type_uint.max);
		}
		printf("\n");
	}
}

/*
 * pmembench_print_help_single -- prints help for single benchmark
 */
static void
pmembench_print_help_single(struct benchmark *bench)
{
	struct benchmark_info *info = bench->info;
	printf("%s\n%s\n", info->name, info->brief);
	printf("\nArguments:\n");
	size_t nclos = sizeof (pmembench_clos) / sizeof (struct benchmark_clo);
	pmembench_print_args(pmembench_clos, nclos);
	if (info->clos == NULL)
		return;
	pmembench_print_args(info->clos, info->nclos);
}

/*
 * pmembench_print_usage -- print usage of framework
 */
static void
pmembench_print_usage()
{
	printf("Usage: $ pmembench [-h|--help] [-v|--version]"
			"\t[<benchmark>[<args>]]\n");
	printf("\t\t\t\t\t\t[<config>[<scenario>]]\n");
}

/*
 *  pmembench_print_version -- print version of framework
 */
static void
pmembench_print_version()
{
	printf("Benchmark framework - version %d.%d\n", version.major,
							version.minor);
}

/*
 * pmembench_print_examples() -- print examples of using framework
 */
static void
pmembench_print_examples()
{
	printf("\nExamples:\n");
	printf("$ pmembench <benchmark_name> <args>\n");
	printf(" # runs benchmark of name <benchmark> with arguments <args>\n");
	printf("or\n");
	printf("$ pmembench <config_file>\n");
	printf(" # runs all scenarios from config file\n");
	printf("or\n");
	printf("$ pmembench [<benchmark_name>] [-h|--help [-v|--version]\n");
	printf(" # prints help\n");
	printf("or\n");
	printf("$ pmembench <config_file> <name_of_scenario>\n");
	printf(" # runs the specified scenario from config file\n");
}

/*
 * pmembench_print_help -- print help for framework
 */
static void
pmembench_print_help()
{
	pmembench_print_version();
	pmembench_print_usage();
	printf("\nCommon arguments:\n");
	size_t nclos = sizeof (pmembench_opts) / sizeof (struct benchmark_clo);
	pmembench_print_args(pmembench_opts, nclos);

	printf("\nAvaliable benchmarks:\n");
	struct benchmark *bench = NULL;
	LIST_FOREACH(bench, &benchmarks.head, next)
		printf("\t%-20s\t\t%s\n", bench->info->name,
						bench->info->brief);
	printf("\n$ pmembench <benchmark> --help to print detailed information"
				" about benchmark arguments\n");
	pmembench_print_examples();
}

/*
 * pmembench_get_bench -- searching benchmarks by name
 */
static struct benchmark *
pmembench_get_bench(const char *name)
{
	struct benchmark *bench;
	LIST_FOREACH(bench, &benchmarks.head, next) {
		if (strcmp(name, bench->info->name) == 0)
			return bench;
	}

	return NULL;
}

/*
 * pmembench_parse_opts -- parse arguments for framework
 */
static int
pmembench_parse_opts(struct pmembench *pb)
{
	int ret = 0;
	int argc = ++pb->argc;
	char **argv = --pb->argv;
	struct benchmark_opts *opts = NULL;
	struct clo_vec *clovec;
	size_t size, n_clos;
	size = sizeof (struct benchmark_opts);
	n_clos = ARRAY_SIZE(pmembench_opts);
	clovec = clo_vec_alloc(size);
	assert(clovec != NULL);

	if (benchmark_clo_parse(argc, argv, pmembench_opts,
			n_clos, clovec)) {
		ret = -1;
		goto out;
	}

	opts = clo_vec_get_args(clovec, 0);

	if (opts->help)
		pmembench_print_help();
	if (opts->version)
		pmembench_print_version();

out:
	clo_vec_free(clovec);
	free(opts);
	return ret;
}

/*
 * pmembench_run -- runs one benchmark. Parses arguments and performs
 * specific functions.
 */
static int
pmembench_run(struct pmembench *pb, struct benchmark *bench)
{
	int ret = 0;
	assert(bench->info != NULL);
	pmembench_merge_clos(bench);

	if (bench->info->pre_init) {
		if (bench->info->pre_init(bench)) {
			warn("%s: pre-init failed", bench->info->name);
			ret = -1;
			goto out_release_clos;
		}
	}

	struct benchmark_args *args = NULL;

	struct clo_vec *clovec = clo_vec_alloc(bench->args_size);
	assert(clovec != NULL);

	if (pmembench_parse_clo(pb, bench, clovec)) {
		warn("%s: parsing command line arguments failed",
				bench->info->name);
		ret = -1;
		goto out_release_args;
	}

	args = clo_vec_get_args(clovec, 0);
	if (args->help) {
		pmembench_print_help_single(bench);
		goto out;
	}
	if (clovec->nargs > 1)
		pmembench_print_header(pb, bench, clovec);

	size_t args_i;
	for (args_i = 0; args_i < clovec->nargs; args_i++) {
		args = clo_vec_get_args(clovec, args_i);
		args->opts = (void *)((uintptr_t)args +
				sizeof (struct benchmark_args));

		if (bench->info->rm_file) {
			remove(args->fname);
		}

		if (bench->info->init) {
			if (bench->info->init(bench, args)) {
				warn("%s: initialization failed",
						bench->info->name);
				ret = -1;
				goto out;
			}
		}

		assert(bench->info->operation != NULL);

		struct benchmark_worker **workers = malloc(args->n_threads *
				sizeof (struct benchmark_worker *));
		assert(workers != NULL);

		pmembench_init_workers(workers, args->n_threads, bench, args);

		unsigned int i;
		benchmark_time_t start, stop, diff;
		benchmark_time_get(&start);
		for (i = 0; i < args->n_threads; i++) {
			benchmark_worker_run(workers[i]);
		}

		for (i = 0; i < args->n_threads; i++) {
			benchmark_worker_join(workers[i]);
			if (workers[i]->ret != 0) {
				ret = workers[i]->ret;
				fprintf(stderr, "thread number %d failed \n",
									i);
			}
		}
		benchmark_time_get(&stop);
		benchmark_time_diff(&diff, &start, &stop);

		struct latency_stats latency;
		pmembench_get_latency_stats(workers, args->n_threads, &latency);

		pmembench_print_results(bench, args, diff, &latency);

		for (i = 0; i < args->n_threads; i++) {
			if (bench->info->free_worker)
				bench->info->free_worker(bench, args,
						&workers[i]->info);

			free(workers[i]->info.opinfo);
			benchmark_worker_free(workers[i]);
		}

		free(workers);

		if (bench->info->exit)
			bench->info->exit(bench, args);
	}
out:
out_release_args:
	clo_vec_free(clovec);
out_release_clos:
	pmembench_release_clos(bench);
	return ret;
}

/*
 * pmembench_free_benchmarks -- release all benchmarks
 */
static void
__attribute__((destructor))
pmembench_free_benchmarks(void)
{
	while (!LIST_EMPTY(&benchmarks.head)) {
		struct benchmark *bench = LIST_FIRST(&benchmarks.head);
		LIST_REMOVE(bench, next);
		free(bench);
	}
}

/*
 * pmembench_run_scenario -- run single benchmark's scenario
 */
static int
pmembench_run_scenario(struct pmembench *pb, struct scenario *scenario)
{
	struct benchmark *bench = pmembench_get_bench(scenario->benchmark);
	if (NULL == bench) {
		fprintf(stderr, "unknown benchmark: %s\n", scenario->benchmark);
		return -1;
	}
	pb->scenario = scenario;
	return pmembench_run(pb, bench);
}

/*
 * pmembench_run_scenarios -- run all scenarios
 */
static int
pmembench_run_scenarios(struct pmembench *pb, struct scenarios *ss)
{
	struct scenario *scenario;
	FOREACH_SCENARIO(scenario, ss) {
		if (pmembench_run_scenario(pb, scenario) != 0)
			return -1;
	}
	return 0;
}

/*
 * pmembench_run_config -- run one or all scenarios from config file
 */
static int
pmembench_run_config(struct pmembench *pb, const char *config)
{
	struct config_reader *cr = config_reader_alloc();
	assert(cr != NULL);

	int ret = 0;
	int i;
	if ((ret = config_reader_read(cr, config)))
		goto out;

	struct scenarios *ss = NULL;
	if ((ret = config_reader_get_scenarios(cr, &ss)))
		goto out;

	assert(ss != NULL);


	if (pb->argc == 1) {
		if ((ret = pmembench_run_scenarios(pb, ss)) != 0)
			goto out_scenarios;
	} else {
		for (i = 1; i < pb->argc; i++) {
			char *name = pb->argv[i];
			struct scenario *scenario =
				scenarios_get_scenario(ss, name);
			if (!scenario) {
				fprintf(stderr, "unknown scenario: %s\n", name);
				ret = -1;
				goto out_scenarios;
			}
			if (pmembench_run_scenario(pb, scenario) != 0) {
				ret = -1;
				goto out_scenarios;
			}
		}
	}

out_scenarios:
	scenarios_free(ss);
out:
	config_reader_free(cr);
	return ret;
}

int
main(int argc, char *argv[])
{
	int ret = 0;
	struct pmembench *pb = calloc(1, sizeof (*pb));
	assert(pb != NULL);

	/*
	 * Parse common command line arguments and
	 * benchmark's specific ones.
	 */
	if (argc < 2) {
		pmembench_print_usage();
		exit(EXIT_FAILURE);
		return -1;
	}

	pb->argc = --argc;
	pb->argv = ++argv;

	char *bench_name = pb->argv[0];
	if (NULL == bench_name) {
		ret = -1;
		goto out;
	}
	int fexists = access(bench_name, R_OK) == 0;
	struct benchmark *bench = pmembench_get_bench(bench_name);
	if (NULL != bench)
		ret = pmembench_run(pb, bench);
	else if (fexists)
		ret = pmembench_run_config(pb, bench_name);
	else if ((ret = pmembench_parse_opts(pb)) != 0) {
		pmembench_print_usage();
		goto out;
	}

out:
	free(pb);
	return ret;
}
