// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * pmemobj_persist.cpp -- pmemobj persist benchmarks definition
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
#include "util.h"

/*
 * The factor used for PMEM pool size calculation, accounts for metadata,
 * fragmentation and etc.
 */
#define FACTOR 3

/* The minimum allocation size that pmalloc can perform */
#define ALLOC_MIN_SIZE 64

/* OOB and allocation header size */
#define OOB_HEADER_SIZE 64

#define CONST_B 0xFF

/*
 * prog_args -- benchmark specific command line options
 */
struct prog_args {
	size_t minsize;       /* minimum size for random allocation size */
	bool use_random_size; /* if set, use random size allocations */
	bool no_warmup;       /* do not do warmup */
	unsigned seed;	/* seed for random numbers */
};

/*
 * obj_bench -- benchmark context
 */
struct obj_bench {
	PMEMobjpool *pop;     /* persistent pool handle */
	struct prog_args *pa; /* prog_args structure */
	PMEMoid *oids;	/* vector of allocated objects */
	void **ptrs;	  /* pointers to allocated objects */
	uint64_t nobjs;       /* number of allocated objects */
	size_t obj_size;      /* size of each allocated objects */
	int const_b;	  /* memset() value */
};

/*
 * init_objects -- allocate persistent objects and obtain direct pointers
 */
static int
init_objects(struct obj_bench *ob)
{
	assert(ob->nobjs != 0);
	ob->oids = (PMEMoid *)malloc(ob->nobjs * sizeof(*ob->oids));
	if (!ob->oids) {
		perror("malloc");
		return -1;
	}

	ob->ptrs = (void **)malloc(ob->nobjs * sizeof(*ob->ptrs));
	if (!ob->ptrs) {
		perror("malloc");
		goto err_malloc;
	}

	for (uint64_t i = 0; i < ob->nobjs; i++) {
		PMEMoid oid;
		void *ptr;
		if (pmemobj_alloc(ob->pop, &oid, ob->obj_size, 0, nullptr,
				  nullptr)) {
			perror("pmemobj_alloc");
			goto err_palloc;
		}
		ptr = pmemobj_direct(oid);
		if (!ptr) {
			perror("pmemobj_direct");
			goto err_palloc;
		}
		ob->oids[i] = oid;
		ob->ptrs[i] = ptr;
	}

	return 0;

err_palloc:
	free(ob->ptrs);
err_malloc:
	free(ob->oids);
	return -1;
}

/*
 * do_warmup -- does the warmup by writing the whole pool area
 */
static void
do_warmup(struct obj_bench *ob)
{
	for (uint64_t i = 0; i < ob->nobjs; ++i) {
		memset(ob->ptrs[i], 0, ob->obj_size);
		pmemobj_persist(ob->pop, ob->ptrs[i], ob->obj_size);
	}
}

/*
 * obj_persist_op -- actual benchmark operation
 */
static int
obj_persist_op(struct benchmark *bench, struct operation_info *info)
{
	auto *ob = (struct obj_bench *)pmembench_get_priv(bench);
	uint64_t idx = info->worker->index * info->args->n_ops_per_thread +
		info->index;

	assert(idx < ob->nobjs);

	void *ptr = ob->ptrs[idx];
	memset(ptr, ob->const_b, ob->obj_size);
	pmemobj_persist(ob->pop, ptr, ob->obj_size);

	return 0;
}

/*
 * obj_persist_init -- initialization function
 */
static int
obj_persist_init(struct benchmark *bench, struct benchmark_args *args)
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

	auto *pa = (struct prog_args *)args->opts;
	size_t poolsize;
	if (pa->minsize >= args->dsize) {
		fprintf(stderr, "Wrong params - allocation size\n");
		return -1;
	}

	auto *ob = (struct obj_bench *)malloc(sizeof(struct obj_bench));
	if (ob == nullptr) {
		perror("malloc");
		return -1;
	}
	pmembench_set_priv(bench, ob);

	ob->pa = pa;

	/* initialize memset() value */
	ob->const_b = CONST_B;

	ob->nobjs = args->n_ops_per_thread * args->n_threads;

	/* Create pmemobj pool. */
	ob->obj_size = args->dsize;
	if (ob->obj_size < ALLOC_MIN_SIZE)
		ob->obj_size = ALLOC_MIN_SIZE;

	/* For data objects */
	poolsize = ob->nobjs * (ob->obj_size + OOB_HEADER_SIZE);

	/* multiply by FACTOR for metadata, fragmentation, etc. */
	poolsize = poolsize * FACTOR;

	if (args->is_poolset || type == TYPE_DEVDAX) {
		if (args->fsize < poolsize) {
			fprintf(stderr, "file size too large\n");
			goto free_ob;
		}
		poolsize = 0;
	} else if (poolsize < PMEMOBJ_MIN_POOL) {
		poolsize = PMEMOBJ_MIN_POOL;
	}

	poolsize = PAGE_ALIGNED_UP_SIZE(poolsize);

	ob->pop = pmemobj_create(args->fname, nullptr, poolsize, args->fmode);
	if (ob->pop == nullptr) {
		fprintf(stderr, "%s\n", pmemobj_errormsg());
		goto free_ob;
	}

	if (init_objects(ob)) {
		goto free_pop;
	}

	if (!ob->pa->no_warmup) {
		do_warmup(ob);
	}

	return 0;

free_pop:
	pmemobj_close(ob->pop);
free_ob:
	free(ob);
	return -1;
}

/*
 * obj_persist_exit -- benchmark cleanup function
 */
static int
obj_persist_exit(struct benchmark *bench, struct benchmark_args *args)
{
	auto *ob = (struct obj_bench *)pmembench_get_priv(bench);

	for (uint64_t i = 0; i < ob->nobjs; ++i) {
		pmemobj_free(&ob->oids[i]);
	}

	pmemobj_close(ob->pop);

	free(ob->oids);
	free(ob->ptrs);
	free(ob);
	return 0;
}

static struct benchmark_clo obj_persist_clo[1];

/* Stores information about benchmark. */
static struct benchmark_info obj_persist_info;
CONSTRUCTOR(pmemobj_persist_constructor)
void
pmemobj_persist_constructor(void)
{
	obj_persist_clo[0].opt_short = 'w';
	obj_persist_clo[0].opt_long = "no-warmup";
	obj_persist_clo[0].descr = "Don't do warmup";
	obj_persist_clo[0].def = "false";
	obj_persist_clo[0].type = CLO_TYPE_FLAG;
	obj_persist_clo[0].off = clo_field_offset(struct prog_args, no_warmup);

	obj_persist_info.name = "pmemobj_persist";
	obj_persist_info.brief = "Benchmark for pmemobj_persist() "
				 "operation";
	obj_persist_info.init = obj_persist_init;
	obj_persist_info.exit = obj_persist_exit;
	obj_persist_info.multithread = true;
	obj_persist_info.multiops = true;
	obj_persist_info.operation = obj_persist_op;
	obj_persist_info.measure_time = true;
	obj_persist_info.clos = obj_persist_clo;
	obj_persist_info.nclos = ARRAY_SIZE(obj_persist_clo);
	obj_persist_info.opts_size = sizeof(struct prog_args);
	obj_persist_info.rm_file = true;
	obj_persist_info.allow_poolset = true;
	REGISTER_BENCHMARK(obj_persist_info);
};
