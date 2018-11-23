/*
 * Copyright 2016-2018, Intel Corporation
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
 * rpmem_persist.cpp -- rpmem persist benchmarks definition
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
#include "libpmem.h"
#include "librpmem.h"
#include "os.h"
#include "set.h"
#include "util.h"

#define CL_ALIGNMENT 64
#define MAX_OFFSET (CL_ALIGNMENT - 1)

#define ALIGN_CL(x) (((x) + CL_ALIGNMENT - 1) & ~(CL_ALIGNMENT - 1))

/*
 * rpmem_args -- benchmark specific command line options
 */
struct rpmem_args {
	char *mode;	/* operation mode: stat, seq, rand */
	bool no_warmup;    /* do not do warmup */
	bool no_memset;    /* do not call memset before each persist */
	size_t chunk_size; /* elementary chunk size */
	size_t dest_off;   /* destination address offset */
	bool relaxed;      /* use RPMEM_PERSIST_RELAXED flag */
};

/*
 * rpmem_bench -- benchmark context
 */
struct rpmem_bench {
	struct rpmem_args *pargs; /* benchmark specific arguments */
	size_t *offsets;	  /* random/sequential address offsets */
	size_t n_offsets;	 /* number of random elements */
	int const_b;		  /* memset() value */
	size_t min_size;	  /* minimum file size */
	void *addrp;		  /* mapped file address */
	void *pool;		  /* memory pool address */
	size_t pool_size;	 /* size of memory pool */
	size_t mapped_len;	/* mapped length */
	RPMEMpool **rpp;	  /* rpmem pool pointers */
	unsigned *nlanes;	 /* number of lanes for each remote replica */
	unsigned nreplicas;       /* number of remote replicas */
	size_t csize_align;       /* aligned elementary chunk size */
	unsigned flags;		  /* flags for rpmem_persist */
};

/*
 * operation_mode -- mode of operation
 */
enum operation_mode {
	OP_MODE_UNKNOWN,
	OP_MODE_STAT,      /* always use the same chunk */
	OP_MODE_SEQ,       /* use consecutive chunks */
	OP_MODE_RAND,      /* use random chunks */
	OP_MODE_SEQ_WRAP,  /* use consequtive chunks, but use file size */
	OP_MODE_RAND_WRAP, /* use random chunks, but use file size */
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
	else if (strcmp(arg, "seq-wrap") == 0)
		return OP_MODE_SEQ_WRAP;
	else if (strcmp(arg, "rand-wrap") == 0)
		return OP_MODE_RAND_WRAP;
	else
		return OP_MODE_UNKNOWN;
}

/*
 * init_offsets -- initialize offsets[] array depending on the selected mode
 */
static int
init_offsets(struct benchmark_args *args, struct rpmem_bench *mb,
	     enum operation_mode op_mode)
{
	size_t n_ops_by_size = (mb->pool_size - POOL_HDR_SIZE) /
		(args->n_threads * mb->csize_align);

	mb->n_offsets = args->n_ops_per_thread * args->n_threads;
	mb->offsets = (size_t *)malloc(mb->n_offsets * sizeof(*mb->offsets));
	if (!mb->offsets) {
		perror("malloc");
		return -1;
	}

	unsigned seed = args->seed;

	for (size_t i = 0; i < args->n_threads; i++) {
		for (size_t j = 0; j < args->n_ops_per_thread; j++) {
			size_t off_idx = i * args->n_ops_per_thread + j;
			size_t chunk_idx;
			switch (op_mode) {
				case OP_MODE_STAT:
					chunk_idx = i;
					break;
				case OP_MODE_SEQ:
					chunk_idx =
						i * args->n_ops_per_thread + j;
					break;
				case OP_MODE_RAND:
					chunk_idx = i * args->n_ops_per_thread +
						os_rand_r(&seed) %
							args->n_ops_per_thread;
					break;
				case OP_MODE_SEQ_WRAP:
					chunk_idx = i * n_ops_by_size +
						j % n_ops_by_size;
					break;
				case OP_MODE_RAND_WRAP:
					chunk_idx = i * n_ops_by_size +
						os_rand_r(&seed) %
							n_ops_by_size;
					break;
				default:
					assert(0);
					return -1;
			}

			mb->offsets[off_idx] = POOL_HDR_SIZE +
				chunk_idx * mb->csize_align +
				mb->pargs->dest_off;
		}
	}

