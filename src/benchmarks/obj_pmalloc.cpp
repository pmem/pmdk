// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */

/*
 * obj_pmalloc.cpp -- pmalloc benchmarks definition
 */

#include <cassert>
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include "benchmark.hpp"
#include "file.h"
#include "libpmemobj.h"
#include "memops.h"
#include "os.h"
#include "pmalloc.h"
#include "poolset_util.hpp"
#include "valgrind_internal.h"

/*
 * The factor used for PMEM pool size calculation, accounts for metadata,
 * fragmentation and etc.
 */
#define FACTOR 1.2f

/* The minimum allocation size that pmalloc can perform */
#define ALLOC_MIN_SIZE 64

/* OOB and allocation header size */
#define OOB_HEADER_SIZE 64

/*
 * prog_args - command line parsed arguments
 */
struct prog_args {
	size_t minsize;	      /* minimum size for random allocation size */
	bool use_random_size; /* if set, use random size allocations */
	unsigned seed;	      /* PRNG seed */
};

POBJ_LAYOUT_BEGIN(pmalloc_layout);
POBJ_LAYOUT_ROOT(pmalloc_layout, struct my_root);
POBJ_LAYOUT_TOID(pmalloc_layout, uint64_t);
POBJ_LAYOUT_END(pmalloc_layout);

/*
 * my_root - root object
 */
struct my_root {
	TOID(uint64_t) offs; /* vector of the allocated object offsets */
};

/*
 * obj_bench - variables used in benchmark, passed within functions
 */
struct obj_bench {
	PMEMobjpool *pop;	   /* persistent pool handle */
	struct prog_args *pa;	   /* prog_args structure */
	size_t *sizes;		   /* sizes for allocations */
	TOID(struct my_root) root; /* root object's OID */
	uint64_t *offs;		   /* pointer to the vector of offsets */
};

/*
 * obj_init -- common part of the benchmark initialization for pmalloc and
 * pfree. It allocates the PMEM memory pool and the necessary offset vector.
 */
static int
obj_init(struct benchmark *bench, struct benchmark_args *args)
{
	struct my_root *root = nullptr;
	assert(bench != nullptr);
	assert(args != nullptr);
	assert(args->opts != nullptr);

	char path[PATH_MAX];
	if (util_safe_strcpy(path, args->fname, sizeof(path)) != 0)
		return -1;

	enum file_type type = util_file_get_type(args->fname);
	if (type == OTHER_ERROR) {
		fprintf(stderr, "could not check type of file %s\n",
			args->fname);
		return -1;
	}

	if (((struct prog_args *)(args->opts))->minsize >= args->dsize) {
		fprintf(stderr, "Wrong params - allocation size\n");
		return -1;
	}

	auto *ob = (struct obj_bench *)malloc(sizeof(struct obj_bench));
	if (ob == nullptr) {
		perror("malloc");
		return -1;
	}
	pmembench_set_priv(bench, ob);

	ob->pa = (struct prog_args *)args->opts;

	size_t n_ops_total = args->n_ops_per_thread * args->n_threads;
	assert(n_ops_total != 0);

	/* Create pmemobj pool. */
	size_t alloc_size = args->dsize;
	if (alloc_size < ALLOC_MIN_SIZE)
		alloc_size = ALLOC_MIN_SIZE;

	/* For data objects */
	size_t poolsize = PMEMOBJ_MIN_POOL +
		(n_ops_total * (alloc_size + OOB_HEADER_SIZE))
		/* for offsets */
		+ n_ops_total * sizeof(uint64_t);

	/* multiply by FACTOR for metadata, fragmentation, etc. */
	poolsize = (size_t)(poolsize * FACTOR);

	if (args->is_poolset || type == TYPE_DEVDAX) {
		if (args->fsize < poolsize) {
			fprintf(stderr, "file size too large\n");
			goto free_ob;
		}
		poolsize = 0;
	} else if (poolsize < PMEMOBJ_MIN_POOL) {
		poolsize = PMEMOBJ_MIN_POOL;
	}

	if (args->is_dynamic_poolset) {
		int ret = dynamic_poolset_create(args->fname, poolsize);
		if (ret == -1)
			goto free_ob;

		if (util_safe_strcpy(path, POOLSET_PATH, sizeof(path)) != 0)
			goto free_ob;

		poolsize = 0;
	}

	ob->pop = pmemobj_create(path, POBJ_LAYOUT_NAME(pmalloc_layout),
				 poolsize, args->fmode);
	if (ob->pop == nullptr) {
		fprintf(stderr, "%s\n", pmemobj_errormsg());
		goto free_ob;
	}

	ob->root = POBJ_ROOT(ob->pop, struct my_root);
	if (TOID_IS_NULL(ob->root)) {
		fprintf(stderr, "POBJ_ROOT: %s\n", pmemobj_errormsg());
		goto free_pop;
	}

	root = D_RW(ob->root);
	assert(root != nullptr);
	POBJ_ZALLOC(ob->pop, &root->offs, uint64_t,
		    n_ops_total * sizeof(PMEMoid));
	if (TOID_IS_NULL(root->offs)) {
		fprintf(stderr, "POBJ_ZALLOC off_vect: %s\n",
			pmemobj_errormsg());
		goto free_pop;
	}

	ob->offs = D_RW(root->offs);

	ob->sizes = (size_t *)malloc(n_ops_total * sizeof(size_t));
	if (ob->sizes == nullptr) {
		fprintf(stderr, "malloc rand size vect err\n");
		goto free_pop;
	}

	if (ob->pa->use_random_size) {
		size_t width = args->dsize - ob->pa->minsize;
		for (size_t i = 0; i < n_ops_total; i++) {
			auto hr = (uint32_t)os_rand_r(&ob->pa->seed);
			auto lr = (uint32_t)os_rand_r(&ob->pa->seed);
			uint64_t r64 = (uint64_t)hr << 32 | lr;
			ob->sizes[i] = r64 % width + ob->pa->minsize;
		}
	} else {
		for (size_t i = 0; i < n_ops_total; i++)
			ob->sizes[i] = args->dsize;
	}

	return 0;

free_pop:
	pmemobj_close(ob->pop);

free_ob:
	free(ob);
	return -1;
}

