/*
 * Copyright 2015-2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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
 * blk.cpp -- pmemblk benchmarks definitions
 */

#include "benchmark.hpp"
#include "file.h"
#include "libpmem.h"
#include "libpmemblk.h"
#include "libpmempool.h"
#include "os.h"
#include "poolset_util.hpp"
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

struct blk_bench;
struct blk_worker;

/*
 * op_type -- type of operation
 */
enum op_type {
	OP_TYPE_UNKNOWN,
	OP_TYPE_BLK,
	OP_TYPE_FILE,
	OP_TYPE_MEMCPY,
};

/*
 * op_mode -- mode of the copy process
 */
enum op_mode {
	OP_MODE_UNKNOWN,
	OP_MODE_STAT, /* read/write always the same chunk */
	OP_MODE_SEQ,  /* read/write chunk by chunk */
	OP_MODE_RAND  /* read/write to chunks selected randomly */
};

/*
 * typedef for the worker function
 */
typedef int (*worker_fn)(struct blk_bench *, struct benchmark_args *,
			 struct blk_worker *, os_off_t);

/*
 * blk_args -- benchmark specific arguments
 */
struct blk_args {
	size_t fsize;   /* requested file size */
	bool no_warmup; /* don't do warmup */
	unsigned seed;  /* seed for randomization */
	char *type_str; /* type: blk, file, memcpy */
	char *mode_str; /* mode: stat, seq, rand */
};

/*
 * blk_bench -- pmemblk benchmark context
 */
struct blk_bench {
	PMEMblkpool *pbp;	 /* pmemblk handle */
	char *addr;		  /* address of user data (memcpy) */
	int fd;			  /* file descr. for file io */
	size_t nblocks;		  /* actual number of blocks */
	size_t blocks_per_thread; /* number of blocks per thread */
	worker_fn worker;	 /* worker function */
	enum op_type type;
	enum op_mode mode;
};

/*
 * struct blk_worker -- pmemblk worker context
 */
struct blk_worker {
	os_off_t *blocks; /* array with block numbers */
	char *buff;       /* buffer for read/write */
	unsigned seed;    /* worker seed */
};

/*
 * parse_op_type -- parse command line "--operation" argument
 *
 * Returns proper operation type.
 */
static enum op_type
parse_op_type(const char *arg)
{
	if (strcmp(arg, "blk") == 0)
		return OP_TYPE_BLK;
	else if (strcmp(arg, "file") == 0)
		return OP_TYPE_FILE;
	else if (strcmp(arg, "memcpy") == 0)
		return OP_TYPE_MEMCPY;
	else
		return OP_TYPE_UNKNOWN;
}

/*
 * parse_op_mode -- parse command line "--mode" argument
 *
 * Returns proper operation mode.
 */
static enum op_mode
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
 * blk_do_warmup -- perform warm-up by writing to each block
 */
static int
blk_do_warmup(struct blk_bench *bb, struct benchmark_args *args)
{
	size_t lba;
	int ret = 0;
	auto *buff = (char *)calloc(1, args->dsize);
	if (!buff) {
		perror("calloc");
		return -1;
	}

	for (lba = 0; lba < bb->nblocks; ++lba) {
		switch (bb->type) {
			case OP_TYPE_FILE: {
				size_t off = lba * args->dsize;
				if (pwrite(bb->fd, buff, args->dsize, off) !=
				    (ssize_t)args->dsize) {
					perror("pwrite");
					ret = -1;
					goto out;
				}
			} break;
			case OP_TYPE_BLK:
				if (pmemblk_write(bb->pbp, buff, lba) < 0) {
					perror("pmemblk_write");
					ret = -1;
					goto out;
				}
				break;
			case OP_TYPE_MEMCPY: {
				size_t off = lba * args->dsize;
				pmem_memcpy_persist((char *)bb->addr + off,
						    buff, args->dsize);
			} break;
			default:
				perror("unknown type");
				ret = -1;
				goto out;
		}
	}

out:
	free(buff);
	return ret;
}