	return 0;
}

/*
 * do_warmup -- does the warmup by writing the whole pool area
 */
static int
do_warmup(struct rpmem_bench *mb)
{
	/* clear the entire pool */
	memset((char *)mb->pool + POOL_HDR_SIZE, 0,
	       mb->pool_size - POOL_HDR_SIZE);

	for (unsigned r = 0; r < mb->nreplicas; ++r) {
		int ret = rpmem_persist(mb->rpp[r], POOL_HDR_SIZE,
					mb->pool_size - POOL_HDR_SIZE, 0,
					mb->flags);
		if (ret)
			return ret;
	}

	/* if no memset for each operation, do one big memset */
	if (mb->pargs->no_memset) {
		memset((char *)mb->pool + POOL_HDR_SIZE, 0xFF,
		       mb->pool_size - POOL_HDR_SIZE);
	}

	return 0;
}

/*
 * rpmem_op -- actual benchmark operation
 */
static int
rpmem_op(struct benchmark *bench, struct operation_info *info)
{
	auto *mb = (struct rpmem_bench *)pmembench_get_priv(bench);

	uint64_t idx = info->worker->index * info->args->n_ops_per_thread +
		info->index;

	assert(idx < mb->n_offsets);

	size_t offset = mb->offsets[idx];
	size_t len = mb->pargs->chunk_size;

	if (!mb->pargs->no_memset) {
		void *dest = (char *)mb->pool + offset;
		/* thread id on MS 4 bits and operation id on LS 4 bits */
		int c = ((info->worker->index & 0xf) << 4) +
			((0xf & info->index));
		memset(dest, c, len);
	}

	int ret = 0;
	for (unsigned r = 0; r < mb->nreplicas; ++r) {
		assert(info->worker->index < mb->nlanes[r]);

		ret = rpmem_persist(mb->rpp[r], offset, len,
				    info->worker->index, mb->flags);
		if (ret) {
			fprintf(stderr, "rpmem_persist replica #%u: %s\n", r,
				rpmem_errormsg());
			return ret;
		}
	}

	return 0;
}

/*
 * rpmem_map_file -- map local file
 */
static int
rpmem_map_file(const char *path, struct rpmem_bench *mb, size_t size)
{
	int mode;
#ifndef _WIN32
	mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
#else
	mode = S_IWRITE | S_IREAD;
#endif

	mb->addrp = pmem_map_file(path, size, PMEM_FILE_CREATE, mode,
				  &mb->mapped_len, nullptr);

	if (!mb->addrp)
		return -1;

	return 0;
}

/*
 * rpmem_unmap_file -- unmap local file
 */
static int
rpmem_unmap_file(struct rpmem_bench *mb)
{
	return pmem_unmap(mb->addrp, mb->mapped_len);
}

/*
 * rpmem_poolset_init -- read poolset file and initialize benchmark accordingly
 */
static int
rpmem_poolset_init(const char *path, struct rpmem_bench *mb,
		   struct benchmark_args *args)
{
	struct pool_set *set;
	struct pool_replica *rep;
	struct remote_replica *remote;
	struct pool_set_part *part;

	struct rpmem_pool_attr attr;
	memset(&attr, 0, sizeof(attr));
	memcpy(attr.signature, "PMEMBNCH", sizeof(attr.signature));

	/* read and validate poolset */
	if (util_poolset_read(&set, path)) {
		fprintf(stderr, "Invalid poolset file '%s'\n", path);
		return -1;
	}

	assert(set);
	if (set->nreplicas < 2) {
		fprintf(stderr, "No replicas defined\n");
		goto err_poolset_free;
	}

	if (set->remote == 0) {
		fprintf(stderr, "No remote replicas defined\n");
		goto err_poolset_free;
	}

	for (unsigned i = 1; i < set->nreplicas; ++i) {
		if (!set->replica[i]->remote) {
			fprintf(stderr, "Local replicas are not supported\n");
			goto err_poolset_free;
		}
	}

	/* read and validate master replica */
	rep = set->replica[0];

	assert(rep);
	assert(rep->remote == nullptr);
	if (rep->nparts != 1) {
		fprintf(stderr,
			"Multipart master replicas are not supported\n");
		goto err_poolset_free;
	}

	if (rep->repsize < mb->min_size) {
		fprintf(stderr, "A master replica is too small (%zu < %zu)\n",
			rep->repsize, mb->min_size);
		goto err_poolset_free;
	}

