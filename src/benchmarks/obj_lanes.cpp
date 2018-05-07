/*
 * Copyright 2015-2018, Intel Corporation
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
 * obj_lanes.cpp -- lane benchmark definition
 */

#include <cassert>
#include <cerrno>
#include <unistd.h>

#include "benchmark.hpp"
#include "file.h"
#include "libpmemobj.h"

/* an internal libpmemobj code */
extern "C" {
#include "lane.h"
}

/*
 * The number of times to repeat the operation, used to get more accurate
 * results, because the operation time was minimal compared to the framework
 * overhead.
 */
#define OPERATION_REPEAT_COUNT 10000

/*
 * prog_args - command line parsed arguments
 */
struct prog_args {
	char *lane_section_name; /* lane section to be held */
};

/*
 * obj_bench - variables used in benchmark, passed within functions
 */
struct obj_bench {
	PMEMobjpool *pop;		  /* persistent pool handle */
	struct prog_args *pa;		  /* prog_args structure */
	enum lane_section_type lane_type; /* lane section to be held */
};

/*
 * parse_lane_section -- parses command line "--lane_section" and returns
 * proper lane section type enum
 */
static enum lane_section_type
parse_lane_section(const char *arg)
{
	if (strcmp(arg, "allocator") == 0)
		return LANE_SECTION_ALLOCATOR;
	else if (strcmp(arg, "list") == 0)
		return LANE_SECTION_LIST;
	else if (strcmp(arg, "transaction") == 0)
		return LANE_SECTION_TRANSACTION;
	else
		return MAX_LANE_SECTION;
}

/*
 * lanes_init -- benchmark initialization
 */
static int
lanes_init(struct benchmark *bench, struct benchmark_args *args)
{
	assert(bench != nullptr);
	assert(args != nullptr);
	assert(args->opts != nullptr);

	auto *ob = (struct obj_bench *)malloc(sizeof(struct obj_bench));
	if (ob == nullptr) {
		perror("malloc");
		return -1;
	}
	pmembench_set_priv(bench, ob);

	ob->pa = (struct prog_args *)args->opts;
	size_t psize;

	if (args->is_poolset || util_file_is_device_dax(args->fname))
		psize = 0;
	else
		psize = PMEMOBJ_MIN_POOL;

	/* create pmemobj pool */
	ob->pop = pmemobj_create(args->fname, "obj_lanes", psize, args->fmode);
	if (ob->pop == nullptr) {
		fprintf(stderr, "%s\n", pmemobj_errormsg());
		goto err;
	}

	ob->lane_type = parse_lane_section(ob->pa->lane_section_name);
	if (ob->lane_type == MAX_LANE_SECTION) {
		fprintf(stderr, "wrong lane type\n");
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
	struct lane_section *section;

	for (int i = 0; i < OPERATION_REPEAT_COUNT; i++) {
		lane_hold(ob->pop, &section, ob->lane_type);

		lane_release(ob->pop);
	}

	return 0;
}
static struct benchmark_clo lanes_clo[1];
static struct benchmark_info lanes_info;

CONSTRUCTOR(obj_lines_constructor)
void
obj_lines_constructor(void)
{
	lanes_clo[0].opt_short = 's';
	lanes_clo[0].opt_long = "lane_section";
	lanes_clo[0].descr = "The lane section type: allocator,"
			     " list or transaction";
	lanes_clo[0].type = CLO_TYPE_STR;
	lanes_clo[0].off =
		clo_field_offset(struct prog_args, lane_section_name);
	lanes_clo[0].def = "allocator";

	lanes_info.name = "obj_lanes";
	lanes_info.brief = "Benchmark for internal lanes "
			   "operation";
	lanes_info.init = lanes_init;
	lanes_info.exit = lanes_exit;
	lanes_info.multithread = true;
	lanes_info.multiops = true;
	lanes_info.operation = lanes_op;
	lanes_info.measure_time = true;
	lanes_info.clos = lanes_clo;
	lanes_info.nclos = ARRAY_SIZE(lanes_clo);
	lanes_info.opts_size = sizeof(struct prog_args);
	lanes_info.rm_file = true;
	lanes_info.allow_poolset = true;
	REGISTER_BENCHMARK(lanes_info);
}
