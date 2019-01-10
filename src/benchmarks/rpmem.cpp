/*
 * Copyright 2016-2019, Intel Corporation
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
 * rpmem.cpp -- rpmem benchmarks definition
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
	char *mode;		    /* op mode: stat, seq[-wrap], rand[-wrap] */
	bool no_warmup;		    /* do not do warmup */
	bool no_memset;		    /* do not call memset before each op */
	size_t dest_off;	    /* destination address offset */
	unsigned flushes_per_drain; /* # of flushes per drain */
	bool relaxed;		    /* use RPMEM_PERSIST_RELAXED flag */
};

/*
 * rpmem_bench -- benchmark context
 */
struct rpmem_bench {
	struct rpmem_args *pargs; /* benchmark specific arguments */

	size_t chunk_size;	  /* data size aligned to CL */
	size_t *offsets;	  /* random/sequential address offsets */
	size_t n_offsets;	  /* number of offsets */

	void *pool;		  /* mapped file address */
	size_t pool_size;	  /* mapped length */

	RPMEMpool **rpp;	  /* rpmem pool pointers */
	unsigned *nlanes;	  /* number of lanes for each remote replica */
	unsigned nreplicas;	  /* number of remote replicas */

	unsigned flags;		  /* flags for rpmem_persist */
};

/*
 * rpmem_flush_drain_op -- actual benchmark operation
 */
static int
rpmem_flush_drain_op(struct benchmark *bench, struct operation_info *info)
{
	auto *mb = (struct rpmem_bench *)pmembench_get_priv(bench);

	uint64_t idx = info->worker->index * info->args->n_ops_per_thread +
		info->index;

	assert(idx < mb->n_offsets);

	size_t offset = mb->offsets[idx];
	size_t len = mb->chunk_size;

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

		ret = rpmem_flush(mb->rpp[r], offset, len, info->worker->index,
				  mb->flags);
		if (unlikely(ret)) {
			fprintf(stderr, "rpmem_persist replica #%u: %s\n", r,
				rpmem_errormsg());
			return ret;
		}
	}

	/* perform rpmem_drain after a certain # of flushes */
	if (mb->pargs->flushes_per_drain == 0 ||
			(info->index + 1) % mb->pargs->flushes_per_drain)
		return 0;

	for (unsigned r = 0; r < mb->nreplicas; ++r) {
		ret = rpmem_drain(mb->rpp[r], info->worker->index, 0);

		if (unlikely(ret)) {
			fprintf(stderr, "rpmem_drain replica #%u: %s\n", r,
				rpmem_errormsg());
			return ret;
		}
	}
	return 0;
}

/*
 * rpmem_drain_op -- actual benchmark operation
 */
static int
rpmem_drain_op(struct benchmark *bench, struct operation_info *info)
{
	auto *mb = (struct rpmem_bench *)pmembench_get_priv(bench);
	int ret;

	for (unsigned r = 0; r < mb->nreplicas; ++r) {
		ret = rpmem_drain(mb->rpp[r], info->worker->index, 0);

		if (unlikely(ret)) {
			fprintf(stderr, "rpmem_drain replica #%u: %s\n", r,
				rpmem_errormsg());
			return ret;
		}
	}
	return 0;
}

/*
 * rpmem_persist_op -- actual benchmark operation
 */
