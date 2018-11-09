/*
 * Copyright 2015-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *	* Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *
 *	* Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in
 *	  the documentation and/or other materials provided with the
 *	  distribution.
 *
 *	* Neither the name of the copyright holder nor the names of its
 *	  contributors may be used to endorse or promote products derived
 *	  from this software without specific prior written permission.
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
 *
 */

/*
 * vmem.cpp -- vmem_malloc, vmem_free and vmem_realloc multithread benchmarks
 */

#include "benchmark.hpp"
#include "file.h"
#include <cassert>
#include <libvmem.h>
#include <sys/stat.h>

#define DIR_MODE 0700
#define MAX_POOLS 8
#define FACTOR 2
#define RRAND(max, min) (rand() % ((max) - (min)) + (min))

struct vmem_bench;
typedef int (*operation)(struct vmem_bench *vb, unsigned worker_idx,
			 size_t info_idx);

/*
 * vmem_args -- additional properties set as arguments opts
 */
struct vmem_args {
	bool stdlib_alloc;    /* use stdlib allocator instead of vmem */
	bool no_warmup;       /* do not perform warmup */
	bool pool_per_thread; /* create single pool per thread */
	ssize_t min_size;     /* size of min allocation in range mode */
	ssize_t rsize;	/* size of reallocation */
	ssize_t min_rsize;    /* size of min reallocation in range mode */

	/* perform operation on object allocated by other thread */
	bool mix;
};

/*
 * item -- structure representing single allocated object
 */
struct item {
	void *buf; /* buffer for operations */

	/* number of pool to which object is assigned */
	unsigned pool_num;
};

/*
 * vmem_worker -- additional properties set as worker private
 */
struct vmem_worker {
	/* array to store objects used in operations performed by worker */
	struct item *objs;
	unsigned pool_number; /* number of pool used by worker */
};

/*
 * vmem_bench -- additional properties set as benchmark private
 */
struct vmem_bench {
	VMEM **pools;		     /* handle for VMEM pools */
	struct vmem_worker *workers; /* array with private workers data */
	size_t pool_size;	    /* size of each pool */
	unsigned npools;	     /* number of created pools */
	size_t *alloc_sizes;	 /* array with allocation sizes */
	size_t *realloc_sizes;       /* array with reallocation sizes */
	unsigned *mix_ops;	   /* array with random indexes */
	bool rand_alloc;	     /* use range mode in allocation */
	bool rand_realloc;	   /* use range mode in reallocation */
	int lib_mode;		     /* library mode - vmem or stdlib */
};

/*
 * lib_mode -- enumeration used to determine mode of the benchmark
 */
enum lib_mode { VMEM_MODE, STDLIB_MODE };

/*
 * vmem_malloc_op -- malloc operation using vmem
 */
static int
vmem_malloc_op(struct vmem_bench *vb, unsigned worker_idx, size_t info_idx)
{
	struct item *item = &vb->workers[worker_idx].objs[info_idx];
	item->buf = vmem_malloc(vb->pools[item->pool_num],
				vb->alloc_sizes[info_idx]);
	if (item->buf == nullptr) {
		perror("vmem_malloc");
		return -1;
	}
	return 0;
}

/*
 * stdlib_malloc_op -- malloc operation using stdlib
 */
static int
stdlib_malloc_op(struct vmem_bench *vb, unsigned worker_idx, size_t info_idx)
{
	struct item *item = &vb->workers[worker_idx].objs[info_idx];
	item->buf = malloc(vb->alloc_sizes[info_idx]);
	if (item->buf == nullptr) {
		perror("malloc");
		return -1;
	}
	return 0;
}

/*
 * vmem_free_op -- free operation using vmem
 */
static int
vmem_free_op(struct vmem_bench *vb, unsigned worker_idx, size_t info_idx)
{
	struct item *item = &vb->workers[worker_idx].objs[info_idx];
	if (item->buf != nullptr)
		vmem_free(vb->pools[item->pool_num], item->buf);
	item->buf = nullptr;
	return 0;
}

