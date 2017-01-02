/*
 * Copyright 2016-2017, Intel Corporation
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
#include "set.h"
#include "util.h"

#define MAX_OFFSET 63
#define CONST_B 0xFF

/*
 * rpmem_args -- benchmark specific command line options
 */
struct rpmem_args {
	char *mode;	/* operation mode: stat, seq, rand */
	bool no_warmup;    /* do not do warmup */
	size_t chunk_size; /* elementary chunk size */
	size_t dest_off;   /* destination address offset */
};

/*
 * rpmem_bench -- benchmark context
 */
struct rpmem_bench {
	struct rpmem_args *pargs; /* benchmark specific arguments */
	uint64_t *offsets;	/* random/sequential address offsets */
	unsigned n_offsets;       /* number of random elements */
	int const_b;		  /* memset() value */
	size_t fsize;		  /* file size */
	void *addrp;		  /* mapped file address */
	size_t mapped_len;	/* mapped length */
	RPMEMpool **rpp;	  /* rpmem pool pointers */
	unsigned *nlanes;	 /* number of lanes for each remote replica */
	unsigned nreplicas;       /* number of remote replicas */
};

/*
 * operation_mode -- mode of operation
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
init_offsets(struct benchmark_args *args, struct rpmem_bench *mb,
	     enum operation_mode op_mode)
{
	uint64_t n_threads = args->n_threads;
	uint64_t n_ops = args->n_ops_per_thread;

	mb->n_offsets = n_ops * n_threads;
	mb->offsets = (uint64_t *)malloc(mb->n_offsets * sizeof(*mb->offsets));
	if (!mb->offsets) {
		perror("malloc");
		return -1;
	}

	unsigned seed = args->seed;

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
do_warmup(struct rpmem_bench *mb, size_t nops)
{
	size_t len = mb->fsize;

	memset(mb->addrp, 0, len);
	for (unsigned r = 0; r < mb->nreplicas; ++r) {
		int ret = rpmem_persist(mb->rpp[r], (size_t)0, len, 0);
		if (ret)
			return ret;
	}

	return 0;
}

/*
 * rpmem_op -- actual benchmark operation
 */
