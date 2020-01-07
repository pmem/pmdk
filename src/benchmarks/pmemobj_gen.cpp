// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * pmemobj_gen.cpp -- benchmark for pmemobj_direct()
 * and pmemobj_open() functions.
 */

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <file.h>
#include <sys/stat.h>
#include <unistd.h>

#include "benchmark.hpp"
#include "libpmemobj.h"

#define LAYOUT_NAME "benchmark"
#define FACTOR 4
#define DIR_MODE 0700
#define FILE_MODE 0666
#define PART_NAME "/part"
#define MAX_DIGITS 2

struct pobj_bench;
struct pobj_worker;

typedef size_t (*fn_type_num_t)(struct pobj_bench *ob, size_t worker_idx,
				size_t op_idx);

typedef size_t (*fn_size_t)(struct pobj_bench *ob, size_t idx);

typedef size_t (*fn_num_t)(size_t idx);

/*
 * Enumeration used to determine the mode of the assigning type_number
 * value to the persistent objects.
 */
enum type_mode {
	TYPE_MODE_ONE,
	TYPE_MODE_PER_THREAD,
	TYPE_MODE_RAND,
	MAX_TYPE_MODE,
};

/*
 * pobj_args - Stores command line parsed arguments.
 *
 * rand_type	: Use random type number for every new allocated object.
 *		  Default, there is one type number for all objects.
 *
 * range	: Use random allocation size.
 *
 * min_size	: Minimum allocation size.
 *
 * n_objs	: Number of objects allocated per thread
 *
 * one_pool	: Use one common pool for all thread
 *
 * one_obj	: Create and use one object per thread
 *
 * obj_size	: Size of each allocated object
 *
 * n_ops	: Number of operations
 */
struct pobj_args {
	char *type_num;
	bool range;
	unsigned min_size;
	size_t n_objs;
	bool one_pool;
	bool one_obj;
	size_t obj_size;
	size_t n_ops;
};

/*
 * pobj_bench - Stores variables used in benchmark, passed within functions.
 *
 * pop			: Pointer to the persistent pool.
 *
 * pa			: Stores pobj_args structure.
 *
 * sets			: Stores files names using to create pool per thread
 *
 * random_types		: Random type numbers for persistent objects.
 *
 * rand_sizes		: random values with allocation sizes.
 *
 * n_pools		: Number of created pools.
 *
 * n_objs		: Number of object created per thread.
 *
 * type_mode		: Type_mode enum value
 *
 * fn_type_num		: Function returning proper type number for each object.
 *
 * fn_size		: Function returning proper size of allocation.
 *
 * pool			: Functions returning number of thread if
 *			  one pool per thread created or index 0 if not.
 *
 * obj			: Function returning number of operation if flag set
 *			  to false or index 0 if set to true.
 */
struct pobj_bench {
	PMEMobjpool **pop;
	struct pobj_args *args_priv;
	const char **sets;
	size_t *random_types;
	size_t *rand_sizes;
	size_t n_pools;
	int type_mode;
	fn_type_num_t fn_type_num;
	fn_size_t fn_size;
	fn_num_t pool;
	fn_num_t obj;
};

/*
 * pobj_worker - Stores variables used by one thread.
 */
struct pobj_worker {
	PMEMoid *oids;
};

/*
 * type_mode_one -- always returns 0, as in the mode TYPE_MODE_ONE
 * all of the persistent objects have the same type_number value.
 */
static size_t
type_mode_one(struct pobj_bench *bench_priv, size_t worker_idx, size_t op_idx)
{
	return 0;
}

/*
 * type_mode_per_thread -- always returns worker index, as in the mode
 * TYPE_MODE_PER_THREAD all persistent object allocated by the same thread
 * have the same type_number value.
 */
static size_t
type_mode_per_thread(struct pobj_bench *bench_priv, size_t worker_idx,
		     size_t op_idx)
{
	return worker_idx;
}

/*
 * type_mode_rand -- returns the value from the random_types array assigned
 * for the specific operation in a specific thread.
 */
static size_t
type_mode_rand(struct pobj_bench *bench_priv, size_t worker_idx, size_t op_idx)
{
	return bench_priv->random_types[op_idx];
}

/*
 * range_size -- returns size of object allocation from rand_sizes array.
 */
static size_t
range_size(struct pobj_bench *bench_priv, size_t idx)
{
	return bench_priv->rand_sizes[idx];
}

/*
 * static_size -- returns always the same size of object allocation.
 */
static size_t
static_size(struct pobj_bench *bench_priv, size_t idx)
{
	return bench_priv->args_priv->obj_size;
}

