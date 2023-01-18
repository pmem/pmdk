// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2023, Intel Corporation */

/*
 * pmembench.cpp -- main source file for benchmark framework
 */

#include <cassert>
#include <cerrno>
#include <cfloat>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <err.h>
#include <getopt.h>
#include <linux/limits.h>
#include <sched.h>
#include <sys/wait.h>
#include <unistd.h>

#include "benchmark.hpp"
#include "benchmark_worker.hpp"
#include "clo.hpp"
#include "clo_vec.hpp"
#include "config_reader.hpp"
#include "file.h"
#include "libpmempool.h"
#include "mmap.h"
#include "os.h"
#include "os_thread.h"
#include "queue.h"
#include "scenario.hpp"
#include "set.h"
#include "util.h"

/* average time required to get a current time from the system */
unsigned long long Get_time_avg;

#define MIN_EXE_TIME_E 0.5

/*
 * struct pmembench -- main context
 */
struct pmembench {
	int argc;
	char **argv;
	struct scenario *scenario;
	struct clo_vec *clovec;
	bool override_clos;
};

/*
 * struct benchmark -- benchmark's context
 */
struct benchmark {
	PMDK_LIST_ENTRY(benchmark) next;
	struct benchmark_info *info;
	void *priv;
	struct benchmark_clo *clos;
	size_t nclos;
	size_t args_size;
};

/*
 * struct bench_list -- list of available benchmarks
 */
struct bench_list {
	PMDK_LIST_HEAD(benchmarks_head, benchmark) head;
	bool initialized;
};

/*
 * struct benchmark_opts -- arguments for pmembench
 */
struct benchmark_opts {
	bool help;
	bool version;
	const char *file_name;
};

static struct version_s {
	unsigned major;
	unsigned minor;
} version = {1, 0};

/* benchmarks list initialization */
static struct bench_list benchmarks;

/* common arguments for benchmarks */
static struct benchmark_clo pmembench_clos[13];

