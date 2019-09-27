/*
 * Copyright 2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *      * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *      * Neither the name of the copyright holder nor the names of its
 *        contributors may be used to endorse or promote products derived
 *        from this software without specific prior written permission.
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
