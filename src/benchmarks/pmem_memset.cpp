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
 * pmem_memset.cpp -- benchmark for pmem_memset function
 */

#include <cassert>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <libpmem.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "benchmark.hpp"
#include "file.h"
#include "os.h"

#define MAX_OFFSET 63
#define CONST_B 0xFF

struct memset_bench;

typedef int (*operation_fn)(void *dest, int c, size_t len);

/*
 * memset_args -- benchmark specific command line options
 */
struct memset_args {
	char *mode;	/* operation mode: stat, seq, rand */
	bool memset;       /* use libc memset function */
	bool persist;      /* perform persist operation */
	bool msync;	/* perform msync operation */
	bool no_warmup;    /* do not do warmup */
	size_t chunk_size; /* elementary chunk size */
	size_t dest_off;   /* destination address offset */
	unsigned seed;     /* seed for random numbers */
};

/*
 * memset_bench -- benchmark context
 */
struct memset_bench {
	struct memset_args *pargs; /* benchmark specific arguments */
	uint64_t *offsets;	 /* random/sequential address offsets */
	size_t n_offsets;	  /* number of random elements */
	int const_b;		   /* memset() value */
	size_t fsize;		   /* file size */
	void *pmem_addr;	   /* mapped file address */
	operation_fn func_op;      /* operation function */
};

/*
 * operation_mode -- mode of operation of memset()
 */
enum operation_mode {
	OP_MODE_UNKNOWN,
	OP_MODE_STAT, /* always use the same chunk */
	OP_MODE_SEQ,  /* use consecutive chunks */
	OP_MODE_RAND  /* use random chunks */
};

/*
 * parse_op_mode -- parse operation mode from string
 */
static enum operation_mode
parse_op_mode(const char *arg)
{
	if (strcmp(arg, "stat") == 0)
		return OP_MODE_STAT;
	else if (strcmp(arg, "seq") == 0)
		return OP_MODE_SEQ;
	else if (strcmp(arg, "rand") == 0)
		return OP_MODE_RAND;
	else
		return OP_MODE_UNKNOWN;
}

/*
 * init_offsets -- initialize offsets[] array depending on the selected mode
 */
static int
init_offsets(struct benchmark_args *args, struct memset_bench *mb,
	     enum operation_mode op_mode)
{
	unsigned n_threads = args->n_threads;
	size_t n_ops = args->n_ops_per_thread;

	mb->n_offsets = n_ops * n_threads;
	assert(mb->n_offsets != 0);
	mb->offsets = (uint64_t *)malloc(mb->n_offsets * sizeof(*mb->offsets));
	if (!mb->offsets) {
		perror("malloc");
		return -1;
	}

	unsigned seed = mb->pargs->seed;

	for (unsigned i = 0; i < n_threads; i++) {
		for (size_t j = 0; j < n_ops; j++) {
			size_t o;
			switch (op_mode) {
				case OP_MODE_STAT:
					o = i;
					break;
				case OP_MODE_SEQ:
					o = i * n_ops + j;
					break;
				case OP_MODE_RAND:
					o = i * n_ops +
						os_rand_r(&seed) % n_ops;
					break;
				default:
					assert(0);
					return -1;
			}
			mb->offsets[i * n_ops + j] = o * mb->pargs->chunk_size;
		}
	}

	return 0;
}

/*
 * libpmem_memset_persist -- perform operation using libpmem
 * pmem_memset_persist().
 */
static int
libpmem_memset_persist(void *dest, int c, size_t len)
{
	pmem_memset_persist(dest, c, len);

	return 0;
}

/*
 * libpmem_memset_nodrain -- perform operation using libpmem
 * pmem_memset_nodrain().
 */
static int
libpmem_memset_nodrain(void *dest, int c, size_t len)
{
	pmem_memset_nodrain(dest, c, len);

	return 0;
}

/*
 * libc_memset_persist -- perform operation using libc memset() function
 * followed by pmem_persist().
 */
static int
libc_memset_persist(void *dest, int c, size_t len)
{
	memset(dest, c, len);

	pmem_persist(dest, len);

	return 0;
}

/*
 * libc_memset_msync -- perform operation using libc memset() function
 * followed by pmem_msync().
 */
static int
libc_memset_msync(void *dest, int c, size_t len)
{
	memset(dest, c, len);

	return pmem_msync(dest, len);
}

/*
 * libc_memset -- perform operation using libc memset() function
 * followed by pmem_flush().
 */
