// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2020, Intel Corporation */

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

#define BENCH_RPMEM_FLUSH_NAME "rpmem_flush_drain"
#define BENCH_RPMEM_PERSIST_NAME "rpmem_persist"
#define BENCH_RPMEM_MIXED_NAME "rpmem_mixed"

/*
 * rpmem_args -- benchmark specific command line options
 */
struct rpmem_args {
	char *mode;	/* operation mode: stat, seq, rand */
	bool no_warmup;    /* do not do warmup */
	bool no_memset;    /* do not call memset before each persist */
	size_t chunk_size; /* elementary chunk size */
	size_t dest_off;   /* destination address offset */
	bool relaxed; /* use RPMEM_PERSIST_RELAXED / RPMEM_FLUSH_RELAXED flag */
	char *workload;	/* workload */
	int flushes_per_drain; /* # of flushes between drains */
};

/*
 * rpmem_bench -- benchmark context
 */
struct rpmem_bench {
	struct rpmem_args *pargs; /* benchmark specific arguments */
	size_t *offsets;	  /* random/sequential address offsets */
	size_t n_offsets;	 /* number of random elements */
	size_t *offsets_pos;      /* position within offsets */
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
	unsigned *flags;	  /* flags for ops */
	size_t workload_len;      /* length of the workload */
	unsigned n_flushing_ops_per_thread; /* # of operation which require
					    offsets per thread */
};

/*
 * operation_mode -- mode of operation
 */
