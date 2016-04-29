/*
 * Copyright 2015-2016, Intel Corporation
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
 * pmem_memset.c -- benchmark for pmem_memset function
 */

#include <libpmem.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>

#include "benchmark.h"

#define MAX_OFFSET 63
#define CONST_B 0xFF

struct memset_bench;

typedef int (*operation_fn) (void *dest, int c, size_t len);

/*
 * memset_args -- benchmark specific command line options
 */
struct memset_args
{
	char *mode;		/* operation mode: stat, seq, rand */
	bool memset;		/* use libc memset function */
	bool persist;		/* perform persist operation */
	bool no_warmup;		/* do not do warmup */
	size_t chunk_size;	/* elementary chunk size */
	size_t dest_off;	/* destination address offset */
	unsigned int seed;	/* seed for random numbers */
};

/*
 * memset_bench -- benchmark context
 */
struct memset_bench {
	struct memset_args *pargs; /* benchmark specific arguments */
	uint64_t *offsets;	/* random/sequential address offsets */
	int n_offsets;		/* number of random elements */
	int const_b;		/* memset() value */
	size_t fsize;		/* file size */
	void *pmem_addr;	/* mapped file address */
	operation_fn func_op;	/* operation function */
};

struct memset_worker {
};

static struct benchmark_clo memset_clo[] = {
	{
		.opt_short	= 'M',
		.opt_long	= "mem-mode",
		.descr		= "Memory writing mode - stat, seq, rand",
		.def		= "seq",
		.off		= clo_field_offset(struct memset_args, mode),
		.type		= CLO_TYPE_STR,
	},
	{
		.opt_short	= 'm',
		.opt_long	= "memset",
		.descr		= "Use libc memset()",
		.def		= "false",
		.off		= clo_field_offset(struct memset_args, memset),
		.type		= CLO_TYPE_FLAG
	},
	{
		.opt_short	= 'p',
		.opt_long	= "persist",
		.descr		= "Use pmem_persist()",
		.def		= "true",
		.off		= clo_field_offset(struct memset_args, persist),
		.type		= CLO_TYPE_FLAG
	},
	{
		.opt_short	= 'D',
		.opt_long	= "dest-offset",
		.descr		= "Destination cache line alignment offset",
		.def		= "0",
		.off	= clo_field_offset(struct memset_args, dest_off),
		.type		= CLO_TYPE_UINT,
		.type_uint	= {
			.size	= clo_field_size(struct memset_args, dest_off),
			.base	= CLO_INT_BASE_DEC,
			.min	= 0,
			.max	= MAX_OFFSET
		}
	},
	{
		.opt_short	= 'w',
		.opt_long	= "no-warmup",
		.descr		= "Don't do warmup",
		.def		= false,
		.type		= CLO_TYPE_FLAG,
		.off	= clo_field_offset(struct memset_args, no_warmup),
	},
	{
		.opt_short	= 'S',
		.opt_long	= "seed",
		.descr		= "seed for random numbers",
		.def		= "1",
		.off		= clo_field_offset(struct memset_args, seed),
		.type		= CLO_TYPE_UINT,
		.type_uint	= {
			.size	= clo_field_size(struct memset_args, seed),
			.base	= CLO_INT_BASE_DEC,
			.min	= 1,
			.max	= UINT_MAX,
		},
	},
};

/*
 * operation_mode -- mode of operation of memset()
 */
enum operation_mode {
	OP_MODE_UNKNOWN,
	OP_MODE_STAT,	/* always use the same chunk */
	OP_MODE_SEQ,	/* use consecutive chunks */
	OP_MODE_RAND	/* use random chunks */
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
	uint64_t n_threads = args->n_threads;
	uint64_t n_ops = args->n_ops_per_thread;

	mb->n_offsets = n_ops * n_threads;
	mb->offsets = malloc(mb->n_offsets * sizeof(*mb->offsets));
	if (!mb->offsets) {
		perror("malloc");
		return -1;
	}

	unsigned int seed = mb->pargs->seed;