/*
 * stdlib_free_op -- free operation using stdlib
 */
static int
stdlib_free_op(struct vmem_bench *vb, unsigned worker_idx, size_t info_idx)
{
	struct item *item = &vb->workers[worker_idx].objs[info_idx];
	if (item->buf != nullptr)
		free(item->buf);
	item->buf = nullptr;
	return 0;
}

/*
 * vmem_realloc_op -- realloc operation using vmem
 */
static int
vmem_realloc_op(struct vmem_bench *vb, unsigned worker_idx, size_t info_idx)
{
	struct item *item = &vb->workers[worker_idx].objs[info_idx];
	item->buf = vmem_realloc(vb->pools[item->pool_num], item->buf,
				 vb->realloc_sizes[info_idx]);
	if (vb->realloc_sizes[info_idx] != 0 && item->buf == nullptr) {
		perror("vmem_realloc");
		return -1;
	}
	return 0;
}

/*
 * stdlib_realloc_op -- realloc operation using stdlib
 */
static int
stdlib_realloc_op(struct vmem_bench *vb, unsigned worker_idx, size_t info_idx)
{
	struct item *item = &vb->workers[worker_idx].objs[info_idx];
	item->buf = realloc(item->buf, vb->realloc_sizes[info_idx]);
	if (vb->realloc_sizes[info_idx] != 0 && item->buf == nullptr) {
		perror("realloc");
		return -1;
	}
	return 0;
}

static operation malloc_op[2] = {vmem_malloc_op, stdlib_malloc_op};
static operation free_op[2] = {vmem_free_op, stdlib_free_op};
static operation realloc_op[2] = {vmem_realloc_op, stdlib_realloc_op};

/*
 * vmem_create_pools -- use vmem_create to create pools
 */
static int
vmem_create_pools(struct vmem_bench *vb, struct benchmark_args *args)
{
	unsigned i;
	auto *va = (struct vmem_args *)args->opts;
	size_t dsize = args->dsize + va->rsize;
	vb->pool_size =
		dsize * args->n_ops_per_thread * args->n_threads / vb->npools;
	vb->pools = (VMEM **)calloc(vb->npools, sizeof(VMEM *));
	if (vb->pools == nullptr) {
		perror("calloc");
		return -1;
	}
	if (vb->pool_size < VMEM_MIN_POOL * args->n_threads)
		vb->pool_size = VMEM_MIN_POOL * args->n_threads;

	/* multiply pool size to prevent out of memory error  */
	vb->pool_size *= FACTOR;
	for (i = 0; i < vb->npools; i++) {
		vb->pools[i] = vmem_create(args->fname, vb->pool_size);
		if (vb->pools[i] == nullptr) {
			perror("vmem_create");
			goto err;
		}
	}
	return 0;
err:
	for (int j = i - 1; j >= 0; j--)
		vmem_delete(vb->pools[j]);
	free(vb->pools);
	return -1;
}

/*
 * random_values -- calculates values for random sizes
 */
static void
random_values(size_t *alloc_sizes, struct benchmark_args *args, size_t max,
	      size_t min)
{
	if (args->seed != 0)
		srand(args->seed);

	for (size_t i = 0; i < args->n_ops_per_thread; i++)
		alloc_sizes[i] = RRAND(max, min);
}

/*
 * static_values -- fulls array with the same value
 */
static void
static_values(size_t *alloc_sizes, size_t dsize, size_t nops)
{
	for (size_t i = 0; i < nops; i++)
		alloc_sizes[i] = dsize;
}

/*
 * vmem_do_warmup -- perform warm-up by malloc and free for every thread
 */