enum operation_mode {
	OP_MODE_UNKNOWN,
	OP_MODE_STAT,      /* always use the same chunk */
	OP_MODE_SEQ,       /* use consecutive chunks */
	OP_MODE_RAND,      /* use random chunks */
	OP_MODE_SEQ_WRAP,  /* use consecutive chunks, but use file size */
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
 * get_flushing_op_num -- return # of operations in the workload which require
 * offsets
 */
static unsigned
get_flushing_op_num(struct benchmark *bench, struct rpmem_bench *mb)
{
	assert(bench);
	struct benchmark_info *info = pmembench_get_info(bench);
	assert(info);

	/*
	 * The rpmem_persist benchmark does one rpmem_persist() per worker op.
	 * The rpmem_flush_drain benchmark does one rpmem_flush() or
	 * rpmem_flush() + rpmem_drain() per worker op. Either way, it
	 * requires one offset per worker op.
	 */
	if (strcmp(info->name, BENCH_RPMEM_PERSIST_NAME) == 0 ||
	    strcmp(info->name, BENCH_RPMEM_FLUSH_NAME) == 0)
		return 1;

	assert(strcmp(info->name, BENCH_RPMEM_MIXED_NAME) == 0);
	assert(mb);
	assert(mb->pargs);
	assert(mb->pargs->workload);
	assert(mb->workload_len > 0);
	unsigned num = 0;

	/*
	 * The rpmem_mixed benchmark performs multiple API calls per worker
	 * op some of them flushes ergo requires its own offset.
	 */
	for (size_t i = 0; i < mb->workload_len; ++i) {
		switch (mb->pargs->workload[i]) {
			case 'f': /* rpmem_flush */
			case 'g': /* rpmem_flush + RPMEM_FLUSH_RELAXED */
			case 'p': /* rpmem_persist */
			case 'r': /* rpmem_persist + RPMEM_PERSIST_RELAXED */
				++num;
				break;
		}
	}

	/*
	 * To simplify checks it is assumed each worker op requires at least one
	 * flushing operation even though it doesn't have to use it.
	 */
	if (num < 1)
		num = 1;
	return num;
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

	mb->n_offsets = mb->n_flushing_ops_per_thread * args->n_threads;
	mb->offsets = (size_t *)malloc(mb->n_offsets * sizeof(*mb->offsets));
	if (!mb->offsets) {
		perror("malloc");
		return -1;
	}
	mb->offsets_pos = (size_t *)calloc(args->n_threads, sizeof(size_t));
	if (!mb->offsets_pos) {
		perror("calloc");
		free(mb->offsets);
		return -1;
	}

	rng_t rng;
	randomize_r(&rng, args->seed);

	for (size_t i = 0; i < args->n_threads; i++) {
		for (size_t j = 0; j < mb->n_flushing_ops_per_thread; j++) {
			size_t off_idx = i * mb->n_flushing_ops_per_thread + j;
			size_t chunk_idx;
			switch (op_mode) {
				case OP_MODE_STAT:
					chunk_idx = i;
					break;
				case OP_MODE_SEQ:
					chunk_idx =
						i * mb->n_flushing_ops_per_thread +
						j;
					break;
				case OP_MODE_RAND:
					chunk_idx =
						i * mb->n_flushing_ops_per_thread +
						rnd64_r(&rng) %
							mb->n_flushing_ops_per_thread;
					break;
				case OP_MODE_SEQ_WRAP:
					chunk_idx = i * n_ops_by_size +
						j % n_ops_by_size;
					break;
				case OP_MODE_RAND_WRAP:
					chunk_idx = i * n_ops_by_size +
						rnd64_r(&rng) % n_ops_by_size;
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
					RPMEM_PERSIST_RELAXED);
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
 * rpmem_mixed_op_flush -- perform rpmem_flush
 */
static inline int
rpmem_mixed_op_flush(struct rpmem_bench *mb, struct operation_info *info)
{
	size_t *pos = &mb->offsets_pos[info->worker->index];
	uint64_t idx =
		info->worker->index * mb->n_flushing_ops_per_thread + *pos;

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

		ret = rpmem_flush(mb->rpp[r], offset, len, info->worker->index,
				  mb->flags[info->worker->index]);
		if (ret) {
			fprintf(stderr, "rpmem_flush replica #%u: %s\n", r,
				rpmem_errormsg());
			return ret;
		}
	}

	++*pos;
	return 0;
}

/*
 * rpmem_mixed_op_drain -- perform rpmem_drain
 */
static inline int
rpmem_mixed_op_drain(struct rpmem_bench *mb, struct operation_info *info)
{
	int ret = 0;
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
 * rpmem_flush_drain_op -- actual benchmark operation for the rpmem_flush_drain
 * benchmark
 */
static int
rpmem_flush_drain_op(struct benchmark *bench, struct operation_info *info)
{
	auto *mb = (struct rpmem_bench *)pmembench_get_priv(bench);
	int ret = 0;

	if (mb->pargs->flushes_per_drain != 0) {
		ret |= rpmem_mixed_op_flush(mb, info);

		/* no rpmem_drain() required */
		if (mb->pargs->flushes_per_drain < 0)
			return ret;

		/* more rpmem_flush() required before rpmem_drain()  */
		if ((info->index + 1) % mb->pargs->flushes_per_drain != 0)
			return ret;

		/* rpmem_drain() required */
	}

	ret |= rpmem_mixed_op_drain(mb, info);
	return ret;
}

/*
 * rpmem_persist_op -- actual benchmark operation for the rpmem_persist
 * benchmark
 */
static int
rpmem_persist_op(struct benchmark *bench, struct operation_info *info)
{
	auto *mb = (struct rpmem_bench *)pmembench_get_priv(bench);
	size_t *pos = &mb->offsets_pos[info->worker->index];
	uint64_t idx =
		info->worker->index * mb->n_flushing_ops_per_thread + *pos;

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
				    info->worker->index,
				    mb->flags[info->worker->index]);
		if (ret) {
			fprintf(stderr, "rpmem_persist replica #%u: %s\n", r,
				rpmem_errormsg());
			return ret;
		}
	}

	++*pos;
	return 0;
}

/*
 * rpmem_mixed_op -- actual benchmark operation for the rpmem_mixed
 * benchmark
 */
static int
rpmem_mixed_op(struct benchmark *bench, struct operation_info *info)
{
	auto *mb = (struct rpmem_bench *)pmembench_get_priv(bench);
	assert(mb->workload_len != 0);
	int ret = 0;

	for (size_t i = 0; i < mb->workload_len; ++i) {
		char op = mb->pargs->workload[i];
		mb->flags[info->worker->index] = 0;
		switch (op) {
			case 'g':
				mb->flags[info->worker->index] =
					RPMEM_FLUSH_RELAXED;
				/* no break here */
			case 'f':
				ret |= rpmem_mixed_op_flush(mb, info);
				break;
			case 'd':
				ret |= rpmem_mixed_op_drain(mb, info);
				break;
			case 'r':
				mb->flags[info->worker->index] =
					RPMEM_PERSIST_RELAXED;
				/* no break here */
			case 'p':
				ret |= rpmem_persist_op(bench, info);
				break;
			default:
				fprintf(stderr, "unknown operation: %c", op);
				return 1;
		}
	}
	return ret;
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
	free(mb->rpp);

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
 * rpmem_flags_init -- initialize flags[] array depending on the selected mode
 */
static int
rpmem_flags_init(struct benchmark *bench, struct benchmark_args *args,
		 struct rpmem_bench *mb)
{
	assert(bench);
	struct benchmark_info *info = pmembench_get_info(bench);
	assert(info);

	mb->flags = (unsigned *)calloc(args->n_threads, sizeof(unsigned));
	if (!mb->flags) {
		perror("calloc");
		return -1;
	}

	unsigned relaxed_flag = 0;
	if (strcmp(info->name, BENCH_RPMEM_PERSIST_NAME) == 0)
		relaxed_flag = RPMEM_PERSIST_RELAXED;
	else if (strcmp(info->name, BENCH_RPMEM_FLUSH_NAME) == 0)
		relaxed_flag = RPMEM_FLUSH_RELAXED;
	/* for rpmem_mixed benchmark flags are set during the benchmark */

	/* for rpmem_persist and rpmem_flush_drain benchmark all ops have the
	 * same flags */
	if (mb->pargs->relaxed) {
		for (unsigned i = 0; i < args->n_threads; ++i)
			mb->flags[i] = relaxed_flag;
	}

	return 0;
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
	mb->pargs->chunk_size = args->dsize;

	enum operation_mode op_mode = parse_op_mode(mb->pargs->mode);
	if (op_mode == OP_MODE_UNKNOWN) {
		fprintf(stderr, "Invalid operation mode argument '%s'\n",
			mb->pargs->mode);
		goto err_parse_mode;
	}

	if (rpmem_flags_init(bench, args, mb))
		goto err_flags_init;

	mb->workload_len = 0;
	if (mb->pargs->workload) {
		mb->workload_len = strlen(mb->pargs->workload);
		assert(mb->workload_len > 0);
	}

	rpmem_set_min_size(mb, op_mode, args);

	if (rpmem_poolset_init(args->fname, mb, args)) {
		goto err_poolset_init;
	}

	/* initialize offsets[] array depending on benchmark args */
	mb->n_flushing_ops_per_thread =
		get_flushing_op_num(bench, mb) * args->n_ops_per_thread;
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
	free(mb->offsets_pos);
	free(mb->offsets);
err_init_offsets:
	rpmem_poolset_fini(mb);
err_poolset_init:
	free(mb->flags);
err_flags_init:
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
	free(mb->offsets_pos);
	free(mb->offsets);
	free(mb->flags);
	free(mb);
	return 0;
}

static struct benchmark_clo rpmem_flush_clo[6];
static struct benchmark_clo rpmem_persist_clo[5];
static struct benchmark_clo rpmem_mixed_clo[5];
/* Stores information about benchmark. */
static struct benchmark_info rpmem_flush_info;
static struct benchmark_info rpmem_persist_info;
static struct benchmark_info rpmem_mixed_info;
CONSTRUCTOR(rpmem_constructor)
void
rpmem_constructor(void)
{
	static struct benchmark_clo common_clo[4];
	static struct benchmark_info common_info;
	memset(&common_info, 0, sizeof(common_info));

	/* common benchmarks definitions */
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

	common_clo[3].opt_short = 'T';
	common_clo[3].opt_long = "no-memset";
	common_clo[3].descr = "Don't call memset for all rpmem_persist";
	common_clo[3].def = "false";
	common_clo[3].off = clo_field_offset(struct rpmem_args, no_memset);
	common_clo[3].type = CLO_TYPE_FLAG;

	common_info.init = rpmem_init;
	common_info.exit = rpmem_exit;
	common_info.multithread = true;
	common_info.multiops = true;
	common_info.measure_time = true;
	common_info.opts_size = sizeof(struct rpmem_args);
	common_info.rm_file = true;
	common_info.allow_poolset = true;
	common_info.print_bandwidth = true;

	/* rpmem_flush_drain benchmark definitions */
	assert(sizeof(rpmem_flush_clo) >= sizeof(common_clo));
	memcpy(rpmem_flush_clo, common_clo, sizeof(common_clo));
	rpmem_flush_clo[4].opt_short = 0;
	rpmem_flush_clo[4].opt_long = "flushes-per-drain";
	rpmem_flush_clo[4].descr =
		"Number of flushes between drains (-1 means flushes only)";
	rpmem_flush_clo[4].def = "-1";
	rpmem_flush_clo[4].off =
		clo_field_offset(struct rpmem_args, flushes_per_drain);
	rpmem_flush_clo[4].type = CLO_TYPE_INT;
	rpmem_flush_clo[4].type_int.size =
		clo_field_size(struct rpmem_args, flushes_per_drain);
	rpmem_flush_clo[4].type_int.base = CLO_INT_BASE_DEC;
	rpmem_flush_clo[4].type_int.min = -1;
	rpmem_flush_clo[4].type_int.max = INT_MAX;

	rpmem_flush_clo[5].opt_short = 0;
	rpmem_flush_clo[5].opt_long = "flush-relaxed";
	rpmem_flush_clo[5].descr = "Use RPMEM_FLUSH_RELAXED flag";
	rpmem_flush_clo[5].def = "false";
	rpmem_flush_clo[5].off = clo_field_offset(struct rpmem_args, relaxed);
	rpmem_flush_clo[5].type = CLO_TYPE_FLAG;

	memcpy(&rpmem_flush_info, &common_info, sizeof(common_info));
	rpmem_flush_info.name = BENCH_RPMEM_FLUSH_NAME;
	rpmem_flush_info.brief =
		"Benchmark for rpmem_flush() and rpmem_drain() operations";
	rpmem_flush_info.operation = rpmem_flush_drain_op;
	rpmem_flush_info.clos = rpmem_flush_clo;
	rpmem_flush_info.nclos = ARRAY_SIZE(rpmem_flush_clo);
	REGISTER_BENCHMARK(rpmem_flush_info);

	/* rpmem_persist benchmark definitions */
	assert(sizeof(rpmem_persist_clo) >= sizeof(common_clo));
	memcpy(rpmem_persist_clo, common_clo, sizeof(common_clo));
	rpmem_persist_clo[4].opt_short = 0;
	rpmem_persist_clo[4].opt_long = "persist-relaxed";
	rpmem_persist_clo[4].descr = "Use RPMEM_PERSIST_RELAXED flag";
	rpmem_persist_clo[4].def = "false";
	rpmem_persist_clo[4].off = clo_field_offset(struct rpmem_args, relaxed);
	rpmem_persist_clo[4].type = CLO_TYPE_FLAG;

	memcpy(&rpmem_persist_info, &common_info, sizeof(common_info));
	rpmem_persist_info.name = BENCH_RPMEM_PERSIST_NAME;
	rpmem_persist_info.brief = "Benchmark for rpmem_persist() operation";
	rpmem_persist_info.operation = rpmem_persist_op;
	rpmem_persist_info.clos = rpmem_persist_clo;
	rpmem_persist_info.nclos = ARRAY_SIZE(rpmem_persist_clo);
	REGISTER_BENCHMARK(rpmem_persist_info);

	/* rpmem_mixed benchmark definitions */
	assert(sizeof(rpmem_mixed_clo) >= sizeof(common_clo));
	memcpy(rpmem_mixed_clo, common_clo, sizeof(common_clo));
	rpmem_mixed_clo[4].opt_short = 0;
	rpmem_mixed_clo[4].opt_long = "workload";
	rpmem_mixed_clo[4].descr = "Workload e.g.: 'prfgd' means "
				   "rpmem_persist(), "
				   "rpmem_persist() + RPMEM_PERSIST_RELAXED, "
				   "rpmem_flush(),"
				   "rpmem_flush() + RPMEM_FLUSH_RELAXED "
				   "and rpmem_drain()";
	rpmem_mixed_clo[4].def = "fd";
	rpmem_mixed_clo[4].off = clo_field_offset(struct rpmem_args, workload);
	rpmem_mixed_clo[4].type = CLO_TYPE_STR;

	memcpy(&rpmem_mixed_info, &common_info, sizeof(common_info));
	rpmem_mixed_info.name = BENCH_RPMEM_MIXED_NAME;
	rpmem_mixed_info.brief = "Benchmark for mixed rpmem workloads";
	rpmem_mixed_info.operation = rpmem_mixed_op;
	rpmem_mixed_info.clos = rpmem_mixed_clo;
	rpmem_mixed_info.nclos = ARRAY_SIZE(rpmem_mixed_clo);
	REGISTER_BENCHMARK(rpmem_mixed_info);
};