static int
rpmem_op(struct benchmark *bench, struct operation_info *info)
{
	struct rpmem_bench *mb =
		(struct rpmem_bench *)pmembench_get_priv(bench);

	assert(info->index < mb->n_offsets);

	uint64_t idx = info->worker->index * info->args->n_ops_per_thread +
		info->index;
	size_t offset = mb->offsets[idx] + mb->pargs->dest_off;
	void *dest = (char *)mb->addrp + offset;
	int c = mb->const_b;
	size_t len = mb->pargs->chunk_size;

	int ret = 0;
	memset(dest, c, len);
	for (unsigned r = 0; r < mb->nreplicas; ++r) {
		unsigned lane = info->worker->index % mb->nlanes[r];
		ret = rpmem_persist(mb->rpp[r], offset, len, lane);
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
rpmem_map_file(const char *path, struct rpmem_bench *mb)
{
	int mode;
#ifndef _WIN32
	mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
#else
	mode = S_IWRITE | S_IREAD;
#endif

	mb->addrp = pmem_map_file(path, mb->fsize, PMEM_FILE_CREATE, mode,
				  &mb->mapped_len, NULL);

	if (!mb->addrp)
		return -1;
	else
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
	unsigned r;

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

	for (r = 1; r < set->nreplicas; ++r) {
		if (!set->replica[r]->remote) {
			fprintf(stderr, "Local replicas are not supported\n");
			goto err_poolset_free;
		}
	}

	/* read and validate master replica */
	rep = set->replica[0];

	assert(rep);
	assert(rep->remote == NULL);
	if (rep->nparts != 1) {
		fprintf(stderr, "Multipart master replicas "
				"are not supported\n");
		goto err_poolset_free;
	}

	if (rep->repsize < mb->fsize) {
		fprintf(stderr, "A master replica is too small (%zu < %zu)\n",
			rep->repsize, mb->fsize);
		goto err_poolset_free;
	}

	part = (struct pool_set_part *)&rep->part[0];
	if (rpmem_map_file(part->path, mb)) {
		perror(part->path);
		goto err_poolset_free;
	}

	/* prepare remote replicas */
	mb->nreplicas = set->nreplicas - 1;
	mb->nlanes = (unsigned *)malloc(mb->nreplicas * sizeof(unsigned));
	if (mb->nlanes == NULL) {
		perror("malloc");
		goto err_unmap_file;
	}
	mb->rpp = (RPMEMpool **)malloc(mb->nreplicas * sizeof(RPMEMpool *));
	if (mb->rpp == NULL) {
		perror("malloc");
		goto err_free_lanes;
	}

	struct rpmem_pool_attr attr;
	memset(&attr, 0, sizeof(attr));

	for (r = 0; r < mb->nreplicas; ++r) {
		remote = set->replica[r + 1]->remote;

		assert(remote);

		mb->nlanes[r] = args->n_threads;
		/* Temporary WA for librpmem issue */
		++mb->nlanes[r];

		mb->rpp[r] = rpmem_create(remote->node_addr, remote->pool_desc,
					  mb->addrp, mb->fsize, &mb->nlanes[r],
					  &attr);
		if (!mb->rpp[r]) {
			perror("rpmem_create");
			goto err_rpmem_close;
		}
	}

	util_poolset_free(set);
	return 0;

err_rpmem_close:
	for (r = 0; mb->rpp[r]; ++r) {
		rpmem_close(mb->rpp[r]);
	}
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
 * rpmem_init -- initialization function
 */
static int
rpmem_init(struct benchmark *bench, struct benchmark_args *args)
{
	assert(bench != NULL);
	assert(args != NULL);
	assert(args->opts != NULL);
	size_t size;
	size_t large;
	size_t small;
	struct rpmem_bench *mb =
		(struct rpmem_bench *)malloc(sizeof(struct rpmem_bench));

	if (!mb) {
		perror("malloc");
		return -1;
	}

	mb->pargs = (rpmem_args *)args->opts;
	mb->pargs->chunk_size = args->dsize;

	enum operation_mode op_mode = parse_op_mode(mb->pargs->mode);
	if (op_mode == OP_MODE_UNKNOWN) {
		fprintf(stderr, "Invalid operation mode argument '%s'\n",
			mb->pargs->mode);
		goto err_free_mb;
	}

	size = MAX_OFFSET + mb->pargs->chunk_size;
	large = size * args->n_ops_per_thread * args->n_threads;
	small = size * args->n_threads;
	mb->fsize = (op_mode == OP_MODE_STAT) ? small : large;
	mb->fsize = PAGE_ALIGNED_UP_SIZE(mb->fsize);

	/* initialize offsets[] array depending on benchmark args */
	if (init_offsets(args, mb, op_mode) < 0) {
		goto err_free_mb;
	}

	/* initialize value */
	mb->const_b = CONST_B;

	if (rpmem_poolset_init(args->fname, mb, args)) {
		goto err_free_offsets;
	}

	if (!mb->pargs->no_warmup) {
		if (do_warmup(mb, args->n_threads * args->n_ops_per_thread) !=
		    0) {
			fprintf(stderr, "do_warmup() function failed.\n");
			goto err_poolset_fini;
		}
	}

	pmembench_set_priv(bench, mb);

	return 0;

err_poolset_fini:
	rpmem_poolset_fini(mb);

err_free_offsets:
	free(mb->offsets);

err_free_mb:
	free(mb);
	return -1;
}

/*
 * memset_exit -- benchmark cleanup function
 */
static int
rpmem_exit(struct benchmark *bench, struct benchmark_args *args)
{
	struct rpmem_bench *mb =
		(struct rpmem_bench *)pmembench_get_priv(bench);
	rpmem_poolset_fini(mb);
	free(mb->offsets);
	free(mb);
	return 0;
}

static struct benchmark_clo rpmem_clo[3];
/* Stores information about benchmark. */
static struct benchmark_info rpmem_info;
CONSTRUCTOR(rpmem_persist_costructor)
void
pmem_rpmem_persist(void)
{

	rpmem_clo[0].opt_short = 'M';
	rpmem_clo[0].opt_long = "mem-mode";
	rpmem_clo[0].descr = "Memory writing mode - "
			     "stat, seq, rand";
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
	REGISTER_BENCHMARK(rpmem_info);
};