/*
 * obj_exit -- common part for the exit function for pmalloc and pfree
 * benchmarks. It frees the allocated offset vector and the memory pool.
 */
static int
obj_exit(struct benchmark *bench, struct benchmark_args *args)
{
	auto *ob = (struct obj_bench *)pmembench_get_priv(bench);

	free(ob->sizes);

	POBJ_FREE(&D_RW(ob->root)->offs);
	pmemobj_close(ob->pop);

	return 0;
}

/*
 * pmalloc_init -- initialization for the pmalloc benchmark. Performs only the
 * common initialization.
 */
static int
pmalloc_init(struct benchmark *bench, struct benchmark_args *args)
{
	return obj_init(bench, args);
}

/*
 * pmalloc_op -- actual benchmark operation. Performs the pmalloc allocations.
 */
static int
pmalloc_op(struct benchmark *bench, struct operation_info *info)
{
	auto *ob = (struct obj_bench *)pmembench_get_priv(bench);

	uint64_t i = info->index +
		info->worker->index * info->args->n_ops_per_thread;

	int ret = pmalloc(ob->pop, &ob->offs[i], ob->sizes[i], 0, 0);
	if (ret) {
		fprintf(stderr, "pmalloc ret: %d\n", ret);
		return ret;
	}

	return 0;
}

struct pmix_worker {
	size_t nobjects;
	size_t shuffle_start;
	rng_t rng;
};

/*
 * pmix_worker_init -- initialization of the worker structure
 */
static int
pmix_worker_init(struct benchmark *bench, struct benchmark_args *args,
		 struct worker_info *worker)
{
	struct pmix_worker *w = (struct pmix_worker *)calloc(1, sizeof(*w));
	auto *ob = (struct obj_bench *)pmembench_get_priv(bench);
	if (w == nullptr)
		return -1;

	randomize_r(&w->rng, ob->pa->seed);

	worker->priv = w;

	return 0;
}

/*
 * pmix_worker_fini -- destruction of the worker structure
 */
static void
pmix_worker_fini(struct benchmark *bench, struct benchmark_args *args,
		 struct worker_info *worker)
{
	auto *w = (struct pmix_worker *)worker->priv;
	free(w);
}

