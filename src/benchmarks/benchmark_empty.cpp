// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * benchmark_empty.cpp -- empty template for benchmarks
 */

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <unistd.h>

#include "benchmark.hpp"
#include "file.h"
#include "libpmemobj.h"

/*
 * prog_args -- benchmark specific command line options
 */
struct prog_args {
	int my_value;
};

/*
 * obj_bench -- benchmark context
 */
struct obj_bench {
	struct prog_args *pa; /* prog_args structure */
};

/*
 * benchmark_empty_op -- actual benchmark operation
 */
static int
benchmark_empty_op(struct benchmark *bench, struct operation_info *info)
{
	return 0;
}

/*
 * benchmark_empty_init -- initialization function
 */
static int
benchmark_empty_init(struct benchmark *bench, struct benchmark_args *args)
{
	assert(bench != nullptr);
	assert(args != nullptr);
	assert(args->opts != nullptr);

	return 0;
}

/*
 * benchmark_empty_exit -- benchmark cleanup function
 */
static int
benchmark_empty_exit(struct benchmark *bench, struct benchmark_args *args)
{
	return 0;
}

static struct benchmark_clo benchmark_empty_clo[0];

/* Stores information about benchmark. */
static struct benchmark_info benchmark_empty_info;
CONSTRUCTOR(benchmark_empty_constructor)
void
benchmark_empty_constructor(void)
{
	benchmark_empty_info.name = "benchmark_empty";
	benchmark_empty_info.brief = "Benchmark for benchmark_empty() "
				     "operation";
	benchmark_empty_info.init = benchmark_empty_init;
	benchmark_empty_info.exit = benchmark_empty_exit;
	benchmark_empty_info.multithread = true;
	benchmark_empty_info.multiops = true;
	benchmark_empty_info.operation = benchmark_empty_op;
	benchmark_empty_info.measure_time = true;
	benchmark_empty_info.clos = benchmark_empty_clo;
	benchmark_empty_info.nclos = ARRAY_SIZE(benchmark_empty_clo);
	benchmark_empty_info.opts_size = sizeof(struct prog_args);
	benchmark_empty_info.rm_file = true;
	benchmark_empty_info.allow_poolset = true;
	REGISTER_BENCHMARK(benchmark_empty_info);
};