/*
 * blk_read -- read function for pmemblk
 */
static int
blk_read(struct blk_bench *bb, struct benchmark_args *ba,
	 struct blk_worker *bworker, os_off_t off)
{
	if (pmemblk_read(bb->pbp, bworker->buff, off) < 0) {
		perror("pmemblk_read");
		return -1;
	}
	return 0;
}

/*
 * fileio_read -- read function for file io
 */
static int
fileio_read(struct blk_bench *bb, struct benchmark_args *ba,
	    struct blk_worker *bworker, os_off_t off)
{
	os_off_t file_off = off * ba->dsize;
	if (pread(bb->fd, bworker->buff, ba->dsize, file_off) !=
	    (ssize_t)ba->dsize) {
		perror("pread");
		return -1;
	}
	return 0;
}

/*
 * memcpy_read -- read function for memcpy
 */
static int
memcpy_read(struct blk_bench *bb, struct benchmark_args *ba,
	    struct blk_worker *bworker, os_off_t off)
{
	os_off_t file_off = off * ba->dsize;
	memcpy(bworker->buff, (char *)bb->addr + file_off, ba->dsize);
	return 0;
}

/*
 * blk_write -- write function for pmemblk
 */
static int
blk_write(struct blk_bench *bb, struct benchmark_args *ba,
	  struct blk_worker *bworker, os_off_t off)
{
	if (pmemblk_write(bb->pbp, bworker->buff, off) < 0) {
		perror("pmemblk_write");
		return -1;
	}
	return 0;
}

/*
 * memcpy_write -- write function for memcpy
 */
static int
memcpy_write(struct blk_bench *bb, struct benchmark_args *ba,
	     struct blk_worker *bworker, os_off_t off)
{
	os_off_t file_off = off * ba->dsize;
	pmem_memcpy_persist((char *)bb->addr + file_off, bworker->buff,
			    ba->dsize);
	return 0;
}

/*
 * fileio_write -- write function for file io
 */
static int
fileio_write(struct blk_bench *bb, struct benchmark_args *ba,
	     struct blk_worker *bworker, os_off_t off)
{
	os_off_t file_off = off * ba->dsize;
	if (pwrite(bb->fd, bworker->buff, ba->dsize, file_off) !=
	    (ssize_t)ba->dsize) {
		perror("pwrite");
		return -1;
	}
	return 0;
}

/*
 * blk_operation -- main operations for blk_read and blk_write benchmark
 */
static int
blk_operation(struct benchmark *bench, struct operation_info *info)
{
	auto *bb = (struct blk_bench *)pmembench_get_priv(bench);
	auto *bworker = (struct blk_worker *)info->worker->priv;

	os_off_t off = bworker->blocks[info->index];
	return bb->worker(bb, info->args, bworker, off);
}

/*
 * blk_init_worker -- initialize worker
 */
static int
blk_init_worker(struct benchmark *bench, struct benchmark_args *args,
		struct worker_info *worker)
{
	struct blk_worker *bworker =
		(struct blk_worker *)malloc(sizeof(*bworker));

	if (!bworker) {
		perror("malloc");
		return -1;
	}

	auto *bb = (struct blk_bench *)pmembench_get_priv(bench);
	auto *bargs = (struct blk_args *)args->opts;

	bworker->seed = os_rand_r(&bargs->seed);

	bworker->buff = (char *)malloc(args->dsize);
	if (!bworker->buff) {
		perror("malloc");
		goto err_buff;
	}

	/* fill buffer with some random data */
	memset(bworker->buff, bworker->seed, args->dsize);

	assert(args->n_ops_per_thread != 0);
	bworker->blocks = (os_off_t *)malloc(sizeof(*bworker->blocks) *
					     args->n_ops_per_thread);
	if (!bworker->blocks) {
		perror("malloc");
		goto err_blocks;
	}