	part = (struct pool_set_part *)&rep->part[0];
	if (rpmem_map_file(part->path, mb, rep->repsize)) {
		perror(part->path);
		goto err_poolset_free;
	}

	mb->pool_size = mb->mapped_len;
	mb->pool = (void *)((uintptr_t)mb->addrp);

	/* prepare remote replicas */
	mb->nreplicas = set->nreplicas - 1;
	mb->nlanes = (unsigned *)malloc(mb->nreplicas * sizeof(unsigned));
	if (mb->nlanes == nullptr) {
		perror("malloc");
		goto err_unmap_file;
	}

	mb->rpp = (RPMEMpool **)malloc(mb->nreplicas * sizeof(RPMEMpool *));
	if (mb->rpp == nullptr) {
		perror("malloc");
		goto err_free_lanes;
	}

	unsigned r;
	for (r = 0; r < mb->nreplicas; ++r) {
		remote = set->replica[r + 1]->remote;

		assert(remote);

		mb->nlanes[r] = args->n_threads;
		/* Temporary WA for librpmem issue */
		++mb->nlanes[r];

		mb->rpp[r] = rpmem_create(remote->node_addr, remote->pool_desc,
					  mb->addrp, mb->pool_size,
					  &mb->nlanes[r], &attr);
		if (!mb->rpp[r]) {
			perror("rpmem_create");
			goto err_rpmem_close;
		}

		if (mb->nlanes[r] < args->n_threads) {
			fprintf(stderr,
				"Number of threads too large for replica #%u (max: %u)\n",
				r, mb->nlanes[r]);
			r++; /* close current replica */
			goto err_rpmem_close;
		}
	}

	util_poolset_free(set);
	return 0;

err_rpmem_close:
	for (unsigned i = 0; i < r; i++)
		rpmem_close(mb->rpp[i]);
	free(mb->rpp);

err_free_lanes:
	free(mb->nlanes);

err_unmap_file:
	rpmem_unmap_file(mb);

err_poolset_free:
	util_poolset_free(set);
	return -1;
}

/*
 * rpmem_poolset_fini -- close opened local and remote replicas
 */
static void
rpmem_poolset_fini(struct rpmem_bench *mb)
{
	for (unsigned r = 0; r < mb->nreplicas; ++r) {
		rpmem_close(mb->rpp[r]);
	}

	rpmem_unmap_file(mb);
}

/*
 * rpmem_set_min_size -- compute minimal file size based on benchmark arguments
 */
static void
rpmem_set_min_size(struct rpmem_bench *mb, enum operation_mode op_mode,
		   struct benchmark_args *args)
{
	mb->csize_align = ALIGN_CL(mb->pargs->chunk_size);

	switch (op_mode) {
		case OP_MODE_STAT:
			mb->min_size = mb->csize_align * args->n_threads;
			break;
		case OP_MODE_SEQ:
		case OP_MODE_RAND:
			mb->min_size = mb->csize_align *
				args->n_ops_per_thread * args->n_threads;
			break;
		case OP_MODE_SEQ_WRAP:
		case OP_MODE_RAND_WRAP:
			/*
			 * at least one chunk per thread to avoid false sharing
			 */
			mb->min_size = mb->csize_align * args->n_threads;
			break;
		default:
			assert(0);
	}

	mb->min_size += POOL_HDR_SIZE;
}

/*
 * rpmem_init -- initialization function
 */
static int
rpmem_init(struct benchmark *bench, struct benchmark_args *args)
{
	assert(bench != nullptr);
	assert(args != nullptr);
	assert(args->opts != nullptr);

	auto *mb = (struct rpmem_bench *)malloc(sizeof(struct rpmem_bench));
	if (!mb) {
		perror("malloc");
		return -1;
	}

	mb->flags = 0;
	mb->pargs = (struct rpmem_args *)args->opts;
	mb->pargs->chunk_size = args->dsize;

	if (mb->pargs->relaxed)
		mb->flags |= RPMEM_PERSIST_RELAXED;

	enum operation_mode op_mode = parse_op_mode(mb->pargs->mode);
	if (op_mode == OP_MODE_UNKNOWN) {
		fprintf(stderr, "Invalid operation mode argument '%s'\n",
			mb->pargs->mode);
		goto err_parse_mode;
	}

	rpmem_set_min_size(mb, op_mode, args);

	if (rpmem_poolset_init(args->fname, mb, args)) {
		goto err_poolset_init;
	}

	/* initialize offsets[] array depending on benchmark args */
	if (init_offsets(args, mb, op_mode) < 0) {
		goto err_init_offsets;
	}

	if (!mb->pargs->no_warmup) {
		if (do_warmup(mb) != 0) {
			fprintf(stderr, "do_warmup() function failed.\n");
			goto err_warmup;
		}
	}

	pmembench_set_priv(bench, mb);

	return 0;
err_warmup:
	free(mb->offsets);
err_init_offsets:
	rpmem_poolset_fini(mb);
err_poolset_init:
err_parse_mode:
	free(mb);
	return -1;
}