static int
libc_memset(void *dest, int c, size_t len)
{
	memset(dest, c, len);

	pmem_flush(dest, len);

	return 0;
}

/*
 * warmup_persist -- does the warmup by writing the whole pool area
 */
static int
warmup_persist(struct memset_bench *mb)
{
	void *dest = mb->pmem_addr;
	int c = mb->const_b;
	size_t len = mb->fsize;

	pmem_memset_persist(dest, c, len);

	return 0;
}

/*
 * warmup_msync -- does the warmup by writing the whole pool area
 */
static int
warmup_msync(struct memset_bench *mb)
{
	void *dest = mb->pmem_addr;
	int c = mb->const_b;
	size_t len = mb->fsize;

	return libc_memset_msync(dest, c, len);
}

/*
 * memset_op -- actual benchmark operation. It can have one of the four
 * functions assigned:
 *              libc_memset,
 *              libc_memset_persist,
 *              libpmem_memset_nodrain,
 *              libpmem_memset_persist.
 */
static int
memset_op(struct benchmark *bench, struct operation_info *info)
{
	auto *mb = (struct memset_bench *)pmembench_get_priv(bench);

	assert(info->index < mb->n_offsets);

	size_t idx = info->worker->index * info->args->n_ops_per_thread +
		info->index;
	void *dest =
		(char *)mb->pmem_addr + mb->offsets[idx] + mb->pargs->dest_off;
	int c = mb->const_b;
	size_t len = mb->pargs->chunk_size;

	mb->func_op(dest, c, len);

	return 0;
}

/*
 * memset_init -- initialization function
 */
static int
memset_init(struct benchmark *bench, struct benchmark_args *args)
{
	assert(bench != nullptr);
	assert(args != nullptr);
	assert(args->opts != nullptr);

	int ret = 0;
	size_t size;
	size_t large;
	size_t little;
	size_t file_size = 0;
	int flags = 0;

	enum file_type type = util_file_get_type(args->fname);
	if (type == OTHER_ERROR) {
		fprintf(stderr, "could not check type of file %s\n",
			args->fname);
		return -1;
	}

	int (*warmup_func)(struct memset_bench *) = warmup_persist;
	auto *mb = (struct memset_bench *)malloc(sizeof(struct memset_bench));
	if (!mb) {
		perror("malloc");
		return -1;
	}

	mb->pargs = (struct memset_args *)args->opts;
	mb->pargs->chunk_size = args->dsize;

	enum operation_mode op_mode = parse_op_mode(mb->pargs->mode);
	if (op_mode == OP_MODE_UNKNOWN) {
		fprintf(stderr, "Invalid operation mode argument '%s'\n",
			mb->pargs->mode);
		ret = -1;
		goto err_free_mb;
	}

	size = MAX_OFFSET + mb->pargs->chunk_size;
	large = size * args->n_ops_per_thread * args->n_threads;
	little = size * args->n_threads;

	mb->fsize = (op_mode == OP_MODE_STAT) ? little : large;

	/* initialize offsets[] array depending on benchmark args */
	if (init_offsets(args, mb, op_mode) < 0) {
		ret = -1;
		goto err_free_mb;
	}

	/* initialize memset() value */
	mb->const_b = CONST_B;

	if (type != TYPE_DEVDAX) {
		file_size = mb->fsize;
		flags = PMEM_FILE_CREATE | PMEM_FILE_EXCL;
	}

	/* create a pmem file and memory map it */
	if ((mb->pmem_addr = pmem_map_file(args->fname, file_size, flags,
					   args->fmode, nullptr, nullptr)) ==
	    nullptr) {
		perror(args->fname);
		ret = -1;
		goto err_free_offsets;
	}

	if (mb->pargs->memset) {
		if (mb->pargs->persist && mb->pargs->msync) {
			fprintf(stderr,
				"Invalid benchmark parameters: persist and msync cannot be specified together\n");
			ret = -1;
			goto err_free_offsets;
		}

		if (mb->pargs->persist) {
			mb->func_op = libc_memset_persist;
		} else if (mb->pargs->msync) {
			mb->func_op = libc_memset_msync;
			warmup_func = warmup_msync;
		} else {
			mb->func_op = libc_memset;
		}
	} else {
		mb->func_op = (mb->pargs->persist) ? libpmem_memset_persist
						   : libpmem_memset_nodrain;
	}

	if (!mb->pargs->no_warmup && type != TYPE_DEVDAX) {
		ret = warmup_func(mb);
		if (ret) {
			perror("Pool warmup failed");
			goto err_free_offsets;
		}
	}

	pmembench_set_priv(bench, mb);

	return ret;

err_free_offsets:
	free(mb->offsets);
err_free_mb:
	free(mb);

	return ret;
}