/*
 * diff_num -- returns given index
 */
static size_t
diff_num(size_t idx)
{
	return idx;
}

/*
 * one_num -- returns always the same index.
 */
static size_t
one_num(size_t idx)
{
	return 0;
}

static fn_type_num_t type_mode_func[MAX_TYPE_MODE] = {
	type_mode_one, type_mode_per_thread, type_mode_rand};

const char *type_mode_names[MAX_TYPE_MODE] = {"one", "per-thread", "rand"};

/*
 * parse_type_mode -- parses command line "--type-number" argument
 * and returns proper type_mode enum value.
 */
static enum type_mode
parse_type_mode(const char *arg)
{
	enum type_mode i = TYPE_MODE_ONE;
	for (; i < MAX_TYPE_MODE && strcmp(arg, type_mode_names[i]) != 0;
	     i = (enum type_mode)(i + 1))
		;
	return i;
}

/*
 * rand_sizes -- allocates array and calculates random values as allocation
 * sizes for each object. Used only when range flag set.
 */
static size_t *
rand_sizes(size_t min, size_t max, size_t n_ops)
{
	assert(n_ops != 0);
	auto *rand_sizes = (size_t *)malloc(n_ops * sizeof(size_t));
	if (rand_sizes == nullptr) {
		perror("malloc");
		return nullptr;
	}
	for (size_t i = 0; i < n_ops; i++) {
		rand_sizes[i] = RRAND(max, min);
	}
	return rand_sizes;
}

/*
 * random_types -- allocates array and calculates random values to assign
 * type_number for each object.
 */
static int
random_types(struct pobj_bench *bench_priv, struct benchmark_args *args)
{
	assert(bench_priv->args_priv->n_objs != 0);
	bench_priv->random_types = (size_t *)malloc(
		bench_priv->args_priv->n_objs * sizeof(size_t));
	if (bench_priv->random_types == nullptr) {
		perror("malloc");
		return -1;
	}
	for (size_t i = 0; i < bench_priv->args_priv->n_objs; i++)
		bench_priv->random_types[i] = rand() % UINT32_MAX;
	return 0;
}

/*
 * pobj_init - common part of the benchmark initialization functions.
 * Parses command line arguments, set variables and creates persistent pools.
 */