	for (uint64_t i = 0; i < n_threads; i++) {
		for (uint64_t j = 0; j < n_ops; j++) {
			uint64_t o;
			switch (op_mode) {
				case OP_MODE_STAT:
					o = i;
					break;
				case OP_MODE_SEQ:
					o = i * n_ops + j;
					break;
				case OP_MODE_RAND:
					o = i * n_ops + rand_r(&seed) % n_ops;
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
 * do_warmup -- does the warmup by writing the whole pool area
 */
static int
do_warmup(struct memset_bench *mb, size_t nops)
{
	void *dest = mb->pmem_addr;
	int c = mb->const_b;
	size_t len = mb->fsize;

	pmem_memset_persist(dest, c, len);

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
	struct memset_bench *mb =
		(struct memset_bench *)pmembench_get_priv(bench);

	assert(info->index < mb->n_offsets);

	uint64_t idx = info->worker->index * info->args->n_ops_per_thread
						+ info->index;
	void *dest = (char *)mb->pmem_addr + mb->offsets[idx]
						+ mb->pargs->dest_off;
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
	assert(bench != NULL);
	assert(args != NULL);
	assert(args->opts != NULL);

	int ret = 0;

	struct memset_bench *mb = malloc(sizeof(struct memset_bench));
	if (!mb) {
		perror("malloc");
		return -1;
	}

	mb->pargs = args->opts;
	mb->pargs->chunk_size = args->dsize;

	enum operation_mode op_mode = parse_op_mode(mb->pargs->mode);
	if (op_mode == OP_MODE_UNKNOWN) {
		fprintf(stderr, "Invalid operation mode argument '%s'",
			mb->pargs->mode);
		ret = -1;
		goto err_free_mb;
	}

	size_t size = MAX_OFFSET + mb->pargs->chunk_size;
	size_t large = size * args->n_ops_per_thread * args->n_threads;
	size_t small = size * args->n_threads;

	mb->fsize = (op_mode == OP_MODE_STAT) ? small : large;

	/* initialize offsets[] array depending on benchmark args */
	if (init_offsets(args, mb, op_mode) < 0) {
		ret = -1;
		goto err_free_mb;
	}

	/* initialize memset() value */
	mb->const_b = CONST_B;

	/* create a pmem file and memory map it */
	if ((mb->pmem_addr = pmem_map_file(args->fname, mb->fsize,
				PMEM_FILE_CREATE|PMEM_FILE_EXCL,
				args->fmode, NULL, NULL)) == NULL) {
		perror(args->fname);
		ret = -1;
		goto err_free_offsets;
	}

	if (mb->pargs->memset)
		mb->func_op = (mb->pargs->persist) ?
				libc_memset_persist : libc_memset;
	else
		mb->func_op = (mb->pargs->persist) ?
				libpmem_memset_persist : libpmem_memset_nodrain;

	if (!mb->pargs->no_warmup) {
		if (do_warmup(
			mb, args->n_threads * args->n_ops_per_thread) != 0) {
			fprintf(stderr, "do_warmup() function failed.");
			ret = -1;
			goto err_unmap;
		}
	}

	pmembench_set_priv(bench, mb);

	return 0;

err_unmap:
	pmem_unmap(mb->pmem_addr, mb->fsize);
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
	struct memset_bench *mb =
		(struct memset_bench *)pmembench_get_priv(bench);
	pmem_unmap(mb->pmem_addr, mb->fsize);
	free(mb->offsets);
	free(mb);
	return 0;
}

/* Stores information about benchmark. */
static struct benchmark_info memset_info = {
	.name		= "pmem_memset",
	.brief		= "Benchmark for pmem_memset_persist() and"
				" pmem_memset_nodrain() operations",
	.init		= memset_init,
	.exit		= memset_exit,
	.multithread	= true,
	.multiops	= true,
	.operation	= memset_op,
	.measure_time	= true,
	.clos		= memset_clo,
	.nclos		= ARRAY_SIZE(memset_clo),
	.opts_size	= sizeof(struct memset_args),
	.rm_file	= true,
	.allow_poolset	= false,
};

REGISTER_BENCHMARK(memset_info);