/*
 * memset_exit -- benchmark cleanup function
 */
static int
memset_exit(struct benchmark *bench, struct benchmark_args *args)
{
	auto *mb = (struct memset_bench *)pmembench_get_priv(bench);
	pmem_unmap(mb->pmem_addr, mb->fsize);
	free(mb->offsets);
	free(mb);
	return 0;
}

static struct benchmark_clo memset_clo[7];
/* Stores information about benchmark. */
static struct benchmark_info memset_info;
CONSTRUCTOR(pmem_memset_constructor)
void
pmem_memset_constructor(void)
{
	memset_clo[0].opt_short = 'M';
	memset_clo[0].opt_long = "mem-mode";
	memset_clo[0].descr = "Memory writing mode - "
			      "stat, seq, rand";
	memset_clo[0].def = "seq";
	memset_clo[0].off = clo_field_offset(struct memset_args, mode);
	memset_clo[0].type = CLO_TYPE_STR;

	memset_clo[1].opt_short = 'm';
	memset_clo[1].opt_long = "memset";
	memset_clo[1].descr = "Use libc memset()";
	memset_clo[1].def = "false";
	memset_clo[1].off = clo_field_offset(struct memset_args, memset);
	memset_clo[1].type = CLO_TYPE_FLAG;

	memset_clo[2].opt_short = 'p';
	memset_clo[2].opt_long = "persist";
	memset_clo[2].descr = "Use pmem_persist()";
	memset_clo[2].def = "true";
	memset_clo[2].off = clo_field_offset(struct memset_args, persist);
	memset_clo[2].type = CLO_TYPE_FLAG;

	memset_clo[3].opt_short = 'D';
	memset_clo[3].opt_long = "dest-offset";
	memset_clo[3].descr = "Destination cache line alignment "
			      "offset";
	memset_clo[3].def = "0";
	memset_clo[3].off = clo_field_offset(struct memset_args, dest_off);
	memset_clo[3].type = CLO_TYPE_UINT;
	memset_clo[3].type_uint.size =
		clo_field_size(struct memset_args, dest_off);
	memset_clo[3].type_uint.base = CLO_INT_BASE_DEC;
	memset_clo[3].type_uint.min = 0;
	memset_clo[3].type_uint.max = MAX_OFFSET;

	memset_clo[4].opt_short = 'w';
	memset_clo[4].opt_long = "no-warmup";
	memset_clo[4].descr = "Don't do warmup";
	memset_clo[4].def = "false";
	memset_clo[4].type = CLO_TYPE_FLAG;
	memset_clo[4].off = clo_field_offset(struct memset_args, no_warmup);

	memset_clo[5].opt_short = 'S';
	memset_clo[5].opt_long = "seed";
	memset_clo[5].descr = "seed for random numbers";
	memset_clo[5].def = "1";
	memset_clo[5].off = clo_field_offset(struct memset_args, seed);
	memset_clo[5].type = CLO_TYPE_UINT;
	memset_clo[5].type_uint.size = clo_field_size(struct memset_args, seed);
	memset_clo[5].type_uint.base = CLO_INT_BASE_DEC;
	memset_clo[5].type_uint.min = 1;
	memset_clo[5].type_uint.max = UINT_MAX;

	memset_clo[6].opt_short = 's';
	memset_clo[6].opt_long = "msync";
	memset_clo[6].descr = "Use pmem_msync()";
	memset_clo[6].def = "false";
	memset_clo[6].off = clo_field_offset(struct memset_args, msync);
	memset_clo[6].type = CLO_TYPE_FLAG;

	memset_info.name = "pmem_memset";
	memset_info.brief = "Benchmark for pmem_memset_persist() "
			    "and pmem_memset_nodrain() operations";
	memset_info.init = memset_init;
	memset_info.exit = memset_exit;
	memset_info.multithread = true;
	memset_info.multiops = true;
	memset_info.operation = memset_op;
	memset_info.measure_time = true;
	memset_info.clos = memset_clo;
	memset_info.nclos = ARRAY_SIZE(memset_clo);
	memset_info.opts_size = sizeof(struct memset_args);
	memset_info.rm_file = true;
	memset_info.allow_poolset = false;
	memset_info.print_bandwidth = true;
	REGISTER_BENCHMARK(memset_info);
};