static int
pobj_init(struct benchmark *bench, struct benchmark_args *args)
{
	unsigned i = 0;
	size_t psize;
	size_t n_objs;

	assert(bench != nullptr);
	assert(args != nullptr);

	enum file_type type = util_file_get_type(args->fname);
	if (type == OTHER_ERROR) {
		fprintf(stderr, "could not check type of file %s\n",
			args->fname);
		return -1;
	}

	auto *bench_priv =
		(struct pobj_bench *)malloc(sizeof(struct pobj_bench));
	if (bench_priv == nullptr) {
		perror("malloc");
		return -1;
	}
	assert(args->opts != nullptr);

	bench_priv->args_priv = (struct pobj_args *)args->opts;
	bench_priv->args_priv->obj_size = args->dsize;
	bench_priv->args_priv->range =
		bench_priv->args_priv->min_size > 0 ? true : false;
	bench_priv->n_pools =
		!bench_priv->args_priv->one_pool ? args->n_threads : 1;
	bench_priv->pool = bench_priv->n_pools > 1 ? diff_num : one_num;
	bench_priv->obj = !bench_priv->args_priv->one_obj ? diff_num : one_num;

	if ((args->is_poolset || type == TYPE_DEVDAX) &&
	    bench_priv->n_pools > 1) {
		fprintf(stderr,
			"cannot use poolset nor device dax for multiple pools,"
			" please use -P|--one-pool option instead");
		goto free_bench_priv;
	}
	/*
	 * Multiplication by FACTOR prevents from out of memory error
	 * as the actual size of the allocated persistent objects
	 * is always larger than requested.
	 */
	n_objs = bench_priv->args_priv->n_objs;
	if (bench_priv->n_pools == 1)
		n_objs *= args->n_threads;
	psize = PMEMOBJ_MIN_POOL +
		n_objs * args->dsize * args->n_threads * FACTOR;

	/* assign type_number determining function */
	bench_priv->type_mode =
		parse_type_mode(bench_priv->args_priv->type_num);
	switch (bench_priv->type_mode) {
		case MAX_TYPE_MODE:
			fprintf(stderr, "unknown type mode");
			goto free_bench_priv;
		case TYPE_MODE_RAND:
			if (random_types(bench_priv, args))
				goto free_bench_priv;
			break;
		default:
			bench_priv->random_types = nullptr;
	}
	bench_priv->fn_type_num = type_mode_func[bench_priv->type_mode];

	/* assign size determining function */
	bench_priv->fn_size =
		bench_priv->args_priv->range ? range_size : static_size;
	bench_priv->rand_sizes = nullptr;
	if (bench_priv->args_priv->range) {
		if (bench_priv->args_priv->min_size > args->dsize) {
			fprintf(stderr, "Invalid allocation size");
			goto free_random_types;
		}
		bench_priv->rand_sizes =
			rand_sizes(bench_priv->args_priv->min_size,
				   bench_priv->args_priv->obj_size,
				   bench_priv->args_priv->n_objs);
		if (bench_priv->rand_sizes == nullptr)
			goto free_random_types;
	}

	assert(bench_priv->n_pools > 0);
	bench_priv->pop = (PMEMobjpool **)calloc(bench_priv->n_pools,
						 sizeof(PMEMobjpool *));
	if (bench_priv->pop == nullptr) {
		perror("calloc");
		goto free_random_sizes;
	}

	bench_priv->sets = (const char **)calloc(bench_priv->n_pools,
						 sizeof(const char *));
	if (bench_priv->sets == nullptr) {
		perror("calloc");
		goto free_pop;
	}
	if (bench_priv->n_pools > 1) {
		assert(!args->is_poolset);
		if (util_file_mkdir(args->fname, DIR_MODE) != 0) {
			fprintf(stderr, "cannot create directory\n");
			goto free_sets;
		}
		size_t path_len = (strlen(PART_NAME) + strlen(args->fname)) +
			MAX_DIGITS + 1;
		for (i = 0; i < bench_priv->n_pools; i++) {
			bench_priv->sets[i] =
				(char *)malloc(path_len * sizeof(char));
			if (bench_priv->sets[i] == nullptr) {
				perror("malloc");
				goto free_sets;
			}
			int ret =
				snprintf((char *)bench_priv->sets[i], path_len,
					 "%s%s%02x", args->fname, PART_NAME, i);
			if (ret < 0 || ret >= (int)path_len) {
				perror("snprintf");
				goto free_sets;
			}
			bench_priv->pop[i] =
				pmemobj_create(bench_priv->sets[i], LAYOUT_NAME,
					       psize, FILE_MODE);
			if (bench_priv->pop[i] == nullptr) {
				perror(pmemobj_errormsg());
				goto free_sets;
			}
		}
	} else {
		if (args->is_poolset || type == TYPE_DEVDAX) {
			if (args->fsize < psize) {
				fprintf(stderr, "file size too large\n");
				goto free_pools;
			}
			psize = 0;
		}
		bench_priv->sets[0] = args->fname;
		bench_priv->pop[0] = pmemobj_create(
			bench_priv->sets[0], LAYOUT_NAME, psize, FILE_MODE);
		if (bench_priv->pop[0] == nullptr) {
			perror(pmemobj_errormsg());
			goto free_pools;
		}
	}
	pmembench_set_priv(bench, bench_priv);

	return 0;
free_sets:
	for (; i > 0; i--) {
		pmemobj_close(bench_priv->pop[i - 1]);
		free((char *)bench_priv->sets[i - 1]);
	}
free_pools:
	free(bench_priv->sets);
free_pop:
	free(bench_priv->pop);
free_random_sizes:
	free(bench_priv->rand_sizes);
free_random_types:
	free(bench_priv->random_types);
free_bench_priv:
	free(bench_priv);

	return -1;
}

/*
 * pobj_direct_init -- special part of pobj_direct benchmark initialization.
 */
static int
pobj_direct_init(struct benchmark *bench, struct benchmark_args *args)
{
	auto *pa = (struct pobj_args *)args->opts;
	pa->n_objs = pa->one_obj ? 1 : args->n_ops_per_thread;
	if (pobj_init(bench, args) != 0)
		return -1;
	return 0;
}

/*
 * pobj_exit -- common part for the benchmarks exit functions
 */
static int
pobj_exit(struct benchmark *bench, struct benchmark_args *args)
{
	size_t i;
	auto *bench_priv = (struct pobj_bench *)pmembench_get_priv(bench);
	if (bench_priv->n_pools > 1) {
		for (i = 0; i < bench_priv->n_pools; i++) {
			pmemobj_close(bench_priv->pop[i]);
			free((char *)bench_priv->sets[i]);
		}
	} else {
		pmemobj_close(bench_priv->pop[0]);
	}
	free(bench_priv->sets);
	free(bench_priv->pop);
	free(bench_priv->rand_sizes);
	free(bench_priv->random_types);
	free(bench_priv);
	return 0;
}