	switch (bb->mode) {
		case OP_MODE_RAND:
			for (size_t i = 0; i < args->n_ops_per_thread; i++) {
				bworker->blocks[i] =
					worker->index * bb->blocks_per_thread +
					os_rand_r(&bworker->seed) %
						bb->blocks_per_thread;
			}
			break;
		case OP_MODE_SEQ:
			for (size_t i = 0; i < args->n_ops_per_thread; i++)
				bworker->blocks[i] = i % bb->blocks_per_thread;
			break;
		case OP_MODE_STAT:
			for (size_t i = 0; i < args->n_ops_per_thread; i++)
				bworker->blocks[i] = 0;
			break;
		default:
			perror("unknown mode");
			goto err_mode;
	}

	worker->priv = bworker;
	return 0;

err_mode:
	free(bworker->blocks);
err_blocks:
	free(bworker->buff);
err_buff:
	free(bworker);

	return -1;
}

/*
 * blk_free_worker -- cleanup worker
 */
static void
blk_free_worker(struct benchmark *bench, struct benchmark_args *args,
		struct worker_info *worker)
{
	auto *bworker = (struct blk_worker *)worker->priv;
	free(bworker->blocks);
	free(bworker->buff);
	free(bworker);
}

/*
 * blk_init -- function for initialization benchmark
 */
static int
blk_init(struct blk_bench *bb, struct benchmark_args *args)
{
	auto *ba = (struct blk_args *)args->opts;
	assert(ba != nullptr);

	char path[PATH_MAX];
	if (util_safe_strcpy(path, args->fname, sizeof(path)) != 0)
		return -1;

	bb->type = parse_op_type(ba->type_str);
	if (bb->type == OP_TYPE_UNKNOWN) {
		fprintf(stderr, "Invalid operation argument '%s'",
			ba->type_str);
		return -1;
	}

	enum file_type type = util_file_get_type(args->fname);
	if (type == OTHER_ERROR) {
		fprintf(stderr, "could not check type of file %s\n",
			args->fname);
		return -1;
	}

	if (bb->type == OP_TYPE_FILE && type == TYPE_DEVDAX) {
		fprintf(stderr, "fileio not supported on device dax\n");
		return -1;
	}

	bb->mode = parse_op_mode(ba->mode_str);
	if (bb->mode == OP_MODE_UNKNOWN) {
		fprintf(stderr, "Invalid mode argument '%s'", ba->mode_str);
		return -1;
	}

	if (ba->fsize == 0)
		ba->fsize = PMEMBLK_MIN_POOL;

	size_t req_fsize = ba->fsize;

	if (ba->fsize / args->dsize < args->n_threads ||
	    ba->fsize < PMEMBLK_MIN_POOL) {
		fprintf(stderr, "too small file size\n");
		return -1;
	}

	if (args->dsize >= ba->fsize) {
		fprintf(stderr, "block size bigger than file size\n");
		return -1;
	}

	if (args->is_poolset || type == TYPE_DEVDAX) {
		if (args->fsize < ba->fsize) {
			fprintf(stderr, "file size too large\n");
			return -1;
		}

		ba->fsize = 0;
	} else if (args->is_dynamic_poolset) {
		int ret = dynamic_poolset_create(args->fname, ba->fsize);
		if (ret == -1)
			return -1;

		if (util_safe_strcpy(path, POOLSET_PATH, sizeof(path)) != 0)
			return -1;

		ba->fsize = 0;
	}

	bb->fd = -1;

	/*
	 * Create pmemblk in order to get the number of blocks
	 * even for file-io mode.
	 */
	bb->pbp = pmemblk_create(path, args->dsize, ba->fsize, args->fmode);

	if (bb->pbp == nullptr) {
		perror("pmemblk_create");
		return -1;
	}

	bb->nblocks = pmemblk_nblock(bb->pbp);

	/* limit the number of used blocks */
	if (bb->nblocks > req_fsize / args->dsize)
		bb->nblocks = req_fsize / args->dsize;

	if (bb->nblocks < args->n_threads) {
		fprintf(stderr, "too small file size");
		goto out_close;
	}

	if (bb->type == OP_TYPE_FILE) {
		pmemblk_close(bb->pbp);
		bb->pbp = nullptr;

		int flags = O_RDWR | O_CREAT | O_SYNC;
#ifdef _WIN32
		flags |= O_BINARY;
#endif
		bb->fd = os_open(args->fname, flags, args->fmode);
		if (bb->fd < 0) {
			perror("open");
			return -1;
		}
	} else if (bb->type == OP_TYPE_MEMCPY) {
		/* skip pool header, so addr points to the first block */
		bb->addr = (char *)bb->pbp + 8192;
	}

	bb->blocks_per_thread = bb->nblocks / args->n_threads;

	if (!ba->no_warmup) {
		if (blk_do_warmup(bb, args) != 0)
			goto out_close;
	}

	return 0;
out_close:
	if (bb->type == OP_TYPE_FILE)
		os_close(bb->fd);
	else
		pmemblk_close(bb->pbp);
	return -1;
}

