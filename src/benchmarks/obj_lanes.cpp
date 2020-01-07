// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * obj_lanes.cpp -- lane benchmark definition
 */

#include <cassert>
#include <cerrno>
#include <unistd.h>

#include "benchmark.hpp"
#include "file.h"
#include "libpmemobj.h"

/* an internal libpmemobj code */
#include "lane.h"

/*
 * The number of times to repeat the operation, used to get more accurate
 * results, because the operation time was minimal compared to the framework
 * overhead.
 */
#define OPERATION_REPEAT_COUNT 10000

/*
 * obj_bench - variables used in benchmark, passed within functions
 */
struct obj_bench {
	PMEMobjpool *pop;     /* persistent pool handle */
	struct prog_args *pa; /* prog_args structure */
};

/*
 * lanes_init -- benchmark initialization
 */
static int
lanes_init(struct benchmark *bench, struct benchmark_args *args)
{
	assert(bench != nullptr);
	assert(args != nullptr);
	assert(args->opts != nullptr);

	enum file_type type = util_file_get_type(args->fname);
	if (type == OTHER_ERROR) {
		fprintf(stderr, "could not check type of file %s\n",
			args->fname);
		return -1;
	}

	auto *ob = (struct obj_bench *)malloc(sizeof(struct obj_bench));
	if (ob == nullptr) {
		perror("malloc");
		return -1;
	}
	pmembench_set_priv(bench, ob);

	ob->pa = (struct prog_args *)args->opts;
	size_t psize;

	if (args->is_poolset || type == TYPE_DEVDAX)
		psize = 0;
	else
		psize = PMEMOBJ_MIN_POOL;

	/* create pmemobj pool */
	ob->pop = pmemobj_create(args->fname, "obj_lanes", psize, args->fmode);
	if (ob->pop == nullptr) {
		fprintf(stderr, "%s\n", pmemobj_errormsg());
		goto err;
	}

	return 0;

err:
	free(ob);
	return -1;
}

/*
 * lanes_exit -- benchmark clean up
 */
static int
lanes_exit(struct benchmark *bench, struct benchmark_args *args)
{
	auto *ob = (struct obj_bench *)pmembench_get_priv(bench);

	pmemobj_close(ob->pop);
	free(ob);

	return 0;
}

/*
 * lanes_op -- performs the lane hold and release operations
 */
static int
lanes_op(struct benchmark *bench, struct operation_info *info)
{
	auto *ob = (struct obj_bench *)pmembench_get_priv(bench);
	struct lane *lane;

	for (int i = 0; i < OPERATION_REPEAT_COUNT; i++) {
		lane_hold(ob->pop, &lane);

		lane_release(ob->pop);
	}

	return 0;
}
static struct benchmark_info lanes_info;

CONSTRUCTOR(obj_lines_constructor)
void
obj_lines_constructor(void)
{
	lanes_info.name = "obj_lanes";
	lanes_info.brief = "Benchmark for internal lanes "
			   "operation";
	lanes_info.init = lanes_init;
	lanes_info.exit = lanes_exit;
	lanes_info.multithread = true;
	lanes_info.multiops = true;
	lanes_info.operation = lanes_op;
	lanes_info.measure_time = true;
	lanes_info.clos = NULL;
	lanes_info.nclos = 0;
	lanes_info.opts_size = 0;
	lanes_info.rm_file = true;
	lanes_info.allow_poolset = true;
	REGISTER_BENCHMARK(lanes_info);
}