static int
rpmem_persist_op(struct benchmark *bench, struct operation_info *info)
{
	auto *mb = (struct rpmem_bench *)pmembench_get_priv(bench);

	uint64_t idx = info->worker->index * info->args->n_ops_per_thread +
		info->index;

	assert(idx < mb->n_offsets);

	size_t offset = mb->offsets[idx];
	size_t len = mb->chunk_size;

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
 * op_mode -- mode of operation
 */
enum op_mode {
	OP_MODE_UNKNOWN,
	OP_MODE_STAT,      /* always use the same chunk */
	OP_MODE_SEQ,       /* use consecutive chunks */
	OP_MODE_RAND,      /* use random chunks */
	OP_MODE_SEQ_WRAP,  /* use consecutive chunks, but use file size */
	OP_MODE_RAND_WRAP, /* use random chunks, but use file size */
};

/*
 * init_args -- init time values
 */
struct init_args {
	enum op_mode op_mode;
	size_t min_size;	 /* min pool size */
	size_t dest_off;	 /* rpmem_args.dest_off + POOL_HDR_SIZE */

	/* copied from struct benchmark_args */
	unsigned seed;		 /* PRNG seed */
	unsigned n_threads;	 /* number of working threads */
	size_t n_ops_per_thread; /* number of operations per thread */
};

/*
 * do_warmup -- does the warmup by writing the whole pool area
 */
static int
do_warmup(struct init_args init, struct rpmem_bench *mb)
{
	if (mb->pargs->no_warmup)
		return 0;

	size_t len = mb->pool_size - init.dest_off;

	/* clear the entire pool */
	memset((char *)mb->pool + init.dest_off, 0, len);

	for (unsigned r = 0; r < mb->nreplicas; ++r) {
		int ret = rpmem_persist(mb->rpp[r], init.dest_off, len, 0,
					mb->flags);
		if (ret)
			return ret;
	}

	/* if no memset for each operation, do one big memset */
	if (mb->pargs->no_memset) {
		memset((char *)mb->pool + init.dest_off, 0xFF, len);
	}

	return 0;
}


/*
 * parse_op_mode -- parse operation mode from string
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
	else if (strcmp(arg, "seq-wrap") == 0)
		return OP_MODE_SEQ_WRAP;
	else if (strcmp(arg, "rand-wrap") == 0)
		return OP_MODE_RAND_WRAP;
	else
		return OP_MODE_UNKNOWN;
}

/*
 * get_min_size -- compute minimal file size based on benchmark arguments
 */
static size_t
get_min_size(size_t chunk_size, struct init_args init)
{
	size_t min_size = 0;

	switch (init.op_mode) {
		case OP_MODE_STAT:
			min_size = chunk_size * init.n_threads;
			break;
		case OP_MODE_SEQ:
		case OP_MODE_RAND:
			min_size = chunk_size *
					init.n_ops_per_thread * init.n_threads;
			break;
		case OP_MODE_SEQ_WRAP:
		case OP_MODE_RAND_WRAP:
			/*
			 * at least one chunk per thread to avoid false sharing
			 */
			min_size = chunk_size * init.n_threads;
			break;
		default:
			assert(0);
	}

	return min_size + init.dest_off;
}

/*
 * offsets_init -- initialize offsets[] array depending on the selected mode
 */
static int
offsets_init(struct init_args init, struct rpmem_bench *mb)
{
	size_t n_ops_by_size = (mb->pool_size - init.dest_off) /
		(init.n_threads * mb->chunk_size);

	mb->n_offsets = init.n_ops_per_thread * init.n_threads;
	mb->offsets = (size_t *)malloc(mb->n_offsets * sizeof(*mb->offsets));
	if (!mb->offsets) {
		perror("malloc");
		return -1;
	}

	unsigned seed = init.seed;

	for (size_t i = 0; i < init.n_threads; i++) {
		for (size_t j = 0; j < init.n_ops_per_thread; j++) {
			size_t off_idx = i * init.n_ops_per_thread + j;
			size_t chunk_idx;
			switch (init.op_mode) {
				case OP_MODE_STAT:
					chunk_idx = i;
					break;
				case OP_MODE_SEQ:
					chunk_idx =
						i * init.n_ops_per_thread + j;
					break;
				case OP_MODE_RAND:
					chunk_idx = i * init.n_ops_per_thread +
						os_rand_r(&seed) %
							init.n_ops_per_thread;
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
					return 1;
			}

			mb->offsets[off_idx] = chunk_idx * mb->chunk_size +
					init.dest_off;
		}
	}
	return 0;
}

/*
 * offsets_fini -- release offsets[] array
 */
static void
offsets_fini(struct rpmem_bench *mb)
{
	free(mb->offsets);
}

/*
 * file_map -- map local file
 */
static int
file_map(const char *path, size_t size, struct rpmem_bench *mb)
{
	int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	mb->pool = pmem_map_file(path, size, PMEM_FILE_CREATE, mode,
				  &mb->pool_size, nullptr);

	if (!mb->pool)
		return 1;

	return 0;
}

/*
 * file_unmap -- unmap local file
 */
static int
file_unmap(struct rpmem_bench *mb)
{
	return pmem_unmap(mb->pool, mb->pool_size);
}

/*
 * rep_local_init -- initialize local replica
 */
static int
rep_local_init(struct pool_replica *rep, struct rpmem_bench *mb)
{
	assert(rep->nparts == 1);
	struct pool_set_part *part = (struct pool_set_part *)&rep->part[0];
	if (file_map(part->path, rep->repsize, mb)) {
		perror(part->path);
		return 1;
	}
	return 0;
}

/*
 * rep_local_fini -- release local replica
 */
static void
rep_local_fini(struct rpmem_bench *mb)
{
	file_unmap(mb);
}

/*
 * rep_remote_init -- initialize remote replicas
 */
static int
rep_remote_init(struct pool_set *set, unsigned n_threads, struct rpmem_bench *mb)
{
	struct remote_replica *remote;
	unsigned r;

	mb->nreplicas = set->nreplicas - 1;
	mb->nlanes = (unsigned *)malloc(mb->nreplicas * sizeof(unsigned));
	if (mb->nlanes == nullptr) {
		perror("malloc");
		return 1;
	}

	mb->rpp = (RPMEMpool **)malloc(mb->nreplicas * sizeof(RPMEMpool *));
	if (mb->rpp == nullptr) {
		perror("malloc");
		goto err_nlanes_free;
	}

	/* prepare pool signature */
	struct rpmem_pool_attr attr;
	memset(&attr, 0, sizeof(attr));
	memcpy(attr.signature, "PMEMBNCH", sizeof(attr.signature));

	/* open replicas */
	for (r = 0; r < mb->nreplicas; ++r) {
		remote = set->replica[r + 1]->remote;
		assert(remote);

		mb->nlanes[r] = n_threads;
		/* XXX temporary WA for librpmem issue */
		++mb->nlanes[r];

		mb->rpp[r] = rpmem_create(remote->node_addr, remote->pool_desc,
					  mb->pool, mb->pool_size,
					  &mb->nlanes[r], &attr);
		if (!mb->rpp[r]) {
			perror("rpmem_create");
			goto err_common_close;
		}

		if (mb->nlanes[r] < n_threads) {
			fprintf(stderr,
				"Number of threads too large for replica #%u (max: %u)\n",
				r, mb->nlanes[r]);
			r++; /* close current replica */
			goto err_common_close;
		}
	}
	return 0;

err_common_close:
	for (unsigned i = 0; i < r; i++)
		rpmem_close(mb->rpp[i]);
	free(mb->rpp);
err_nlanes_free:
	free(mb->nlanes);
	return 1;
}

/*
 * rep_remote_fini -- close and release remote replicas
 */
static void
rep_remote_fini(struct rpmem_bench *mb)
{
	for (unsigned i = 0; i < mb->nreplicas; i++)
		rpmem_close(mb->rpp[i]);

	free(mb->rpp);
	free(mb->nlanes);
}

/*
 * get_master_replica -- get master replica from poolset
 */
static inline struct pool_replica*
get_master_replica(struct pool_set *set)
{
	/* master replica is the first one */
	return set->replica[0];
}

/*
 * poolset_init -- read and validate poolset
 */
static struct pool_set *
poolset_init(const char *path, size_t min_size)
{
	struct pool_set *set;
	struct pool_replica *rep;

	if (util_poolset_read(&set, path)) {
		fprintf(stderr, "Invalid poolset file '%s'\n", path);
		return NULL;
	}
	assert(set);

	if (set->nreplicas < 2) {
		fprintf(stderr, "No replicas defined\n");
		goto err;
	}

	if (set->remote == 0) {
		fprintf(stderr, "No remote replicas defined\n");
		goto err;
	}

	for (unsigned i = 1; i < set->nreplicas; ++i) {
		if (!set->replica[i]->remote) {
			fprintf(stderr, "Local replicas are not supported\n");
			goto err;
		}
	}

	/* validate master replica */
	rep = get_master_replica(set);

	assert(rep);
	assert(rep->remote == nullptr);
	if (rep->nparts != 1) {
		fprintf(stderr,
			"Multipart master replicas are not supported\n");
		goto err;
	}

	if (rep->repsize < min_size) {
		fprintf(stderr, "A master replica is too small (%zu < %zu)\n",
			rep->repsize, min_size);
		goto err;
	}
	return set;

err:
	util_poolset_free(set);
	return NULL;
}

/*
 * poolset_fini -- release poolset
 */
static void
poolset_fini(struct pool_set *set)
{
	util_poolset_free(set);
}

/*
 * poolset_init -- prepare poolset
 */
static int
pool_init(const char *path, struct init_args init, struct rpmem_bench *mb)
{
	struct pool_set *set = poolset_init(path, init.min_size);

	struct pool_replica *local = get_master_replica(set);
	if (rep_local_init(local, mb))
		goto err_poolset_fini;

	if (rep_remote_init(set, init.n_threads /* req lanes num */, mb))
		goto err_local_fini;

	poolset_fini(set);
	return 0;

err_local_fini:
	rep_local_fini(mb);
err_poolset_fini:
	poolset_fini(set);
	return 1;
}

/*
 * pool_fini -- close opened local and remote replicas
 */
static void
pool_fini(struct rpmem_bench *mb)
{
	rep_remote_fini(mb);
	rep_local_fini(mb);
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
	mb->pargs = (struct rpmem_args *)args->opts;
	struct init_args init;
	memset(&init, 0, sizeof(init));
	init.dest_off = mb->pargs->dest_off + POOL_HDR_SIZE;
	init.seed = args->seed;
	init.n_threads = args->n_threads;
	init.n_ops_per_thread = args->n_ops_per_thread;

	/* calculate flags */
	mb->flags = 0;
	if (mb->pargs->relaxed)
		mb->flags |= RPMEM_PERSIST_RELAXED;

	/* parse operation mode */
	init.op_mode = parse_op_mode(mb->pargs->mode);
	if (init.op_mode == OP_MODE_UNKNOWN) {
		fprintf(stderr, "Invalid operation mode argument '%s'\n",
			mb->pargs->mode);
		goto err_parse_mode;
	}

	/* calculate a chunk size and a minimal required pool size */
	mb->chunk_size = ALIGN_CL(args->dsize);
	init.min_size = get_min_size(mb->chunk_size, init);

	if (pool_init(args->fname, init, mb)) {
		goto err_parse_mode;
	}

	/* initialize offsets[] array depending on benchmark args */
	if (offsets_init(init, mb) < 0) {
		goto err_pool_fini;
	}

	/* perform warmup */
	if (do_warmup(init, mb) != 0) {
		fprintf(stderr, "do_warmup() function failed.\n");
		goto err_offsets_fini;
	}

	pmembench_set_priv(bench, mb);

	return 0;

err_offsets_fini:
	offsets_fini(mb);
err_pool_fini:
	pool_fini(mb);
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
	offsets_fini(mb);
	pool_fini(mb);
	free(mb);
	return 0;
}

static struct benchmark_clo common_clo[4];
static struct benchmark_clo flush_clo[5];
static struct benchmark_clo drain_clo[4];
static struct benchmark_clo persist_clo[5];
/* Stores information about benchmark. */
static struct benchmark_info flush_info;
static struct benchmark_info drain_info;
static struct benchmark_info persist_info;
CONSTRUCTOR(rpmem_constructor)
void
rpmem_constructor(void)
{
	common_clo[0].opt_short = 'M';
	common_clo[0].opt_long = "mem-mode";
	common_clo[0].descr = "Memory writing mode :"
			     " stat, seq[-wrap], rand[-wrap]";
	common_clo[0].def = "seq";
	common_clo[0].off = clo_field_offset(struct rpmem_args, mode);
	common_clo[0].type = CLO_TYPE_STR;

	common_clo[1].opt_short = 'D';
	common_clo[1].opt_long = "dest-offset";
	common_clo[1].descr = "Destination cache line "
			     "alignment offset";
	common_clo[1].def = "0";
	common_clo[1].off = clo_field_offset(struct rpmem_args, dest_off);
	common_clo[1].type = CLO_TYPE_UINT;
	common_clo[1].type_uint.size =
		clo_field_size(struct rpmem_args, dest_off);
	common_clo[1].type_uint.base = CLO_INT_BASE_DEC;
	common_clo[1].type_uint.min = 0;
	common_clo[1].type_uint.max = MAX_OFFSET;

	common_clo[2].opt_short = 'w';
	common_clo[2].opt_long = "no-warmup";
	common_clo[2].descr = "Don't do warmup";
	common_clo[2].def = "false";
	common_clo[2].type = CLO_TYPE_FLAG;
	common_clo[2].off = clo_field_offset(struct rpmem_args, no_warmup);

	common_clo[3].opt_short = 0;
	common_clo[3].opt_long = "no-memset";
	common_clo[3].descr = "Don't call memset for all rpmem_persist";
	common_clo[3].def = "false";
	common_clo[3].off = clo_field_offset(struct rpmem_args, no_memset);
	common_clo[3].type = CLO_TYPE_FLAG;

	memcpy(persist_clo, common_clo, sizeof(common_clo));

	persist_clo[4].opt_short = 0;
	persist_clo[4].opt_long = "persist-relaxed";
	persist_clo[4].descr = "Use RPMEM_PERSIST_RELAXED flag";
	persist_clo[4].def = "false";
	persist_clo[4].off = clo_field_offset(struct rpmem_args, relaxed);
	persist_clo[4].type = CLO_TYPE_FLAG;

	persist_info.name = "rpmem_persist";
	persist_info.brief = "Benchmark for rpmem_persist() operation";
	persist_info.init = rpmem_init;
	persist_info.exit = rpmem_exit;
	persist_info.multithread = true;
	persist_info.multiops = true;
	persist_info.operation = rpmem_persist_op;
	persist_info.measure_time = true;
	persist_info.clos = persist_clo;
	persist_info.nclos = ARRAY_SIZE(persist_clo);
	persist_info.opts_size = sizeof(struct rpmem_args);
	persist_info.rm_file = true;
	persist_info.allow_poolset = true;
	persist_info.print_bandwidth = true;
	REGISTER_BENCHMARK(persist_info);

	memcpy(flush_clo, common_clo, sizeof(common_clo));

	flush_clo[4].opt_short = 0;
	flush_clo[4].opt_long = "flushes-per-drain";
	flush_clo[4].descr = "Number of rpmem_flush() before rpmem_drain()";
	flush_clo[4].def = "1";
	flush_clo[4].off = clo_field_offset(struct rpmem_args, flushes_per_drain);
	flush_clo[4].type = CLO_TYPE_UINT;
	flush_clo[4].type_uint.size =
		clo_field_size(struct rpmem_args, flushes_per_drain);
	flush_clo[4].type_uint.base = CLO_INT_BASE_DEC;
	flush_clo[4].type_uint.min = 0;
	flush_clo[4].type_uint.max = UINT_MAX;

	memcpy(&flush_info, &persist_info, sizeof(flush_info));
	flush_info.name = "rpmem_flush";
	flush_info.brief =
		"Benchmark for rpmem_flush() and rpmem_drain() operation";
	flush_info.operation = rpmem_flush_drain_op;
	flush_info.clos = flush_clo;
	flush_info.nclos = ARRAY_SIZE(flush_clo);
	REGISTER_BENCHMARK(flush_info);

	memcpy(drain_clo, common_clo, sizeof(common_clo));

	memcpy(&drain_info, &persist_info, sizeof(drain_info));
	drain_info.name = "rpmem_drain";
	drain_info.brief =
		"Benchmark for rpmem_drain()";
	drain_info.operation = rpmem_drain_op;
	drain_info.clos = drain_clo;
	drain_info.nclos = ARRAY_SIZE(drain_clo);
	REGISTER_BENCHMARK(drain_info);
};
