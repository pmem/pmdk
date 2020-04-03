// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

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
 * POOL_SIZE_COEFFICIENT -- pool has to hold every allocated object with
 * its snapshot (1 + 1), plus 0.5 because of fragmentation
 */
#define POOL_SIZE_COEFFICIENT (1 + 1 + 0.5)

/*
 * MAX_ALLOC_SIZE -- maximum size of one allocation (128 MiB)
 */
#define MAX_ALLOC_SIZE (1024 * 1024 * 128)

/*
 * ranged_obj -- ranged object
 */
struct ranged_obj {
	void *ptr;   /* address of allocated object */
	size_t size; /* size of allocated object */
};

/*
 * obj_bench_args -- benchmark specific command line options
 */
struct obj_bench_args {
	uint64_t nranges;  /* number of allocated objects */
	bool shuffle_objs; /* shuffles the array of allocated objects */
};

/*
 * obj_bench -- benchmark context
 */
struct obj_bench {
	PMEMobjpool *pop;	   /* persistent pool handle */
	struct ranged_obj *ranges; /* array of ranges */
	size_t obj_size;	   /* size of a single range */
	uint64_t nranges;	   /* number of ranges */
	uint64_t nallocs;	   /* number of allocations */
	bool shuffle_objs;	   /* shuffles array of ranges */
	rng_t rng;		   /* PRNG */
};

/*
 * shuffle_ranges -- randomly shuffles elements in an array
 * to avoid sequential pattern in the transaction loop
 */
static void
shuffle_ranges(struct ranged_obj *ranged, uint64_t nranges, rng_t *rng)
{
	struct ranged_obj tmp;
	uint64_t dest;

	for (uint64_t n = 0; n < nranges; ++n) {
		dest = RRAND_R(rng, nranges - 1, 0);
		tmp = ranged[n];
		ranged[n] = ranged[dest];
		ranged[dest] = tmp;
	}
}

/*
 * init_ranges -- allocate persistent objects and carve ranges from them
 */
static int
init_ranges(struct obj_bench *ob)
{
	assert(ob->nranges != 0);
	ob->ranges = (struct ranged_obj *)malloc((ob->nranges) *
						 sizeof(struct ranged_obj));
	if (!ob->ranges) {
		perror("malloc");
		return -1;
	}

	size_t nranges_per_object = MAX_ALLOC_SIZE / ob->obj_size;

	for (size_t i = 0, n = 0; n < ob->nranges && i < ob->nallocs; i++) {
		PMEMoid oid;
		if (pmemobj_alloc(ob->pop, &oid, MAX_ALLOC_SIZE, 0, nullptr,
				  nullptr)) {
			perror("pmemobj_alloc");
			goto err;
		}

		for (size_t j = 0; j < nranges_per_object; j++) {
			void *ptr = (char *)pmemobj_direct(oid) +
				(j * ob->obj_size);
			struct ranged_obj range = {ptr, ob->obj_size};
			ob->ranges[n++] = range;
			if (n == ob->nranges)
				break;
		}
	}

	if (ob->shuffle_objs == true)
		shuffle_ranges(ob->ranges, ob->nranges, &ob->rng);

	return 0;

err:
	free(ob->ranges);
	return -1;
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

	/* let's calculate number of allocations */
	ob->nallocs = (args->dsize * bargs->nranges / MAX_ALLOC_SIZE) + 1;

	size_t pool_size;

	if (args->is_poolset || type == TYPE_DEVDAX)
		pool_size = 0;
	else {
		pool_size =
			ob->nallocs * MAX_ALLOC_SIZE * POOL_SIZE_COEFFICIENT;
	}

	/* create pmemobj pool */
	ob->pop = pmemobj_create(args->fname, LAYOUT_NAME, pool_size,
				 args->fmode);
	if (ob->pop == nullptr) {
		fprintf(stderr, "%s\n", pmemobj_errormsg());
		goto err;
	}

	ob->nranges = bargs->nranges;
	ob->obj_size = args->dsize;
	ob->shuffle_objs = bargs->shuffle_objs;
	randomize_r(&ob->rng, args->seed);

	if (init_ranges(ob))
		goto err_pop_close;

	pmembench_set_priv(bench, ob);

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
		for (size_t i = 0; i < ob->nranges; i++) {
			struct ranged_obj *r = &ob->ranges[i];
			pmemobj_tx_add_range_direct(r->ptr, r->size);
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

	pmemobj_close(ob->pop);
	free(ob->ranges);
	free(ob);

	return 0;
}

static struct benchmark_clo tx_add_range_clo[2];

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
		clo_field_offset(struct obj_bench_args, nranges);
	tx_add_range_clo[0].type = CLO_TYPE_UINT;
	tx_add_range_clo[0].type_uint.size =
		clo_field_size(struct obj_bench_args, nranges);
	tx_add_range_clo[0].type_uint.base = CLO_INT_BASE_DEC;
	tx_add_range_clo[0].type_uint.min = 1;
	tx_add_range_clo[0].type_uint.max = ULONG_MAX;

	tx_add_range_clo[1].opt_short = 's';
	tx_add_range_clo[1].opt_long = "shuffle";
	tx_add_range_clo[1].descr =
		"Use shuffle objects - "
		"randomly shuffles array of allocated objects";
	tx_add_range_clo[1].def = "false";
	tx_add_range_clo[1].off =
		clo_field_offset(struct obj_bench_args, shuffle_objs);
	tx_add_range_clo[1].type = CLO_TYPE_FLAG;

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