/* list of arguments for pmembench */
static struct benchmark_clo pmembench_opts[2];
CONSTRUCTOR(pmembench_constructor)
void
pmembench_constructor(void)
{
	pmembench_opts[0].opt_short = 'h';
	pmembench_opts[0].opt_long = "help";
	pmembench_opts[0].descr = "Print help";
	pmembench_opts[0].type = CLO_TYPE_FLAG;
	pmembench_opts[0].off = clo_field_offset(struct benchmark_opts, help);
	pmembench_opts[0].ignore_in_res = true;

	pmembench_opts[1].opt_short = 'v';
	pmembench_opts[1].opt_long = "version";
	pmembench_opts[1].descr = "Print version";
	pmembench_opts[1].type = CLO_TYPE_FLAG;
	pmembench_opts[1].off =
		clo_field_offset(struct benchmark_opts, version);
	pmembench_opts[1].ignore_in_res = true;

	pmembench_clos[0].opt_short = 'h';
	pmembench_clos[0].opt_long = "help";
	pmembench_clos[0].descr = "Print help for single benchmark";
	pmembench_clos[0].type = CLO_TYPE_FLAG;
	pmembench_clos[0].off = clo_field_offset(struct benchmark_args, help);
	pmembench_clos[0].ignore_in_res = true;

	pmembench_clos[1].opt_short = 't';
	pmembench_clos[1].opt_long = "threads";
	pmembench_clos[1].type = CLO_TYPE_UINT;
	pmembench_clos[1].descr = "Number of working threads";
	pmembench_clos[1].off =
		clo_field_offset(struct benchmark_args, n_threads);
	pmembench_clos[1].def = "1";
	pmembench_clos[1].type_uint.size =
		clo_field_size(struct benchmark_args, n_threads);
	pmembench_clos[1].type_uint.base = CLO_INT_BASE_DEC;
	pmembench_clos[1].type_uint.min = 1;
	pmembench_clos[1].type_uint.max = UINT_MAX;

	pmembench_clos[2].opt_short = 'n';
	pmembench_clos[2].opt_long = "ops-per-thread";
	pmembench_clos[2].type = CLO_TYPE_UINT;
	pmembench_clos[2].descr = "Number of operations per thread";
	pmembench_clos[2].off =
		clo_field_offset(struct benchmark_args, n_ops_per_thread);
	pmembench_clos[2].def = "1";
	pmembench_clos[2].type_uint.size =
		clo_field_size(struct benchmark_args, n_ops_per_thread);
	pmembench_clos[2].type_uint.base = CLO_INT_BASE_DEC;
	pmembench_clos[2].type_uint.min = 1;
	pmembench_clos[2].type_uint.max = ULLONG_MAX;

	pmembench_clos[3].opt_short = 'd';
	pmembench_clos[3].opt_long = "data-size";
	pmembench_clos[3].type = CLO_TYPE_UINT;
	pmembench_clos[3].descr = "IO data size";
	pmembench_clos[3].off = clo_field_offset(struct benchmark_args, dsize);
	pmembench_clos[3].def = "1";

	pmembench_clos[3].type_uint.size =
		clo_field_size(struct benchmark_args, dsize);
	pmembench_clos[3].type_uint.base = CLO_INT_BASE_DEC | CLO_INT_BASE_HEX;
	pmembench_clos[3].type_uint.min = 1;
	pmembench_clos[3].type_uint.max = ULONG_MAX;

	pmembench_clos[4].opt_short = 'f';
	pmembench_clos[4].opt_long = "file";
	pmembench_clos[4].type = CLO_TYPE_STR;
	pmembench_clos[4].descr = "File name";
	pmembench_clos[4].off = clo_field_offset(struct benchmark_args, fname);
	pmembench_clos[4].def = "/mnt/pmem/testfile";
	pmembench_clos[4].ignore_in_res = true;

	pmembench_clos[5].opt_short = 'm';
	pmembench_clos[5].opt_long = "fmode";
	pmembench_clos[5].type = CLO_TYPE_UINT;
	pmembench_clos[5].descr = "File mode";
	pmembench_clos[5].off = clo_field_offset(struct benchmark_args, fmode);
	pmembench_clos[5].def = "0666";
	pmembench_clos[5].ignore_in_res = true;
	pmembench_clos[5].type_uint.size =
		clo_field_size(struct benchmark_args, fmode);
	pmembench_clos[5].type_uint.base = CLO_INT_BASE_OCT;
	pmembench_clos[5].type_uint.min = 0;
	pmembench_clos[5].type_uint.max = ULONG_MAX;

	pmembench_clos[6].opt_short = 's';
	pmembench_clos[6].opt_long = "seed";
	pmembench_clos[6].type = CLO_TYPE_UINT;
	pmembench_clos[6].descr = "PRNG seed";
	pmembench_clos[6].off = clo_field_offset(struct benchmark_args, seed);
	pmembench_clos[6].def = "0";
	pmembench_clos[6].type_uint.size =
		clo_field_size(struct benchmark_args, seed);
	pmembench_clos[6].type_uint.base = CLO_INT_BASE_DEC;
	pmembench_clos[6].type_uint.min = 0;
	pmembench_clos[6].type_uint.max = ~0;

	pmembench_clos[7].opt_short = 'r';
	pmembench_clos[7].opt_long = "repeats";
	pmembench_clos[7].type = CLO_TYPE_UINT;
	pmembench_clos[7].descr = "Number of repeats of scenario";
	pmembench_clos[7].off =
		clo_field_offset(struct benchmark_args, repeats);
	pmembench_clos[7].def = "1";
	pmembench_clos[7].type_uint.size =
		clo_field_size(struct benchmark_args, repeats);
	pmembench_clos[7].type_uint.base = CLO_INT_BASE_DEC | CLO_INT_BASE_HEX;
	pmembench_clos[7].type_uint.min = 1;
	pmembench_clos[7].type_uint.max = ULONG_MAX;

	pmembench_clos[8].opt_short = 'F';
	pmembench_clos[8].opt_long = "thread-affinity";
	pmembench_clos[8].descr = "Set worker threads CPU affinity mask";
	pmembench_clos[8].type = CLO_TYPE_FLAG;
	pmembench_clos[8].off =
		clo_field_offset(struct benchmark_args, thread_affinity);
	pmembench_clos[8].def = "false";

	/*
	 * XXX: add link to blog post about optimal affinity
	 * when it will be done
	 */
	pmembench_clos[9].opt_short = 'I';
	pmembench_clos[9].opt_long = "affinity-list";
	pmembench_clos[9].descr =
		"Set affinity mask as a list of CPUs separated by semicolon";
	pmembench_clos[9].type = CLO_TYPE_STR;
	pmembench_clos[9].off =
		clo_field_offset(struct benchmark_args, affinity_list);
	pmembench_clos[9].def = "";
	pmembench_clos[9].ignore_in_res = true;

	pmembench_clos[10].opt_long = "main-affinity";
	pmembench_clos[10].descr = "Set affinity for main thread";
	pmembench_clos[10].type = CLO_TYPE_INT;
	pmembench_clos[10].off =
		clo_field_offset(struct benchmark_args, main_affinity);
	pmembench_clos[10].def = "-1";
	pmembench_clos[10].ignore_in_res = false;
	pmembench_clos[10].type_int.size =
		clo_field_size(struct benchmark_args, main_affinity);
	pmembench_clos[10].type_int.base = CLO_INT_BASE_DEC;
	pmembench_clos[10].type_int.min = (-1);
	pmembench_clos[10].type_int.max = LONG_MAX;

	pmembench_clos[11].opt_short = 'e';
	pmembench_clos[11].opt_long = "min-exe-time";
	pmembench_clos[11].type = CLO_TYPE_UINT;
	pmembench_clos[11].descr = "Minimal execution time in seconds";
	pmembench_clos[11].off =
		clo_field_offset(struct benchmark_args, min_exe_time);
	pmembench_clos[11].def = "0";
	pmembench_clos[11].type_uint.size =
		clo_field_size(struct benchmark_args, min_exe_time);
	pmembench_clos[11].type_uint.base = CLO_INT_BASE_DEC;
	pmembench_clos[11].type_uint.min = 0;
	pmembench_clos[11].type_uint.max = ULONG_MAX;

	pmembench_clos[12].opt_short = 'p';
	pmembench_clos[12].opt_long = "dynamic-poolset";
	pmembench_clos[12].type = CLO_TYPE_FLAG;
	pmembench_clos[12].descr =
		"Allow benchmark to create poolset and reuse files";
	pmembench_clos[12].off =
		clo_field_offset(struct benchmark_args, is_dynamic_poolset);
	pmembench_clos[12].ignore_in_res = true;
}

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
	assert(bench_info->name && bench_info->brief);

	struct benchmark *bench = (struct benchmark *)calloc(1, sizeof(*bench));
	assert(bench != nullptr);

	bench->info = bench_info;

	if (!benchmarks.initialized) {
		PMDK_LIST_INIT(&benchmarks.head);
		benchmarks.initialized = true;
	}

	PMDK_LIST_INSERT_HEAD(&benchmarks.head, bench, next);

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
	size_t size = sizeof(struct benchmark_args);
	size_t pb_nclos = ARRAY_SIZE(pmembench_clos);
	size_t nclos = pb_nclos;
	size_t i;

	if (bench->info->clos) {
		size += bench->info->opts_size;
		nclos += bench->info->nclos;
	}

	auto *clos = (struct benchmark_clo *)calloc(
		nclos, sizeof(struct benchmark_clo));
	assert(clos != nullptr);

	memcpy(clos, pmembench_clos, pb_nclos * sizeof(struct benchmark_clo));

	if (bench->info->clos) {
		memcpy(&clos[pb_nclos], bench->info->clos,
		       bench->info->nclos * sizeof(struct benchmark_clo));

		for (i = 0; i < bench->info->nclos; i++) {
			clos[pb_nclos + i].off += sizeof(struct benchmark_args);
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
pmembench_run_worker(struct benchmark *bench, struct worker_info *winfo)
{
	benchmark_time_get(&winfo->beg);
	for (size_t i = 0; i < winfo->nops; i++) {
		if (bench->info->operation(bench, &winfo->opinfo[i]))
			return -1;
		benchmark_time_get(&winfo->opinfo[i].end);
	}
	benchmark_time_get(&winfo->end);

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
		printf("%s: %s [%" PRIu64 "]%s%s%s\n", pb->scenario->name,
		       bench->info->name, clovec->nargs,
		       pb->scenario->group ? " [group: " : "",
		       pb->scenario->group ? pb->scenario->group : "",
		       pb->scenario->group ? "]" : "");
	} else {
		printf("%s [%" PRIu64 "]\n", bench->info->name, clovec->nargs);
	}
	printf("total-avg[sec];"
	       "ops-per-second[1/sec];"
	       "total-max[sec];"
	       "total-min[sec];"
	       "total-median[sec];"
	       "total-std-dev[sec];"
	       "latency-avg[nsec];"
	       "latency-min[nsec];"
	       "latency-max[nsec];"
	       "latency-std-dev[nsec];"
	       "latency-pctl-50.0%%[nsec];"
	       "latency-pctl-99.0%%[nsec];"
	       "latency-pctl-99.9%%[nsec]");
	size_t i;
	for (i = 0; i < bench->nclos; i++) {
		if (!bench->clos[i].ignore_in_res) {
			printf(";%s", bench->clos[i].opt_long);
		}
	}

	if (bench->info->print_bandwidth)
		printf(";bandwidth[MiB/s]");

	if (bench->info->print_extra_headers)
		bench->info->print_extra_headers();
	printf("\n");
}

/*
 * pmembench_print_results -- print benchmark's results
 */
static void
pmembench_print_results(struct benchmark *bench, struct benchmark_args *args,
			struct total_results *res)
{
	printf("%f;%f;%f;%f;%f;%f;%" PRIu64 ";%" PRIu64 ";%" PRIu64
	       ";%f;%" PRIu64 ";%" PRIu64 ";%" PRIu64,
	       res->total.avg, res->nopsps, res->total.max, res->total.min,
	       res->total.med, res->total.std_dev, res->latency.avg,
	       res->latency.min, res->latency.max, res->latency.std_dev,
	       res->latency.pctl50_0p, res->latency.pctl99_0p,
	       res->latency.pctl99_9p);

	size_t i;
	for (i = 0; i < bench->nclos; i++) {
		if (!bench->clos[i].ignore_in_res)
			printf(";%s",
			       benchmark_clo_str(&bench->clos[i], args,
						 bench->args_size));
	}

	if (bench->info->print_bandwidth)
		printf(";%f", res->nopsps * args->dsize / 1024 / 1024);

	if (bench->info->print_extra_values)
		bench->info->print_extra_values(bench, args, res);
	printf("\n");
}

/*
 * pmembench_parse_clos -- parse command line arguments for benchmark
 */
static int
pmembench_parse_clo(struct pmembench *pb, struct benchmark *bench,
		    struct clo_vec *clovec)
{
	if (!pb->scenario) {
		return benchmark_clo_parse(pb->argc, pb->argv, bench->clos,
					   bench->nclos, clovec);
	}

	if (pb->override_clos) {
		/*
		 * Use only ARRAY_SIZE(pmembench_clos) clos - these are the
		 * general clos and are placed at the beginning of the
		 * clos array.
		 */
		int ret = benchmark_override_clos_in_scenario(
			pb->scenario, pb->argc, pb->argv, bench->clos,
			ARRAY_SIZE(pmembench_clos));
		/* reset for the next benchmark in the config file */
		optind = 1;

		if (ret)
			return ret;
	}
	return benchmark_clo_parse_scenario(pb->scenario, bench->clos,
					    bench->nclos, clovec);
}

/*
 * pmembench_parse_affinity -- parse affinity list
 */
static int
pmembench_parse_affinity(const char *list, char **saveptr)
{
	char *str = nullptr;
	char *end;
	int cpu = 0;

	if (*saveptr) {
		str = strtok(nullptr, ";");
		if (str == nullptr) {
			/* end of list - we have to start over */
			free(*saveptr);
			*saveptr = nullptr;
		}
	}

	if (!*saveptr) {
		*saveptr = strdup(list);
		if (*saveptr == nullptr) {
			perror("strdup");
			return -1;
		}

		str = strtok(*saveptr, ";");
		if (str == nullptr)
			goto err;
	}

	if ((str == nullptr) || (*str == '\0'))
		goto err;

	cpu = strtol(str, &end, 10);

	if (*end != '\0')
		goto err;

	return cpu;
err:
	errno = EINVAL;
	perror("pmembench_parse_affinity");
	free(*saveptr);
	*saveptr = nullptr;
	return -1;
}

/*
 * pmembench_init_workers -- init benchmark's workers
 */
static int
pmembench_init_workers(struct benchmark_worker **workers,
		       struct benchmark *bench, struct benchmark_args *args)
{
	unsigned i;
	int ncpus = 0;
	char *saveptr = nullptr;
	int ret = 0;

	if (args->thread_affinity) {
		ncpus = sysconf(_SC_NPROCESSORS_ONLN);
		if (ncpus <= 0)
			return -1;
	}

	for (i = 0; i < args->n_threads; i++) {
		workers[i] = benchmark_worker_alloc();

		if (args->thread_affinity) {
			int cpu;
			os_cpu_set_t cpuset;

			if (args->affinity_list &&
			    *args->affinity_list != '\0') {
				cpu = pmembench_parse_affinity(
					args->affinity_list, &saveptr);
				if (cpu == -1) {
					ret = -1;
					goto end;
				}
			} else {
				cpu = (int)i;
			}

			assert(ncpus > 0);
			cpu %= ncpus;
			os_cpu_zero(&cpuset);
			os_cpu_set(cpu, &cpuset);
			errno = os_thread_setaffinity_np(&workers[i]->thread,
							 sizeof(os_cpu_set_t),
							 &cpuset);
			if (errno) {
				perror("os_thread_setaffinity_np");
				ret = -1;
				goto end;
			}
		}

		workers[i]->info.index = i;
		workers[i]->info.nops = args->n_ops_per_thread;
		workers[i]->info.opinfo = (struct operation_info *)calloc(
			args->n_ops_per_thread, sizeof(struct operation_info));
		size_t j;
		for (j = 0; j < args->n_ops_per_thread; j++) {
			workers[i]->info.opinfo[j].worker = &workers[i]->info;
			workers[i]->info.opinfo[j].args = args;
			workers[i]->info.opinfo[j].index = j;
		}
		workers[i]->bench = bench;
		workers[i]->args = args;
		workers[i]->func = pmembench_run_worker;
		workers[i]->init = bench->info->init_worker;
		workers[i]->exit = bench->info->free_worker;
		if (benchmark_worker_init(workers[i])) {
			fprintf(stderr,
				"thread number %u initialization failed\n", i);
			ret = -1;
			goto end;
		}
	}

end:
	free(saveptr);
	return ret;
}

/*
 * results_store -- store results of a single repeat
 */
static void
results_store(struct bench_results *res, struct benchmark_worker **workers,
	      unsigned nthreads, size_t nops)
{
	for (unsigned i = 0; i < nthreads; i++) {
		res->thres[i]->beg = workers[i]->info.beg;
		res->thres[i]->end = workers[i]->info.end;
		for (size_t j = 0; j < nops; j++) {
			res->thres[i]->end_op[j] =
				workers[i]->info.opinfo[j].end;
		}
	}
}

/*
 * compare_time -- compare time values
 */
static int
compare_time(const void *p1, const void *p2)
{
	const auto *t1 = (const benchmark_time_t *)p1;
	const auto *t2 = (const benchmark_time_t *)p2;

	return benchmark_time_compare(t1, t2);
}

/*
 * compare_doubles -- comparing function used for sorting
 */
static int
compare_doubles(const void *a1, const void *b1)
{
	const auto *a = (const double *)a1;
	const auto *b = (const double *)b1;
	return (*a > *b) - (*a < *b);
}

/*
 * compare_uint64t -- comparing function used for sorting
 */
static int
compare_uint64t(const void *a1, const void *b1)
{
	const auto *a = (const uint64_t *)a1;
	const auto *b = (const uint64_t *)b1;
	return (*a > *b) - (*a < *b);
}

/*
 * results_alloc -- prepare structure to store all benchmark results
 */
static struct total_results *
results_alloc(struct benchmark_args *args)
{
	struct total_results *total =
		(struct total_results *)malloc(sizeof(*total));
	assert(total != nullptr);
	total->nrepeats = args->repeats;
	total->nthreads = args->n_threads;
	total->nops = args->n_ops_per_thread;
	total->res = (struct bench_results *)malloc(args->repeats *
						    sizeof(*total->res));
	assert(total->res != nullptr);

	for (size_t i = 0; i < args->repeats; i++) {
		struct bench_results *res = &total->res[i];
		assert(args->n_threads != 0);
		res->thres = (struct thread_results **)malloc(
			args->n_threads * sizeof(*res->thres));
		assert(res->thres != nullptr);
		for (size_t j = 0; j < args->n_threads; j++) {
			res->thres[j] = (struct thread_results *)malloc(
				sizeof(*res->thres[j]) +
				args->n_ops_per_thread *
					sizeof(benchmark_time_t));
			assert(res->thres[j] != nullptr);
		}
	}

	return total;
}

/*
 * results_free -- release results structure
 */
static void
results_free(struct total_results *total)
{
	for (size_t i = 0; i < total->nrepeats; i++) {
		for (size_t j = 0; j < total->nthreads; j++)
			free(total->res[i].thres[j]);
		free(total->res[i].thres);
	}
	free(total->res);
	free(total);
}

/*
 * get_total_results -- return results of all repeats of scenario
 */
static void
get_total_results(struct total_results *tres)
{
	assert(tres->nrepeats != 0);
	assert(tres->nthreads != 0);
	assert(tres->nops != 0);

	/* reset results */
	memset(&tres->total, 0, sizeof(tres->total));
	memset(&tres->latency, 0, sizeof(tres->latency));

	tres->total.min = DBL_MAX;
	tres->total.max = DBL_MIN;
	tres->latency.min = UINT64_MAX;
	tres->latency.max = 0;

	/* allocate helper arrays */
	benchmark_time_t *tbeg =
		(benchmark_time_t *)malloc(tres->nthreads * sizeof(*tbeg));
	assert(tbeg != nullptr);
	benchmark_time_t *tend =
		(benchmark_time_t *)malloc(tres->nthreads * sizeof(*tend));
	assert(tend != nullptr);
	auto *totals = (double *)malloc(tres->nrepeats * sizeof(double));
	assert(totals != nullptr);

	/* estimate total penalty of getting time from the system */
	benchmark_time_t Tget;
	unsigned long long nsecs = tres->nops * Get_time_avg;
	benchmark_time_set(&Tget, nsecs);

	for (size_t i = 0; i < tres->nrepeats; i++) {
		struct bench_results *res = &tres->res[i];

		/* get start and end timestamps of each worker */
		for (size_t j = 0; j < tres->nthreads; j++) {
			tbeg[j] = res->thres[j]->beg;
			tend[j] = res->thres[j]->end;
		}

		/* sort start and end timestamps */
		qsort(tbeg, tres->nthreads, sizeof(benchmark_time_t),
		      compare_time);
		qsort(tend, tres->nthreads, sizeof(benchmark_time_t),
		      compare_time);

		/* calculating time interval between start and end time */
		benchmark_time_t Tbeg = tbeg[0];
		benchmark_time_t Tend = tend[tres->nthreads - 1];
		benchmark_time_t Ttot_ove;
		benchmark_time_diff(&Ttot_ove, &Tbeg, &Tend);
		/*
		 * subtract time used for getting the current time from the
		 * system
		 */
		benchmark_time_t Ttot;
		benchmark_time_diff(&Ttot, &Tget, &Ttot_ove);

		double Stot = benchmark_time_get_secs(&Ttot);

		if (Stot > tres->total.max)
			tres->total.max = Stot;
		if (Stot < tres->total.min)
			tres->total.min = Stot;

		tres->total.avg += Stot;
		totals[i] = Stot;
	}

	/* median */
	qsort(totals, tres->nrepeats, sizeof(double), compare_doubles);
	if (tres->nrepeats % 2) {
		tres->total.med = totals[tres->nrepeats / 2];
	} else {
		double m1 = totals[tres->nrepeats / 2];
		double m2 = totals[tres->nrepeats / 2 - 1];
		tres->total.med = (m1 + m2) / 2.0;
	}

	/* total average time */
	tres->total.avg /= (double)tres->nrepeats;

	/* number of operations per second */
	tres->nopsps =
		(double)tres->nops * (double)tres->nthreads / tres->total.avg;

	/* std deviation of total time */
	for (size_t i = 0; i < tres->nrepeats; i++) {
		double dev = (totals[i] - tres->total.avg);
		dev *= dev;

		tres->total.std_dev += dev;
	}

	tres->total.std_dev = sqrt(tres->total.std_dev / tres->nrepeats);

	/* latency */
	for (size_t i = 0; i < tres->nrepeats; i++) {
		struct bench_results *res = &tres->res[i];
		for (size_t j = 0; j < tres->nthreads; j++) {
			struct thread_results *thres = res->thres[j];
			benchmark_time_t *beg = &thres->beg;
			for (size_t o = 0; o < tres->nops; o++) {
				benchmark_time_t lat;
				benchmark_time_diff(&lat, beg,
						    &thres->end_op[o]);
				uint64_t nsecs = benchmark_time_get_nsecs(&lat);

				/* min, max latency */
				if (nsecs > tres->latency.max)
					tres->latency.max = nsecs;
				if (nsecs < tres->latency.min)
					tres->latency.min = nsecs;

				tres->latency.avg += nsecs;

				beg = &thres->end_op[o];
			}
		}
	}

	/* average latency */
	size_t count = tres->nrepeats * tres->nthreads * tres->nops;
	assert(count > 0);
	tres->latency.avg /= count;

	auto *ntotals = (uint64_t *)calloc(count, sizeof(uint64_t));
	assert(ntotals != nullptr);
	count = 0;

	/* std deviation of latency and percentiles */
	for (size_t i = 0; i < tres->nrepeats; i++) {
		struct bench_results *res = &tres->res[i];
		for (size_t j = 0; j < tres->nthreads; j++) {
			struct thread_results *thres = res->thres[j];
			benchmark_time_t *beg = &thres->beg;
			for (size_t o = 0; o < tres->nops; o++) {
				benchmark_time_t lat;
				benchmark_time_diff(&lat, beg,
						    &thres->end_op[o]);
				uint64_t nsecs = benchmark_time_get_nsecs(&lat);

				uint64_t dev = (nsecs - tres->latency.avg);
				dev *= dev;

				tres->latency.std_dev += dev;

				beg = &thres->end_op[o];

				ntotals[count] = nsecs;
				++count;
			}
		}
	}

	tres->latency.std_dev = sqrt(tres->latency.std_dev / count);

	/* find 50%, 99.0% and 99.9% percentiles */
	qsort(ntotals, count, sizeof(uint64_t), compare_uint64t);
	uint64_t p50_0 = count * 50 / 100;
	uint64_t p99_0 = count * 99 / 100;
	uint64_t p99_9 = count * 999 / 1000;
	tres->latency.pctl50_0p = ntotals[p50_0];
	tres->latency.pctl99_0p = ntotals[p99_0];
	tres->latency.pctl99_9p = ntotals[p99_9];
	free(ntotals);

	free(totals);
	free(tend);
	free(tbeg);
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
				printf(" [min: %" PRId64 "]", clo.type_int.min);
			if (clo.type_int.max != LONG_MAX)
				printf(" [max: %" PRId64 "]", clo.type_int.max);
		} else if (clo.type == CLO_TYPE_UINT) {
			if (clo.type_uint.min != 0)
				printf(" [min: %" PRIu64 "]",
				       clo.type_uint.min);
			if (clo.type_uint.max != ULONG_MAX)
				printf(" [max: %" PRIu64 "]",
				       clo.type_uint.max);
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
	size_t nclos = sizeof(pmembench_clos) / sizeof(struct benchmark_clo);
	pmembench_print_args(pmembench_clos, nclos);
	if (info->clos == nullptr)
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
	printf("\t\t\t\t\t\t[<config>[<scenario>[<common_args>]]]\n");
}

/*
 *  pmembench_print_version -- print version of framework
 */
static void
pmembench_print_version()
{
	printf("Benchmark framework - version %u.%u\n", version.major,
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
	printf("$ pmembench <config_file> <name_of_scenario_1> "
	       "<name_of_scenario_2> <common_args>\n");
	printf(" # runs the specified scenarios from config file and overwrites"
	       " the given common_args from the config file\n");
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
	size_t nclos = sizeof(pmembench_opts) / sizeof(struct benchmark_clo);
	pmembench_print_args(pmembench_opts, nclos);

	printf("\nAvaliable benchmarks:\n");
	struct benchmark *bench = nullptr;
	PMDK_LIST_FOREACH(bench, &benchmarks.head, next)
	printf("\t%-20s\t\t%s\n", bench->info->name, bench->info->brief);
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
	PMDK_LIST_FOREACH(bench, &benchmarks.head, next)
	{
		if (strcmp(name, bench->info->name) == 0)
			return bench;
	}

	return nullptr;
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
	struct benchmark_opts *opts = nullptr;
	struct clo_vec *clovec;
	size_t size, n_clos;
	size = sizeof(struct benchmark_opts);
	n_clos = ARRAY_SIZE(pmembench_opts);
	clovec = clo_vec_alloc(size);
	assert(clovec != nullptr);

	if (benchmark_clo_parse(argc, argv, pmembench_opts, n_clos, clovec)) {
		ret = -1;
		goto out;
	}

	opts = (struct benchmark_opts *)clo_vec_get_args(clovec, 0);
	if (opts == nullptr) {
		ret = -1;
		goto out;
	}

	if (opts->help)
		pmembench_print_help();
	if (opts->version)
		pmembench_print_version();

out:
	clo_vec_free(clovec);
	return ret;
}

/*
 * pmembench_remove_file -- remove file or directory if exists
 */
static int
pmembench_remove_file(const char *path)
{
	int ret = 0;
	os_stat_t status;
	char *tmp;

	int exists = util_file_exists(path);
	if (exists < 0)
		return -1;

	if (!exists)
		return 0;

	if (os_stat(path, &status) != 0)
		return 0;

	if (!(status.st_mode & S_IFDIR))
		return pmempool_rm(path, 0);

	struct dir_handle it;
	struct file_info info;

	if (util_file_dir_open(&it, path)) {
		return -1;
	}

	while (util_file_dir_next(&it, &info) == 0) {
		if (strcmp(info.filename, ".") == 0 ||
		    strcmp(info.filename, "..") == 0)
			continue;
		tmp = (char *)malloc(strlen(path) + strlen(info.filename) + 2);
		if (tmp == nullptr)
			return -1;
		sprintf(tmp, "%s" OS_DIR_SEP_STR "%s", path, info.filename);
		ret = info.is_dir ? pmembench_remove_file(tmp)
				  : util_unlink(tmp);
		free(tmp);
		if (ret != 0) {
			util_file_dir_close(&it);
			return ret;
		}
	}

	util_file_dir_close(&it);

	return util_file_dir_remove(path);
}

/*
 * pmembench_single_repeat -- runs benchmark ones
 */
static int
pmembench_single_repeat(struct benchmark *bench, struct benchmark_args *args,
			struct bench_results *res)
{
	int ret = 0;

	if (args->main_affinity != -1) {
		os_cpu_set_t cpuset;
		os_cpu_zero(&cpuset);
		os_thread_t self;
		os_thread_self(&self);
		os_cpu_set(args->main_affinity, &cpuset);
		errno = os_thread_setaffinity_np(&self, sizeof(os_cpu_set_t),
						 &cpuset);
		if (errno) {
			perror("os_thread_setaffinity_np");
			return -1;
		}
		sched_yield();
	}

	if (bench->info->rm_file && !args->is_dynamic_poolset) {
		ret = pmembench_remove_file(args->fname);
		if (ret != 0 && errno != ENOENT) {
			perror("removing file failed");

			return ret;
		}
	}

	if (bench->info->init) {
		if (bench->info->init(bench, args)) {
			warn("%s: initialization failed", bench->info->name);
			return -1;
		}
	}

	assert(bench->info->operation != nullptr);
	assert(args->n_threads != 0);

	struct benchmark_worker **workers;
	workers = (struct benchmark_worker **)malloc(
		args->n_threads * sizeof(struct benchmark_worker *));
	assert(workers != nullptr);

	if ((ret = pmembench_init_workers(workers, bench, args)) != 0) {
		goto out;
	}

	unsigned j;
	for (j = 0; j < args->n_threads; j++) {
		benchmark_worker_run(workers[j]);
	}

	for (j = 0; j < args->n_threads; j++) {
		benchmark_worker_join(workers[j]);
		if (workers[j]->ret != 0) {
			ret = workers[j]->ret;
			fprintf(stderr, "thread number %u failed\n", j);
		}
	}

	results_store(res, workers, args->n_threads, args->n_ops_per_thread);

	for (j = 0; j < args->n_threads; j++) {
		benchmark_worker_exit(workers[j]);

		free(workers[j]->info.opinfo);
		benchmark_worker_free(workers[j]);
	}

out:
	free(workers);

	if (bench->info->exit)
		bench->info->exit(bench, args);

	return ret;
}

/*
 * scale_up_min_exe_time -- scale up the number of operations to obtain an
 * execution time not smaller than the assumed minimal execution time
 */
int
scale_up_min_exe_time(struct benchmark *bench, struct benchmark_args *args,
		      struct total_results **total_results)
{
	const double min_exe_time = args->min_exe_time;
	struct total_results *total_res = *total_results;
	total_res->nrepeats = 1;
	do {
		/*
		 * run single benchmark repeat to probe execution time
		 */
		int ret = pmembench_single_repeat(bench, args,
						  &total_res->res[0]);
		if (ret != 0)
			return 1;
		get_total_results(total_res);
		if (min_exe_time < total_res->total.min + MIN_EXE_TIME_E)
			break;

		/*
		 * scale up number of operations to get assumed minimal
		 * execution time
		 */
		args->n_ops_per_thread = (size_t)(
			(double)args->n_ops_per_thread *
			(min_exe_time + MIN_EXE_TIME_E) / total_res->total.min);

		results_free(total_res);
		*total_results = results_alloc(args);
		assert(*total_results != nullptr);
		total_res = *total_results;
		total_res->nrepeats = 1;
	} while (1);
	total_res->nrepeats = args->repeats;
	return 0;
}

/*
 * is_absolute_path_to_directory -- checks if passed argument is absolute
 * path to directory
 */
static bool
is_absolute_path_to_directory(const char *path)
{
	os_stat_t sb;

	return util_is_absolute_path(path) && os_stat(path, &sb) == 0 &&
		S_ISDIR(sb.st_mode);
}

/*
 * pmembench_run -- runs one benchmark. Parses arguments and performs
 * specific functions.
 */
static int
pmembench_run(struct pmembench *pb, struct benchmark *bench)
{
	enum file_type type;
	char old_wd[PATH_MAX];
	int ret = 0;

	struct benchmark_args *args = nullptr;
	struct total_results *total_res = nullptr;
	struct latency *stats = nullptr;
	double *workers_times = nullptr;

	struct clo_vec *clovec = nullptr;

	assert(bench->info != nullptr);
	pmembench_merge_clos(bench);

	/*
	 * Check if PMEMBENCH_DIR env var is set and change
	 * the working directory accordingly.
	 */
	char *wd = os_getenv("PMEMBENCH_DIR");
	if (wd != nullptr) {
		/* get current dir name */
		if (getcwd(old_wd, PATH_MAX) == nullptr) {
			perror("getcwd");
			ret = -1;
			goto out_release_clos;
		}
		os_stat_t stat_buf;
		if (os_stat(wd, &stat_buf) != 0) {
			perror("os_stat");
			ret = -1;
			goto out_release_clos;
		}
		if (!S_ISDIR(stat_buf.st_mode)) {
			warn("PMEMBENCH_DIR is not a directory: %s", wd);
			ret = -1;
			goto out_release_clos;
		}
		if (chdir(wd)) {
			perror("chdir(wd)");
			ret = -1;
			goto out_release_clos;
		}
	}

	if (bench->info->pre_init) {
		if (bench->info->pre_init(bench)) {
			warn("%s: pre-init failed", bench->info->name);
			ret = -1;
			goto out_old_wd;
		}
	}

	clovec = clo_vec_alloc(bench->args_size);
	assert(clovec != nullptr);

	if (pmembench_parse_clo(pb, bench, clovec)) {
		warn("%s: parsing command line arguments failed",
		     bench->info->name);
		ret = -1;
		goto out_release_args;
	}

	args = (struct benchmark_args *)clo_vec_get_args(clovec, 0);
	if (args == nullptr) {
		warn("%s: parsing command line arguments failed",
		     bench->info->name);
		ret = -1;
		goto out_release_args;
	}

	if (args->help) {
		pmembench_print_help_single(bench);
		goto out;
	}

	if (strlen(args->fname) > PATH_MAX) {
		warn("Filename too long");
		ret = -1;
		goto out;
	}

	type = util_file_get_type(args->fname);
	if (type == OTHER_ERROR) {
		fprintf(stderr, "could not check type of file %s\n",
			args->fname);
		return -1;
	}

	pmembench_print_header(pb, bench, clovec);

	size_t args_i;
	for (args_i = 0; args_i < clovec->nargs; args_i++) {

		args = (struct benchmark_args *)clo_vec_get_args(clovec,
								 args_i);
		if (args == nullptr) {
			warn("%s: parsing command line arguments failed",
			     bench->info->name);
			ret = -1;
			goto out;
		}

		args->opts = (void *)((uintptr_t)args +
				      sizeof(struct benchmark_args));

		if (args->is_dynamic_poolset) {
			if (!bench->info->allow_poolset) {
				fprintf(stderr,
					"dynamic poolset not supported\n");
				goto out;
			}

			if (!is_absolute_path_to_directory(args->fname)) {
				fprintf(stderr,
					"path must be absolute and point to a directory\n");
				goto out;
			}
		} else {
			args->is_poolset =
				util_is_poolset_file(args->fname) == 1;
			if (args->is_poolset) {
				if (!bench->info->allow_poolset) {
					fprintf(stderr,
						"poolset files not supported\n");
					goto out;
				}
				args->fsize = util_poolset_size(args->fname);
				if (!args->fsize) {
					fprintf(stderr,
						"invalid size of poolset\n");
					goto out;
				}
			} else if (type == TYPE_DEVDAX) {
				args->fsize = util_file_get_size(args->fname);

				if (!args->fsize) {
					fprintf(stderr,
						"invalid size of device dax\n");
					goto out;
				}
			}
		}

		unsigned n_threads_copy = args->n_threads;
		args->n_threads =
			!bench->info->multithread ? 1 : args->n_threads;
		size_t n_ops_per_thread_copy = args->n_ops_per_thread;
		args->n_ops_per_thread =
			!bench->info->multiops ? 1 : args->n_ops_per_thread;

		stats = (struct latency *)calloc(args->repeats,
						 sizeof(struct latency));
		assert(stats != nullptr);
		workers_times = (double *)calloc(
			args->n_threads * args->repeats, sizeof(double));
		assert(workers_times != nullptr);
		total_res = results_alloc(args);
		assert(total_res != nullptr);

		unsigned i = 0;
		if (args->min_exe_time != 0 && bench->info->multiops) {
			ret = scale_up_min_exe_time(bench, args, &total_res);
			if (ret != 0)
				goto out;
			i = 1;
		}

		for (; i < args->repeats; i++) {
			ret = pmembench_single_repeat(bench, args,
						      &total_res->res[i]);
			if (ret != 0)
				goto out;
		}

		get_total_results(total_res);
		pmembench_print_results(bench, args, total_res);

		args->n_ops_per_thread = n_ops_per_thread_copy;
		args->n_threads = n_threads_copy;

		results_free(total_res);
		free(stats);
		free(workers_times);
		total_res = nullptr;
		stats = nullptr;
		workers_times = nullptr;
	}
out:
	if (total_res)
		results_free(total_res);
	if (stats)
		free(stats);
	if (workers_times)
		free(workers_times);
out_release_args:
	clo_vec_free(clovec);

out_old_wd:
	/* restore the original working directory */
	if (wd != nullptr) { /* Only if PMEMBENCH_DIR env var was defined */
		if (chdir(old_wd)) {
			perror("chdir(old_wd)");
			ret = -1;
		}
	}

out_release_clos:
	pmembench_release_clos(bench);
	return ret;
}

/*
 * pmembench_free_benchmarks -- release all benchmarks
 */
static void __attribute__((destructor)) pmembench_free_benchmarks(void)
{
	while (!PMDK_LIST_EMPTY(&benchmarks.head)) {
		struct benchmark *bench = PMDK_LIST_FIRST(&benchmarks.head);
		PMDK_LIST_REMOVE(bench, next);
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
	if (nullptr == bench) {
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
	FOREACH_SCENARIO(scenario, ss)
	{
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
	struct scenarios *ss = nullptr;
	struct config_reader *cr = config_reader_alloc();
	assert(cr != nullptr);

	int ret = 0;

	if ((ret = config_reader_read(cr, config)))
		goto out;

	if ((ret = config_reader_get_scenarios(cr, &ss)))
		goto out;

	assert(ss != nullptr);

	if (pb->argc == 1) {
		if ((ret = pmembench_run_scenarios(pb, ss)) != 0)
			goto out_scenarios;
	} else {
		/* Skip the config file name in cmd line params */
		int tmp_argc = pb->argc - 1;
		char **tmp_argv = pb->argv + 1;

		if (!contains_scenarios(tmp_argc, tmp_argv, ss)) {
			/* no scenarios in cmd line arguments - parse params */
			pb->override_clos = true;
			if ((ret = pmembench_run_scenarios(pb, ss)) != 0)
				goto out_scenarios;
		} else { /* scenarios in cmd line */
			struct scenarios *cmd_ss = scenarios_alloc();
			assert(cmd_ss != nullptr);

			int parsed_scenarios = clo_get_scenarios(
				tmp_argc, tmp_argv, ss, cmd_ss);
			if (parsed_scenarios < 0)
				goto out_cmd;

			/*
			 * If there are any cmd line args left, treat
			 * them as config file params override.
			 */
			if (tmp_argc - parsed_scenarios)
				pb->override_clos = true;

			/*
			 * Skip the scenarios in the cmd line,
			 * pmembench_run_scenarios does not expect them and will
			 * fail otherwise.
			 */
			pb->argc -= parsed_scenarios;
			pb->argv += parsed_scenarios;
			ret = pmembench_run_scenarios(pb, cmd_ss);

		out_cmd:
			scenarios_free(cmd_ss);
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
	util_init();
	util_mmap_init();

	/*
	 * Parse common command line arguments and
	 * benchmark's specific ones.
	 */
	if (argc < 2) {
		pmembench_print_usage();
		exit(EXIT_FAILURE);
	}

	int ret = 0;
	int fexists;
	struct benchmark *bench;
	struct pmembench *pb = (struct pmembench *)calloc(1, sizeof(*pb));
	assert(pb != nullptr);
	Get_time_avg = benchmark_get_avg_get_time();

	pb->argc = --argc;
	pb->argv = ++argv;

	char *bench_name = pb->argv[0];
	if (nullptr == bench_name) {
		ret = -1;
		goto out;
	}

	fexists = os_access(bench_name, R_OK) == 0;
	bench = pmembench_get_bench(bench_name);
	if (nullptr != bench)
		ret = pmembench_run(pb, bench);
	else if (fexists)
		ret = pmembench_run_config(pb, bench_name);
	else if ((ret = pmembench_parse_opts(pb)) != 0) {
		pmembench_print_usage();
		goto out;
	}

out:
	free(pb);

	util_mmap_fini();
	return ret;
}