/*
 * rpmem_exit -- benchmark cleanup function
 */
static int
rpmem_exit(struct benchmark *bench, struct benchmark_args *args)
{
	auto *mb = (struct rpmem_bench *)pmembench_get_priv(bench);
	rpmem_poolset_fini(mb);
	free(mb->offsets);
	free(mb);
	return 0;
}

static struct benchmark_clo rpmem_clo[5];
/* Stores information about benchmark. */
static struct benchmark_info rpmem_info;
CONSTRUCTOR(rpmem_persist_constructor)
void
pmem_rpmem_persist(void)
{

	rpmem_clo[0].opt_short = 'M';
	rpmem_clo[0].opt_long = "mem-mode";
	rpmem_clo[0].descr = "Memory writing mode :"
			     " stat, seq[-wrap], rand[-wrap]";
	rpmem_clo[0].def = "seq";
	rpmem_clo[0].off = clo_field_offset(struct rpmem_args, mode);
	rpmem_clo[0].type = CLO_TYPE_STR;

	rpmem_clo[1].opt_short = 'D';
	rpmem_clo[1].opt_long = "dest-offset";
	rpmem_clo[1].descr = "Destination cache line "
			     "alignment offset";
	rpmem_clo[1].def = "0";
	rpmem_clo[1].off = clo_field_offset(struct rpmem_args, dest_off);
	rpmem_clo[1].type = CLO_TYPE_UINT;
	rpmem_clo[1].type_uint.size =
		clo_field_size(struct rpmem_args, dest_off);
	rpmem_clo[1].type_uint.base = CLO_INT_BASE_DEC;
	rpmem_clo[1].type_uint.min = 0;
	rpmem_clo[1].type_uint.max = MAX_OFFSET;

	rpmem_clo[2].opt_short = 'w';
	rpmem_clo[2].opt_long = "no-warmup";
	rpmem_clo[2].descr = "Don't do warmup";
	rpmem_clo[2].def = "false";
	rpmem_clo[2].type = CLO_TYPE_FLAG;
	rpmem_clo[2].off = clo_field_offset(struct rpmem_args, no_warmup);

	rpmem_clo[3].opt_short = 'T';
	rpmem_clo[3].opt_long = "no-memset";
	rpmem_clo[3].descr = "Don't call memset for all rpmem_persist";
	rpmem_clo[3].def = "false";
	rpmem_clo[3].off = clo_field_offset(struct rpmem_args, no_memset);
	rpmem_clo[3].type = CLO_TYPE_FLAG;

	rpmem_clo[4].opt_short = 0;
	rpmem_clo[4].opt_long = "persist-relaxed";
	rpmem_clo[4].descr = "Use RPMEM_PERSIST_RELAXED flag";
	rpmem_clo[4].def = "false";
	rpmem_clo[4].off = clo_field_offset(struct rpmem_args, relaxed);
	rpmem_clo[4].type = CLO_TYPE_FLAG;

	rpmem_info.name = "rpmem_persist";
	rpmem_info.brief = "Benchmark for rpmem_persist() "
			   "operation";
	rpmem_info.init = rpmem_init;
	rpmem_info.exit = rpmem_exit;
	rpmem_info.multithread = true;
	rpmem_info.multiops = true;
	rpmem_info.operation = rpmem_op;
	rpmem_info.measure_time = true;
	rpmem_info.clos = rpmem_clo;
	rpmem_info.nclos = ARRAY_SIZE(rpmem_clo);
	rpmem_info.opts_size = sizeof(struct rpmem_args);
	rpmem_info.rm_file = true;
	rpmem_info.allow_poolset = true;
	rpmem_info.print_bandwidth = true;
	REGISTER_BENCHMARK(rpmem_info);
};