static int
vmem_do_warmup(struct vmem_bench *vb, struct benchmark_args *args)
{
	unsigned i;
	size_t j;
	int ret = 0;
	for (i = 0; i < args->n_threads; i++) {
		for (j = 0; j < args->n_ops_per_thread; j++) {
			if (malloc_op[vb->lib_mode](vb, i, j) != 0) {
				ret = -1;
				fprintf(stderr, "warmup failed");
				break;
			}
		}

		for (; j > 0; j--)
			free_op[vb->lib_mode](vb, i, j - 1);
	}
	return ret;
}

/*
 * malloc_main_op -- main operations for vmem_malloc benchmark
 */
static int
malloc_main_op(struct benchmark *bench, struct operation_info *info)
{
	auto *vb = (struct vmem_bench *)pmembench_get_priv(bench);
	return malloc_op[vb->lib_mode](vb, info->worker->index, info->index);
}

/*
 * free_main_op -- main operations for vmem_free benchmark
 */
static int
free_main_op(struct benchmark *bench, struct operation_info *info)
{
	auto *vb = (struct vmem_bench *)pmembench_get_priv(bench);
	return free_op[vb->lib_mode](vb, info->worker->index, info->index);
}

/*
 * realloc_main_op -- main operations for vmem_realloc benchmark
 */
static int
realloc_main_op(struct benchmark *bench, struct operation_info *info)
{
	auto *vb = (struct vmem_bench *)pmembench_get_priv(bench);
	return realloc_op[vb->lib_mode](vb, info->worker->index, info->index);
}

/*
 * vmem_mix_op -- main operations for vmem_mix benchmark
 */
static int
vmem_mix_op(struct benchmark *bench, struct operation_info *info)
{
	auto *vb = (struct vmem_bench *)pmembench_get_priv(bench);
	unsigned idx = vb->mix_ops[info->index];
	free_op[vb->lib_mode](vb, info->worker->index, idx);
	return malloc_op[vb->lib_mode](vb, info->worker->index, idx);
}

/*
 * vmem_init_worker_alloc -- initialize worker for vmem_free and
 * vmem_realloc benchmark when mix flag set to false
 */
static int
vmem_init_worker_alloc(struct vmem_bench *vb, struct benchmark_args *args,
		       struct worker_info *worker)
{
	size_t i;
	for (i = 0; i < args->n_ops_per_thread; i++) {
		if (malloc_op[vb->lib_mode](vb, worker->index, i) != 0)
			goto out;
	}
	return 0;
out:
	for (int j = i - 1; j >= 0; j--)
		free_op[vb->lib_mode](vb, worker->index, i);
	return -1;
}

/*
 * vmem_init_worker_alloc_mix -- initialize worker for vmem_free and
 * vmem_realloc benchmark when mix flag set to true
 */
static int
vmem_init_worker_alloc_mix(struct vmem_bench *vb, struct benchmark_args *args,
			   struct worker_info *worker)
{
	unsigned i = 0;
	uint64_t j = 0;
	size_t idx = 0;
	size_t ops_per_thread = args->n_ops_per_thread / args->n_threads;
	for (i = 0; i < args->n_threads; i++) {
		for (j = 0; j < ops_per_thread; j++) {
			idx = ops_per_thread * worker->index + j;
			vb->workers[i].objs[idx].pool_num =
				vb->workers[i].pool_number;
			if (malloc_op[vb->lib_mode](vb, i, idx) != 0)
				goto out;
		}
	}
	for (idx = ops_per_thread * args->n_threads;
	     idx < args->n_ops_per_thread; idx++) {
		if (malloc_op[vb->lib_mode](vb, worker->index, idx) != 0)
			goto out_ops;
	}
	return 0;
out_ops:
	for (idx--; idx >= ops_per_thread; idx--)
		free_op[vb->lib_mode](vb, worker->index, idx);
out:

	for (; i > 0; i--) {
		for (; j > 0; j--) {
			idx = ops_per_thread * worker->index + j - 1;
			free_op[vb->lib_mode](vb, i - 1, idx);
		}
	}
	return -1;
}