/*
 * blk_read_init - function for initializing blk_read benchmark
 */
static int
blk_read_init(struct benchmark *bench, struct benchmark_args *args)
{
	assert(bench != nullptr);
	assert(args != nullptr);

	int ret;
	auto *bb = (struct blk_bench *)malloc(sizeof(struct blk_bench));
	if (bb == nullptr) {
		perror("malloc");
		return -1;
	}

	pmembench_set_priv(bench, bb);

	ret = blk_init(bb, args);
	if (ret != 0) {
		free(bb);
		return ret;
	}

	switch (bb->type) {
		case OP_TYPE_FILE:
			bb->worker = fileio_read;
			break;
		case OP_TYPE_BLK:
			bb->worker = blk_read;
			break;
		case OP_TYPE_MEMCPY:
			bb->worker = memcpy_read;
			break;
		default:
			perror("unknown operation type");
			return -1;
	}

	return ret;
}

/*
 * blk_write_init - function for initializing blk_write benchmark
 */
static int
blk_write_init(struct benchmark *bench, struct benchmark_args *args)
{
	assert(bench != nullptr);
	assert(args != nullptr);

	int ret;
	auto *bb = (struct blk_bench *)malloc(sizeof(struct blk_bench));
	if (bb == nullptr) {
		perror("malloc");
		return -1;
	}

	pmembench_set_priv(bench, bb);

	ret = blk_init(bb, args);
	if (ret != 0) {
		free(bb);
		return ret;
	}

	switch (bb->type) {
		case OP_TYPE_FILE:
			bb->worker = fileio_write;
			break;
		case OP_TYPE_BLK:
			bb->worker = blk_write;
			break;
		case OP_TYPE_MEMCPY:
			bb->worker = memcpy_write;
			break;
		default:
			perror("unknown operation type");
			return -1;
	}

	return ret;
}

/*
 * blk_exit -- function for de-initialization benchmark
 */
static int
blk_exit(struct benchmark *bench, struct benchmark_args *args)
{
	auto *bb = (struct blk_bench *)pmembench_get_priv(bench);
	char path[PATH_MAX];
	if (util_safe_strcpy(path, args->fname, sizeof(path)) != 0)
		return -1;

	if (args->is_dynamic_poolset) {
		if (util_safe_strcpy(path, POOLSET_PATH, sizeof(path)) != 0)
			return -1;
	}

	int result;
	switch (bb->type) {
		case OP_TYPE_FILE:
			os_close(bb->fd);
			break;
		case OP_TYPE_BLK:
			pmemblk_close(bb->pbp);
			result = pmemblk_check(path, args->dsize);
			if (result < 0) {
				perror("pmemblk_check error");
				return -1;
			} else if (result == 0) {
				perror("pmemblk_check: not consistent");
				return -1;
			}
			break;
		case OP_TYPE_MEMCPY:
			pmemblk_close(bb->pbp);
			break;
		default:
			perror("unknown operation type");
			return -1;
	}

	free(bb);
	return 0;
}