/*
 * pobj_init_worker -- worker initialization
 */
static int
pobj_init_worker(struct benchmark *bench, struct benchmark_args *args,
		 struct worker_info *worker)
{
	size_t i, idx = worker->index;
	auto *bench_priv = (struct pobj_bench *)pmembench_get_priv(bench);
	auto *pw = (struct pobj_worker *)calloc(1, sizeof(struct pobj_worker));
	if (pw == nullptr) {
		perror("calloc");
		return -1;
	}

	worker->priv = pw;
	pw->oids = (PMEMoid *)calloc(bench_priv->args_priv->n_objs,
				     sizeof(PMEMoid));
	if (pw->oids == nullptr) {
		free(pw);
		perror("calloc");
		return -1;
	}

	PMEMobjpool *pop = bench_priv->pop[bench_priv->pool(idx)];
	for (i = 0; i < bench_priv->args_priv->n_objs; i++) {
		size_t size = bench_priv->fn_size(bench_priv, i);
		size_t type = bench_priv->fn_type_num(bench_priv, idx, i);
		if (pmemobj_alloc(pop, &pw->oids[i], size, type, nullptr,
				  nullptr) != 0) {
			perror("pmemobj_alloc");
			goto out;
		}
	}
	return 0;
out:
	for (; i > 0; i--)
		pmemobj_free(&pw->oids[i - 1]);
	free(pw->oids);
	free(pw);
	return -1;
}

/*
 * pobj_direct_op -- main operations of the obj_direct benchmark.
 */
static int
pobj_direct_op(struct benchmark *bench, struct operation_info *info)
{
	auto *bench_priv = (struct pobj_bench *)pmembench_get_priv(bench);
	auto *pw = (struct pobj_worker *)info->worker->priv;
	size_t idx = bench_priv->obj(info->index);
	/* Query an invalid uuid:off pair to invalidate the cache. */
	PMEMoid bad = {1, 1};
#define OBJ_DIRECT_NITER 1024
	/*
	 * As we measure a very fast operation, we need a loop inside the
	 * test harness.
	 */
	for (int i = 0; i < OBJ_DIRECT_NITER; i++) {
		if (pmemobj_direct(pw->oids[idx]) == nullptr)
			return -1;
		if (pmemobj_direct(bad) != nullptr)
			return -1;
	}
	return 0;
#undef OBJ_DIRECT_NITER
}

/*
 * pobj_open_op -- main operations of the obj_open benchmark.
 */
static int
pobj_open_op(struct benchmark *bench, struct operation_info *info)
{
	auto *bench_priv = (struct pobj_bench *)pmembench_get_priv(bench);
	size_t idx = bench_priv->pool(info->worker->index);
	pmemobj_close(bench_priv->pop[idx]);
	bench_priv->pop[idx] = pmemobj_open(bench_priv->sets[idx], LAYOUT_NAME);
	if (bench_priv->pop[idx] == nullptr)
		return -1;
	return 0;
}

/*
 * pobj_free_worker -- worker exit function
 */
static void
pobj_free_worker(struct benchmark *bench, struct benchmark_args *args,
		 struct worker_info *worker)
{
	auto *pw = (struct pobj_worker *)worker->priv;
	auto *bench_priv = (struct pobj_bench *)pmembench_get_priv(bench);
	for (size_t i = 0; i < bench_priv->args_priv->n_objs; i++)
		pmemobj_free(&pw->oids[i]);
	free(pw->oids);
	free(pw);
}

static struct benchmark_info obj_open;
static struct benchmark_info obj_direct;

/* Array defining common command line arguments. */
static struct benchmark_clo pobj_direct_clo[4];

static struct benchmark_clo pobj_open_clo[3];