/*
 * vmem_init_worker_alloc_mix -- initialize worker for vmem_free and
 * vmem_realloc benchmark
 */
static int
vmem_init_worker(struct benchmark *bench, struct benchmark_args *args,
		 struct worker_info *worker)
{
	auto *va = (struct vmem_args *)args->opts;
	auto *vb = (struct vmem_bench *)pmembench_get_priv(bench);
	int ret = va->mix ? vmem_init_worker_alloc_mix(vb, args, worker)
			  : vmem_init_worker_alloc(vb, args, worker);
	return ret;
}

/*
 * vmem_exit -- function for de-initialization benchmark
 */
static int
vmem_exit(struct benchmark *bench, struct benchmark_args *args)
{
	unsigned i;
	auto *vb = (struct vmem_bench *)pmembench_get_priv(bench);
	auto *va = (struct vmem_args *)args->opts;
	if (!va->stdlib_alloc) {
		for (i = 0; i < vb->npools; i++) {
			vmem_delete(vb->pools[i]);
		}
		free(vb->pools);
	}
	for (i = 0; i < args->n_threads; i++)
		free(vb->workers[i].objs);
	free(vb->workers);
	free(vb->alloc_sizes);
	if (vb->realloc_sizes != nullptr)
		free(vb->realloc_sizes);
	if (vb->mix_ops != nullptr)
		free(vb->mix_ops);
	free(vb);
	return 0;
}

/*
 * vmem_exit_free -- frees worker with freeing elements
 */
static int
vmem_exit_free(struct benchmark *bench, struct benchmark_args *args)
{
	auto *vb = (struct vmem_bench *)pmembench_get_priv(bench);
	for (unsigned i = 0; i < args->n_threads; i++) {
		for (size_t j = 0; j < args->n_ops_per_thread; j++) {
			free_op[vb->lib_mode](vb, i, j);
		}
	}
	return vmem_exit(bench, args);
}

/*
 * vmem_init -- function for initialization benchmark
 */