static struct benchmark_clo blk_clo[5];
static struct benchmark_info blk_read_info;
static struct benchmark_info blk_write_info;

CONSTRUCTOR(blk_constructor)
void
blk_constructor(void)
{
	blk_clo[0].opt_short = 'o';
	blk_clo[0].opt_long = "operation";
	blk_clo[0].descr = "Operation type - blk, file, memcpy";
	blk_clo[0].type = CLO_TYPE_STR;
	blk_clo[0].off = clo_field_offset(struct blk_args, type_str);
	blk_clo[0].def = "blk";

	blk_clo[1].opt_short = 'w';
	blk_clo[1].opt_long = "no-warmup";
	blk_clo[1].descr = "Don't do warmup";
	blk_clo[1].type = CLO_TYPE_FLAG;
	blk_clo[1].off = clo_field_offset(struct blk_args, no_warmup);

	blk_clo[2].opt_short = 'm';
	blk_clo[2].opt_long = "mode";
	blk_clo[2].descr = "Reading/writing mode - stat, seq, rand";
	blk_clo[2].type = CLO_TYPE_STR;
	blk_clo[2].off = clo_field_offset(struct blk_args, mode_str);
	blk_clo[2].def = "seq";

	blk_clo[3].opt_short = 'S';
	blk_clo[3].opt_long = "seed";
	blk_clo[3].descr = "Random seed";
	blk_clo[3].off = clo_field_offset(struct blk_args, seed);
	blk_clo[3].def = "1";
	blk_clo[3].type = CLO_TYPE_UINT;
	blk_clo[3].type_uint.size = clo_field_size(struct blk_args, seed);
	blk_clo[3].type_uint.base = CLO_INT_BASE_DEC;
	blk_clo[3].type_uint.min = 1;
	blk_clo[3].type_uint.max = UINT_MAX;

	blk_clo[4].opt_short = 's';
	blk_clo[4].opt_long = "file-size";
	blk_clo[4].descr = "Requested file size in bytes - 0 means minimum";
	blk_clo[4].type = CLO_TYPE_UINT;
	blk_clo[4].off = clo_field_offset(struct blk_args, fsize);
	blk_clo[4].def = "0";
	blk_clo[4].type_uint.size = clo_field_size(struct blk_args, fsize);
	blk_clo[4].type_uint.base = CLO_INT_BASE_DEC;
	blk_clo[4].type_uint.min = 0;
	blk_clo[4].type_uint.max = ~0;

	blk_read_info.name = "blk_read";
	blk_read_info.brief = "Benchmark for blk_read() operation";
	blk_read_info.init = blk_read_init;
	blk_read_info.exit = blk_exit;
	blk_read_info.multithread = true;
	blk_read_info.multiops = true;
	blk_read_info.init_worker = blk_init_worker;
	blk_read_info.free_worker = blk_free_worker;
	blk_read_info.operation = blk_operation;
	blk_read_info.clos = blk_clo;
	blk_read_info.nclos = ARRAY_SIZE(blk_clo);
	blk_read_info.opts_size = sizeof(struct blk_args);
	blk_read_info.rm_file = true;
	blk_read_info.allow_poolset = true;

	REGISTER_BENCHMARK(blk_read_info);

	blk_write_info.name = "blk_write";
	blk_write_info.brief = "Benchmark for blk_write() operation";
	blk_write_info.init = blk_write_init;
	blk_write_info.exit = blk_exit;
	blk_write_info.multithread = true;
	blk_write_info.multiops = true;
	blk_write_info.init_worker = blk_init_worker;
	blk_write_info.free_worker = blk_free_worker;
	blk_write_info.operation = blk_operation;
	blk_write_info.clos = blk_clo;
	blk_write_info.nclos = ARRAY_SIZE(blk_clo);
	blk_write_info.opts_size = sizeof(struct blk_args);
	blk_write_info.rm_file = true;
	blk_write_info.allow_poolset = true;

	REGISTER_BENCHMARK(blk_write_info);
}