CONSTRUCTOR(pmemobj_gen_constructor)
void
pmemobj_gen_constructor(void)
{
	pobj_direct_clo[0].opt_short = 'T';
	pobj_direct_clo[0].opt_long = "type-number";
	pobj_direct_clo[0].descr = "Type number mode - one, per-thread, "
				   "rand";
	pobj_direct_clo[0].def = "one";
	pobj_direct_clo[0].off = clo_field_offset(struct pobj_args, type_num);
	pobj_direct_clo[0].type = CLO_TYPE_STR;
	pobj_direct_clo[1].opt_short = 'm';
	pobj_direct_clo[1].opt_long = "min-size";
	pobj_direct_clo[1].type = CLO_TYPE_UINT;
	pobj_direct_clo[1].descr = "Minimum allocation size";
	pobj_direct_clo[1].off = clo_field_offset(struct pobj_args, min_size);
	pobj_direct_clo[1].def = "0";
	pobj_direct_clo[1].type_uint.size =
		clo_field_size(struct pobj_args, min_size);
	pobj_direct_clo[1].type_uint.base = CLO_INT_BASE_DEC | CLO_INT_BASE_HEX;
	pobj_direct_clo[1].type_uint.min = 0;
	pobj_direct_clo[1].type_uint.max = UINT_MAX;

	pobj_direct_clo[2].opt_short = 'P';
	pobj_direct_clo[2].opt_long = "one-pool";
	pobj_direct_clo[2].descr = "Create one pool for all threads";
	pobj_direct_clo[2].type = CLO_TYPE_FLAG;
	pobj_direct_clo[2].off = clo_field_offset(struct pobj_args, one_pool);

	pobj_direct_clo[3].opt_short = 'O';
	pobj_direct_clo[3].opt_long = "one-object";
	pobj_direct_clo[3].descr = "Use only one object per thread";
	pobj_direct_clo[3].type = CLO_TYPE_FLAG;
	pobj_direct_clo[3].off = clo_field_offset(struct pobj_args, one_obj);

	pobj_open_clo[0].opt_short = 'T',
	pobj_open_clo[0].opt_long = "type-number",
	pobj_open_clo[0].descr = "Type number mode - one, "
				 "per-thread, rand",
	pobj_open_clo[0].def = "one",
	pobj_open_clo[0].off = clo_field_offset(struct pobj_args, type_num),
	pobj_open_clo[0].type = CLO_TYPE_STR,

	pobj_open_clo[1].opt_short = 'm',
	pobj_open_clo[1].opt_long = "min-size",
	pobj_open_clo[1].type = CLO_TYPE_UINT,
	pobj_open_clo[1].descr = "Minimum allocation size",
	pobj_open_clo[1].off = clo_field_offset(struct pobj_args, min_size),
	pobj_open_clo[1].def = "0",
	pobj_open_clo[1].type_uint.size =
		clo_field_size(struct pobj_args, min_size),
	pobj_open_clo[1].type_uint.base = CLO_INT_BASE_DEC | CLO_INT_BASE_HEX,
	pobj_open_clo[1].type_uint.min = 0,
	pobj_open_clo[1].type_uint.max = UINT_MAX,

	pobj_open_clo[2].opt_short = 'o';
	pobj_open_clo[2].opt_long = "objects";
	pobj_open_clo[2].type = CLO_TYPE_UINT;
	pobj_open_clo[2].descr = "Number of objects in each pool";
	pobj_open_clo[2].off = clo_field_offset(struct pobj_args, n_objs);
	pobj_open_clo[2].def = "1";
	pobj_open_clo[2].type_uint.size =
		clo_field_size(struct pobj_args, n_objs);
	pobj_open_clo[2].type_uint.base = CLO_INT_BASE_DEC | CLO_INT_BASE_HEX;
	pobj_open_clo[2].type_uint.min = 1;
	pobj_open_clo[2].type_uint.max = UINT_MAX;

	obj_open.name = "obj_open";
	obj_open.brief = "pmemobj_open() benchmark";
	obj_open.init = pobj_init;
	obj_open.exit = pobj_exit;
	obj_open.multithread = true;
	obj_open.multiops = true;
	obj_open.init_worker = pobj_init_worker;
	obj_open.free_worker = pobj_free_worker;
	obj_open.operation = pobj_open_op;
	obj_open.measure_time = true;
	obj_open.clos = pobj_open_clo;
	obj_open.nclos = ARRAY_SIZE(pobj_open_clo);
	obj_open.opts_size = sizeof(struct pobj_args);
	obj_open.rm_file = true;
	obj_open.allow_poolset = true;
	REGISTER_BENCHMARK(obj_open);

	obj_direct.name = "obj_direct";
	obj_direct.brief = "pmemobj_direct() benchmark";
	obj_direct.init = pobj_direct_init;
	obj_direct.exit = pobj_exit;
	obj_direct.multithread = true;
	obj_direct.multiops = true;
	obj_direct.init_worker = pobj_init_worker;
	obj_direct.free_worker = pobj_free_worker;
	obj_direct.operation = pobj_direct_op;
	obj_direct.measure_time = true;
	obj_direct.clos = pobj_direct_clo;
	obj_direct.nclos = ARRAY_SIZE(pobj_direct_clo);
	obj_direct.opts_size = sizeof(struct pobj_args);
	obj_direct.rm_file = true;
	obj_direct.allow_poolset = true;
	REGISTER_BENCHMARK(obj_direct);
};