static int
vmem_init(struct benchmark *bench, struct benchmark_args *args)
{
	unsigned i;
	size_t j;
	assert(bench != nullptr);
	assert(args != nullptr);

	enum file_type type = util_file_get_type(args->fname);
	if (type == OTHER_ERROR) {
		fprintf(stderr, "could not check type of file %s\n",
			args->fname);
		return -1;
	}

	auto *vb = (struct vmem_bench *)calloc(1, sizeof(struct vmem_bench));
	if (vb == nullptr) {
		perror("malloc");
		return -1;
	}
	pmembench_set_priv(bench, vb);
	struct vmem_worker *vw;
	auto *va = (struct vmem_args *)args->opts;
	vb->alloc_sizes = nullptr;
	vb->lib_mode = va->stdlib_alloc ? STDLIB_MODE : VMEM_MODE;

	if (type == TYPE_DEVDAX && va->pool_per_thread) {
		fprintf(stderr, "cannot use device dax for multiple pools\n");
		goto err;
	}

	if (type == TYPE_NORMAL) {
		fprintf(stderr, "Path cannot point to existing file\n");
		goto err;
	}

	if (type == NOT_EXISTS && !va->stdlib_alloc &&
	    mkdir(args->fname, DIR_MODE) != 0)
		goto err;

	vb->npools = va->pool_per_thread ? args->n_threads : 1;

	vb->rand_alloc = va->min_size != -1;
	if (vb->rand_alloc && (size_t)va->min_size > args->dsize) {
		fprintf(stderr, "invalid allocation size\n");
		goto err;
	}

	/* vmem library is enable to create limited number of pools */
	if (va->pool_per_thread && args->n_threads > MAX_POOLS) {
		fprintf(stderr,
			"Maximum number of threads is %d for pool-per-thread option\n",
			MAX_POOLS);
		goto err;
	}

	/* initializes buffers for operations for every thread */
	vb->workers = (struct vmem_worker *)calloc(args->n_threads,
						   sizeof(struct vmem_worker));
	if (vb->workers == nullptr) {
		perror("calloc");
		goto err;
	}
	for (i = 0; i < args->n_threads; i++) {
		vw = &vb->workers[i];
		vw->objs = (struct item *)calloc(args->n_ops_per_thread,
						 sizeof(struct item));
		if (vw->objs == nullptr) {
			perror("calloc");
			goto err_free_workers;
		}

		vw->pool_number = va->pool_per_thread ? i : 0;
		for (j = 0; j < args->n_ops_per_thread; j++)
			vw->objs[j].pool_num = vw->pool_number;
	}

	if ((vb->alloc_sizes = (size_t *)malloc(
		     sizeof(size_t) * args->n_ops_per_thread)) == nullptr) {
		perror("malloc");
		goto err_free_buf;
	}
	if (vb->rand_alloc)
		random_values(vb->alloc_sizes, args, args->dsize,
			      (size_t)va->min_size);
	else
		static_values(vb->alloc_sizes, args->dsize,
			      args->n_ops_per_thread);

	if (!va->stdlib_alloc && vmem_create_pools(vb, args) != 0)
		goto err_free_sizes;

	if (!va->no_warmup && vmem_do_warmup(vb, args) != 0)
		goto err_free_all;

	return 0;

err_free_all:
	if (!va->stdlib_alloc) {
		for (i = 0; i < vb->npools; i++)
			vmem_delete(vb->pools[i]);
		free(vb->pools);
	}
err_free_sizes:
	free(vb->alloc_sizes);
err_free_buf:
	for (j = i; j > 0; j--)
		free(vb->workers[j - 1].objs);
err_free_workers:
	free(vb->workers);
err:
	free(vb);
	return -1;
}

/*
 * vmem_realloc_init -- function for initialization vmem_realloc benchmark
 */
static int
vmem_realloc_init(struct benchmark *bench, struct benchmark_args *args)
{
	if (vmem_init(bench, args) != 0)
		return -1;

	auto *vb = (struct vmem_bench *)pmembench_get_priv(bench);
	auto *va = (struct vmem_args *)args->opts;
	vb->rand_realloc = va->min_rsize != -1;

	if (vb->rand_realloc && va->min_rsize > va->rsize) {
		fprintf(stderr, "invalid reallocation size\n");
		goto err;
	}
	if ((vb->realloc_sizes = (size_t *)calloc(args->n_ops_per_thread,
						  sizeof(size_t))) == nullptr) {
		perror("calloc");
		goto err;
	}
	if (vb->rand_realloc)
		random_values(vb->realloc_sizes, args, (size_t)va->rsize,
			      (size_t)va->min_rsize);
	else
		static_values(vb->realloc_sizes, (size_t)va->rsize,
			      args->n_ops_per_thread);
	return 0;
err:
	vmem_exit(bench, args);
	return -1;
}

/*
 * vmem_mix_init -- function for initialization vmem_realloc benchmark
 */
static int
vmem_mix_init(struct benchmark *bench, struct benchmark_args *args)
{
	if (vmem_init(bench, args) != 0)
		return -1;

	size_t i;
	unsigned idx, tmp;
	auto *vb = (struct vmem_bench *)pmembench_get_priv(bench);
	if ((vb->mix_ops = (unsigned *)calloc(args->n_ops_per_thread,
					      sizeof(unsigned))) == nullptr) {
		perror("calloc");
		goto err;
	}
	for (i = 0; i < args->n_ops_per_thread; i++)
		vb->mix_ops[i] = i;

	if (args->seed != 0)
		srand(args->seed);

	for (i = 1; i < args->n_ops_per_thread; i++) {
		idx = RRAND(args->n_ops_per_thread - 1, 0);
		tmp = vb->mix_ops[idx];
		vb->mix_ops[i] = vb->mix_ops[idx];
		vb->mix_ops[idx] = tmp;
	}
	return 0;
err:
	vmem_exit(bench, args);
	return -1;
}

