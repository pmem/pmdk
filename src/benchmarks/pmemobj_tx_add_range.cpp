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
 * pmemobj_tx_add_range.cpp -- pmemobj_tx_add_range benchmarks definition
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

#define LAYOUT_NAME "tx_add_range_benchmark"

/*
 * RANGE_COEFFICIENT -- pool has to hold every allocated object with
 * its snapshot (1 + 1), plus 0.5 because of fragmentation
 */
#define RANGE_COEFFICIENT (1 + 1 + 0.5)

/*
 * obj_bench_args -- benchmark specific command line options
 */
struct obj_bench_args {
	uint64_t nranged; /* number of allocated objects */
};

/*
 * obj_bench -- benchmark context
 */
struct obj_bench {
	PMEMobjpool *pop; /* persistent pool handle */
	PMEMoid *ranged;  /* array of allocated objects */
	size_t obj_size;  /* size of allocated object */
	uint64_t nranged; /* number of allocated objects */
};

/*
 * init_objects -- allocate persistent objects
 */
static int
init_objects(struct obj_bench *ob)
{
	assert(ob->nranged != 0);
	ob->ranged = (PMEMoid *)malloc(ob->nranged * sizeof(PMEMoid));

	if (!ob->ranged) {
		perror("malloc");
		return -1;
	}

	for (uint64_t i = 0; i < ob->nranged; i++) {
		PMEMoid *oid = &ob->ranged[i];
		if (pmemobj_alloc(ob->pop, oid, ob->obj_size, 0, nullptr,
				  nullptr)) {
			perror("pmemobj_alloc");
			return -1;
		}
	}

	return 0;
}

/*
 * tx_add_range_init -- initialization function
 */
static int
tx_add_range_init(struct benchmark *bench, struct benchmark_args *args)
{
	assert(bench != nullptr);
	assert(args != nullptr);
	assert(args->opts != nullptr);

	struct obj_bench_args *bargs = (struct obj_bench_args *)args->opts;

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

	size_t psize;

	if (args->is_poolset || type == TYPE_DEVDAX)
		psize = 0;
	else {
		psize = bargs->nranged * args->dsize * RANGE_COEFFICIENT;
		if (psize < PMEMOBJ_MIN_POOL)
			psize = PMEMOBJ_MIN_POOL;
	}

	/* create pmemobj pool */
	ob->pop = pmemobj_create(args->fname, LAYOUT_NAME, psize, args->fmode);
	if (ob->pop == nullptr) {
		fprintf(stderr, "%s\n", pmemobj_errormsg());
		goto err;
	}

	ob->nranged = bargs->nranged;
	ob->obj_size = args->dsize;

	if (init_objects(ob))
		goto err_pop_close;

	return 0;

err_pop_close:
	pmemobj_close(ob->pop);
err:
	free(ob);
	return -1;
}

/*
 * tx_add_range_op -- actual benchmark operation
 */
static int
tx_add_range_op(struct benchmark *bench, struct operation_info *info)
{
	auto *ob = (struct obj_bench *)pmembench_get_priv(bench);
	int ret = 0;

	TX_BEGIN(ob->pop)
	{
		for (uint64_t i = 0; i < ob->nranged; i++) {
			pmemobj_tx_add_range(ob->ranged[i], 0, ob->obj_size);
		}
	}
	TX_ONABORT
	{
		fprintf(stderr, "transaction failed\n");
		ret = -1;
	}
	TX_END

	return ret;
}

/*
 * tx_add_range_exit -- benchmark cleanup function
 */
static int
tx_add_range_exit(struct benchmark *bench, struct benchmark_args *args)
{
	auto *ob = (struct obj_bench *)pmembench_get_priv(bench);

	for (size_t i = 0; i < ob->nranged; ++i) {
		pmemobj_free(&ob->ranged[i]);
	}

	pmemobj_close(ob->pop);
	free(ob->ranged);
	free(ob);

	return 0;
}

static struct benchmark_clo tx_add_range_clo[1];

/* Stores information about benchmark. */
static struct benchmark_info tx_add_range_info;
CONSTRUCTOR(tx_add_range_constructor)
void
tx_add_range_constructor(void)
{
	tx_add_range_clo[0].opt_short = 0;
	tx_add_range_clo[0].opt_long = "num-of-ranges";
	tx_add_range_clo[0].descr = "Number of ranges";
	tx_add_range_clo[0].def = "1000";
	tx_add_range_clo[0].off =
		clo_field_offset(struct obj_bench_args, nranged);
	tx_add_range_clo[0].type = CLO_TYPE_UINT;
	tx_add_range_clo[0].type_uint.size =
		clo_field_size(struct obj_bench_args, nranged);
	tx_add_range_clo[0].type_uint.base = CLO_INT_BASE_DEC;
	tx_add_range_clo[0].type_uint.min = 1;
	tx_add_range_clo[0].type_uint.max = ULONG_MAX;

	tx_add_range_info.name = "pmemobj_tx_add_range";
	tx_add_range_info.brief = "Benchmark for pmemobj_tx_add_range() "
				  "operation";
	tx_add_range_info.init = tx_add_range_init;
	tx_add_range_info.exit = tx_add_range_exit;
	tx_add_range_info.multithread = true;
	tx_add_range_info.multiops = true;
	tx_add_range_info.operation = tx_add_range_op;
	tx_add_range_info.measure_time = true;
	tx_add_range_info.clos = tx_add_range_clo;
	tx_add_range_info.nclos = ARRAY_SIZE(tx_add_range_clo);
	tx_add_range_info.opts_size = sizeof(struct obj_bench_args);
	tx_add_range_info.rm_file = true;
	tx_add_range_info.allow_poolset = true;
	REGISTER_BENCHMARK(tx_add_range_info);
};