/*
 * shuffle_objects -- randomly shuffle elements on a list
 *
 * Ideally, we wouldn't count the time this function takes, but for all
 * practical purposes this is fast enough and isn't visible on the results.
 * Just make sure the amount of objects to shuffle is not large.
 */
static void
shuffle_objects(uint64_t *objects, size_t start, size_t nobjects, rng_t *rng)
{
	uint64_t tmp;
	size_t dest;
	for (size_t n = start; n < nobjects; ++n) {
		dest = RRAND_R(rng, nobjects - 1, 0);
		tmp = objects[n];
		objects[n] = objects[dest];
		objects[dest] = tmp;
	}
}

#define FREE_PCT 10
#define FREE_OPS 10

/*
 * pmix_op -- mixed workload benchmark
 */
static int
pmix_op(struct benchmark *bench, struct operation_info *info)
{
	auto *ob = (struct obj_bench *)pmembench_get_priv(bench);
	auto *w = (struct pmix_worker *)info->worker->priv;

	uint64_t idx = info->worker->index * info->args->n_ops_per_thread;

	uint64_t *objects = &ob->offs[idx];

	if (w->nobjects > FREE_OPS && FREE_PCT > RRAND_R(&w->rng, 100, 0)) {
		shuffle_objects(objects, w->shuffle_start, w->nobjects,
				&w->rng);

		for (int i = 0; i < FREE_OPS; ++i) {
			uint64_t off = objects[--w->nobjects];
			pfree(ob->pop, &off);
		}
		w->shuffle_start = w->nobjects;
	} else {
		int ret = pmalloc(ob->pop, &objects[w->nobjects++],
				  ob->sizes[idx + info->index], 0, 0);
		if (ret) {
			fprintf(stderr, "pmalloc ret: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

/*
 * pmalloc_exit -- the end of the pmalloc benchmark. Frees the memory allocated
 * during pmalloc_op and performs the common exit operations.
 */
static int
pmalloc_exit(struct benchmark *bench, struct benchmark_args *args)
{
	auto *ob = (struct obj_bench *)pmembench_get_priv(bench);

	for (size_t i = 0; i < args->n_ops_per_thread * args->n_threads; i++) {
		if (ob->offs[i])
			pfree(ob->pop, &ob->offs[i]);
	}

	return obj_exit(bench, args);
}

/*
 * pfree_init -- initialization for the pfree benchmark. Performs the common
 * initialization and allocates the memory to be freed during pfree_op.
 */
static int
pfree_init(struct benchmark *bench, struct benchmark_args *args)
{
	int ret = obj_init(bench, args);
	if (ret)
		return ret;

	auto *ob = (struct obj_bench *)pmembench_get_priv(bench);

	for (size_t i = 0; i < args->n_ops_per_thread * args->n_threads; i++) {
		ret = pmalloc(ob->pop, &ob->offs[i], ob->sizes[i], 0, 0);
		if (ret) {
			fprintf(stderr, "pmalloc at idx %" PRIu64 " ret: %s\n",
				i, pmemobj_errormsg());
			/* free the allocated memory */
			while (i != 0) {
				pfree(ob->pop, &ob->offs[i - 1]);
				i--;
			}
			obj_exit(bench, args);
			return ret;
		}
	}

	return 0;
}

/*
 * pmalloc_op -- actual benchmark operation. Performs the pfree operation.
 */
static int
pfree_op(struct benchmark *bench, struct operation_info *info)
{
	auto *ob = (struct obj_bench *)pmembench_get_priv(bench);

	uint64_t i = info->index +
		info->worker->index * info->args->n_ops_per_thread;

	pfree(ob->pop, &ob->offs[i]);

	return 0;
}

/* command line options definition */
static struct benchmark_clo pmalloc_clo[3];
/*
 * Stores information about pmalloc benchmark.
 */
static struct benchmark_info pmalloc_info;
/*
 * Stores information about pfree benchmark.
 */
static struct benchmark_info pfree_info;
/*
 * Stores information about pmix benchmark.
 */
static struct benchmark_info pmix_info;

CONSTRUCTOR(obj_pmalloc_constructor)
void
obj_pmalloc_constructor(void)
{
	pmalloc_clo[0].opt_short = 'r';
	pmalloc_clo[0].opt_long = "random";
	pmalloc_clo[0].descr = "Use random size allocations - "
			       "from min-size to data-size";
	pmalloc_clo[0].off =
		clo_field_offset(struct prog_args, use_random_size);
	pmalloc_clo[0].type = CLO_TYPE_FLAG;

	pmalloc_clo[1].opt_short = 'm';
	pmalloc_clo[1].opt_long = "min-size";
	pmalloc_clo[1].descr = "Minimum size of allocation for "
			       "random mode";
	pmalloc_clo[1].type = CLO_TYPE_UINT;
	pmalloc_clo[1].off = clo_field_offset(struct prog_args, minsize);
	pmalloc_clo[1].def = "1";
	pmalloc_clo[1].type_uint.size =
		clo_field_size(struct prog_args, minsize);
	pmalloc_clo[1].type_uint.base = CLO_INT_BASE_DEC;
	pmalloc_clo[1].type_uint.min = 1;
	pmalloc_clo[1].type_uint.max = UINT64_MAX;

	pmalloc_clo[2].opt_short = 'S';
	pmalloc_clo[2].opt_long = "seed";
	pmalloc_clo[2].descr = "Random mode seed value";
	pmalloc_clo[2].off = clo_field_offset(struct prog_args, seed);
	pmalloc_clo[2].def = "1";
	pmalloc_clo[2].type = CLO_TYPE_UINT;
	pmalloc_clo[2].type_uint.size = clo_field_size(struct prog_args, seed);
	pmalloc_clo[2].type_uint.base = CLO_INT_BASE_DEC;
	pmalloc_clo[2].type_uint.min = 1;
	pmalloc_clo[2].type_uint.max = UINT_MAX;

	pmalloc_info.name = "pmalloc",
	pmalloc_info.brief = "Benchmark for internal pmalloc() "
			     "operation";
	pmalloc_info.init = pmalloc_init;
	pmalloc_info.exit = pmalloc_exit;
	pmalloc_info.multithread = true;
	pmalloc_info.multiops = true;
	pmalloc_info.operation = pmalloc_op;
	pmalloc_info.measure_time = true;
	pmalloc_info.clos = pmalloc_clo;
	pmalloc_info.nclos = ARRAY_SIZE(pmalloc_clo);
	pmalloc_info.opts_size = sizeof(struct prog_args);
	pmalloc_info.rm_file = true;
	pmalloc_info.allow_poolset = true;
	REGISTER_BENCHMARK(pmalloc_info);

	pfree_info.name = "pfree";
	pfree_info.brief = "Benchmark for internal pfree() "
			   "operation";
	pfree_info.init = pfree_init;

	pfree_info.exit = pmalloc_exit; /* same as for pmalloc */
	pfree_info.multithread = true;
	pfree_info.multiops = true;
	pfree_info.operation = pfree_op;
	pfree_info.measure_time = true;
	pfree_info.clos = pmalloc_clo;
	pfree_info.nclos = ARRAY_SIZE(pmalloc_clo);
	pfree_info.opts_size = sizeof(struct prog_args);
	pfree_info.rm_file = true;
	pfree_info.allow_poolset = true;
	REGISTER_BENCHMARK(pfree_info);

	pmix_info.name = "pmix";
	pmix_info.brief = "Benchmark for mixed alloc/free workload";
	pmix_info.init = pmalloc_init;

	pmix_info.exit = pmalloc_exit; /* same as for pmalloc */
	pmix_info.multithread = true;
	pmix_info.multiops = true;
	pmix_info.operation = pmix_op;
	pmix_info.init_worker = pmix_worker_init;
	pmix_info.free_worker = pmix_worker_fini;
	pmix_info.measure_time = true;
	pmix_info.clos = pmalloc_clo;
	pmix_info.nclos = ARRAY_SIZE(pmalloc_clo);
	pmix_info.opts_size = sizeof(struct prog_args);
	pmix_info.rm_file = true;
	pmix_info.allow_poolset = true;
	REGISTER_BENCHMARK(pmix_info);
};