static struct benchmark_info vmem_malloc_bench;
static struct benchmark_info vmem_mix_bench;
static struct benchmark_info vmem_free_bench;
static struct benchmark_info vmem_realloc_bench;
static struct benchmark_clo vmem_clo[7];

CONSTRUCTOR(vmem_persist_constructor)
void
vmem_persist_constructor(void)
{
	vmem_clo[0].opt_short = 'a';
	vmem_clo[0].opt_long = "stdlib-alloc";
	vmem_clo[0].descr = "Use stdlib allocator";
	vmem_clo[0].type = CLO_TYPE_FLAG;
	vmem_clo[0].off = clo_field_offset(struct vmem_args, stdlib_alloc);

	vmem_clo[1].opt_short = 'w';
	vmem_clo[1].opt_long = "no-warmup";
	vmem_clo[1].descr = "Do not perform warmup";
	vmem_clo[1].type = CLO_TYPE_FLAG;
	vmem_clo[1].off = clo_field_offset(struct vmem_args, no_warmup);

	vmem_clo[2].opt_short = 'p';
	vmem_clo[2].opt_long = "pool-per-thread";
	vmem_clo[2].descr = "Create separate pool per thread";
	vmem_clo[2].type = CLO_TYPE_FLAG;
	vmem_clo[2].off = clo_field_offset(struct vmem_args, pool_per_thread);

	vmem_clo[3].opt_short = 'm';
	vmem_clo[3].opt_long = "alloc-min";
	vmem_clo[3].type = CLO_TYPE_INT;
	vmem_clo[3].descr = "Min allocation size";
	vmem_clo[3].off = clo_field_offset(struct vmem_args, min_size);
	vmem_clo[3].def = "-1";
	vmem_clo[3].type_int.size = clo_field_size(struct vmem_args, min_size);
	vmem_clo[3].type_int.base = CLO_INT_BASE_DEC;
	vmem_clo[3].type_int.min = (-1);
	vmem_clo[3].type_int.max = INT_MAX;

	/*
	 * number of command line arguments is decremented to make below
	 * options available only for vmem_free and vmem_realloc benchmark
	 */
	vmem_clo[4].opt_short = 'T';
	vmem_clo[4].opt_long = "mix-thread";
	vmem_clo[4].descr = "Reallocate object allocated "
			    "by another thread";
	vmem_clo[4].type = CLO_TYPE_FLAG;
	vmem_clo[4].off = clo_field_offset(struct vmem_args, mix);

	/*
	 * number of command line arguments is decremented to make below
	 * options available only for vmem_realloc benchmark
	 */

	vmem_clo[5].opt_short = 'r';
	vmem_clo[5].opt_long = "realloc-size";
	vmem_clo[5].type = CLO_TYPE_UINT;
	vmem_clo[5].descr = "Reallocation size";
	vmem_clo[5].off = clo_field_offset(struct vmem_args, rsize);
	vmem_clo[5].def = "512";
	vmem_clo[5].type_uint.size = clo_field_size(struct vmem_args, rsize);
	vmem_clo[5].type_uint.base = CLO_INT_BASE_DEC;
	vmem_clo[5].type_uint.min = 0;
	vmem_clo[5].type_uint.max = ~0;

	vmem_clo[6].opt_short = 'R';
	vmem_clo[6].opt_long = "realloc-min";
	vmem_clo[6].type = CLO_TYPE_INT;
	vmem_clo[6].descr = "Min reallocation size";
	vmem_clo[6].off = clo_field_offset(struct vmem_args, min_rsize);
	vmem_clo[6].def = "-1";
	vmem_clo[6].type_int.size = clo_field_size(struct vmem_args, min_rsize);
	vmem_clo[6].type_int.base = CLO_INT_BASE_DEC;
	vmem_clo[6].type_int.min = -1;
	vmem_clo[6].type_int.max = INT_MAX;

	vmem_malloc_bench.name = "vmem_malloc";
	vmem_malloc_bench.brief = "vmem_malloc() benchmark";
	vmem_malloc_bench.init = vmem_init;
	vmem_malloc_bench.exit = vmem_exit_free;
	vmem_malloc_bench.multithread = true;
	vmem_malloc_bench.multiops = true;
	vmem_malloc_bench.init_worker = nullptr;
	vmem_malloc_bench.free_worker = nullptr;
	vmem_malloc_bench.operation = malloc_main_op;
	vmem_malloc_bench.clos = vmem_clo;
	vmem_malloc_bench.nclos = ARRAY_SIZE(vmem_clo) - 3;
	vmem_malloc_bench.opts_size = sizeof(struct vmem_args);
	vmem_malloc_bench.rm_file = true;
	vmem_malloc_bench.allow_poolset = false;
	REGISTER_BENCHMARK(vmem_malloc_bench);

	vmem_mix_bench.name = "vmem_mix";
	vmem_mix_bench.brief = "vmem_malloc() and vmem_free() "
			       "bechmark";
	vmem_mix_bench.init = vmem_mix_init;
	vmem_mix_bench.exit = vmem_exit_free;
	vmem_mix_bench.multithread = true;
	vmem_mix_bench.multiops = true;
	vmem_mix_bench.init_worker = vmem_init_worker;
	vmem_mix_bench.free_worker = nullptr;
	vmem_mix_bench.operation = vmem_mix_op;
	vmem_mix_bench.clos = vmem_clo;
	vmem_mix_bench.nclos = ARRAY_SIZE(vmem_clo) - 3;
	vmem_mix_bench.opts_size = sizeof(struct vmem_args);
	vmem_mix_bench.rm_file = true;
	vmem_mix_bench.allow_poolset = false;
	REGISTER_BENCHMARK(vmem_mix_bench);

	vmem_free_bench.name = "vmem_free";
	vmem_free_bench.brief = "vmem_free() benchmark";
	vmem_free_bench.init = vmem_init;
	vmem_free_bench.exit = vmem_exit;
	vmem_free_bench.multithread = true;
	vmem_free_bench.multiops = true;
	vmem_free_bench.init_worker = vmem_init_worker;
	vmem_free_bench.free_worker = nullptr;
	vmem_free_bench.operation = free_main_op;
	vmem_free_bench.clos = vmem_clo;
	vmem_free_bench.nclos = ARRAY_SIZE(vmem_clo) - 2;
	vmem_free_bench.opts_size = sizeof(struct vmem_args);
	vmem_free_bench.rm_file = true;
	vmem_free_bench.allow_poolset = false;
	REGISTER_BENCHMARK(vmem_free_bench);

	vmem_realloc_bench.name = "vmem_realloc";
	vmem_realloc_bench.brief = "Multithread benchmark vmem - "
				   "realloc";
	vmem_realloc_bench.init = vmem_realloc_init;
	vmem_realloc_bench.exit = vmem_exit_free;
	vmem_realloc_bench.multithread = true;
	vmem_realloc_bench.multiops = true;
	vmem_realloc_bench.init_worker = vmem_init_worker;
	vmem_realloc_bench.free_worker = nullptr;
	vmem_realloc_bench.operation = realloc_main_op;
	vmem_realloc_bench.clos = vmem_clo;
	vmem_realloc_bench.nclos = ARRAY_SIZE(vmem_clo);
	vmem_realloc_bench.opts_size = sizeof(struct vmem_args);
	vmem_realloc_bench.rm_file = true;
	vmem_realloc_bench.allow_poolset = false;
	REGISTER_BENCHMARK(vmem_realloc_bench);
};
