/*
 * Copyright 2015-2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *	* Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *	* Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *	* Neither the name of the copyright holder nor the names of its
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
 * pmem_memcpy.cpp -- benchmark implementation for pmem_memcpy
 */
#include <cassert>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <libpmem.h>
#include <sys/mman.h>
#include <unistd.h>

#include "benchmark.hpp"
#include "file.h"

#define FLUSH_ALIGN 64

#define MAX_OFFSET (FLUSH_ALIGN - 1)

struct pmem_bench;

typedef size_t (*offset_fn)(struct pmem_bench *pmb,
			    struct operation_info *info);

/*
 * pmem_args -- benchmark specific arguments
 */
struct pmem_args {
	/*
	 * Defines the copy operation direction. Whether it is
	 * writing from RAM to PMEM (for argument value "write")
	 * or PMEM to RAM (for argument value "read").
	 */
	char *operation;

	/*
	 * The source address offset used to test pmem_memcpy()
	 * performance when source address is not aligned.
	 */
	size_t src_off;

	/*
	 * The destination address offset used to test
	 * pmem_memcpy() performance when destination address
	 * is not aligned.
	 */
	size_t dest_off;

	/* The size of data chunk. */
	size_t chunk_size;

	/*
	 * Specifies the order in which data chunks are selected
	 * to be copied. There are three modes supported:
	 * stat, seq, rand.
	 */
	char *src_mode;

	/*
	 * Specifies the order in which data chunks are written
	 * to the destination address. There are three modes
	 * supported: stat, seq, rand.
	 */
	char *dest_mode;

	/*
	 * When this flag is set to true, PMEM is not used.
	 * This option is useful, when comparing performance
	 * of pmem_memcpy() function to regular memcpy().
	 */
	bool memcpy;

	/*
	 * When this flag is set to true, pmem_persist()
	 * function is used, otherwise pmem_flush() is performed.
	 */
	bool persist;

	/* do not do warmup */
	bool no_warmup;
};

/*
 * pmem_bench -- benchmark context
 */
struct pmem_bench {
	/* random offsets */
	unsigned *rand_offsets;

	/* number of elements in randoms array */
	size_t n_rand_offsets;

	/* The size of the allocated PMEM */
	size_t fsize;

	/* The size of the allocated buffer */
	size_t bsize;

	/* Pointer to the allocated volatile memory */
	unsigned char *buf;

	/* Pointer to the allocated PMEM */
	unsigned char *pmem_addr;

	/*
	 * This field gets 'buf' or 'pmem_addr' fields assigned,
	 * depending on the prog_args operation direction.
	 */
	unsigned char *src_addr;

	/*
	 * This field gets 'buf' or 'pmem_addr' fields assigned,
	 * depending on the prog_args operation direction.
	 */
	unsigned char *dest_addr;

	/* Stores prog_args structure */
	struct pmem_args *pargs;

	/*
	 * Function which returns src offset. Matches src_mode.
	 */
	offset_fn func_src;

	/*
	 * Function which returns dst offset. Matches dst_mode.
	 */
	offset_fn func_dest;

	/*
	 * The actual operation performed based on benchmark specific
	 * arguments.
	 */
	int (*func_op)(void *dest, void *source, size_t len);
};

/*
 * operation_type -- type of operation relative to persistent memory
 */
enum operation_type { OP_TYPE_UNKNOWN, OP_TYPE_READ, OP_TYPE_WRITE };

/*
 * operation_mode -- the mode of the copy process
 *
 *	* static - read/write always the same chunk,
 *	* sequential - read/write chunk by chunk,
 *	* random - read/write to chunks selected randomly.
 *
 *  It is used to determine source mode as well as the destination mode.
 */
enum operation_mode {
	OP_MODE_UNKNOWN,
	OP_MODE_STAT,
	OP_MODE_SEQ,
	OP_MODE_RAND
};

/*
 * parse_op_type -- parses command line "--operation" argument
 * and returns proper operation type.
 */
static enum operation_type
parse_op_type(const char *arg)
{
	if (strcmp(arg, "read") == 0)
		return OP_TYPE_READ;
	else if (strcmp(arg, "write") == 0)
		return OP_TYPE_WRITE;
	else
		return OP_TYPE_UNKNOWN;
}

/*
 * parse_op_mode -- parses command line "--src-mode" or "--dest-mode"
 * and returns proper operation mode.
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
 * mode_seq -- if copy mode is sequential mode_seq() returns
 * index of a chunk.
 */
static uint64_t
mode_seq(struct pmem_bench *pmb, struct operation_info *info)
{
	return info->args->n_ops_per_thread * info->worker->index + info->index;
}

/*
 * mode_stat -- if mode is static, the offset is always 0,
 * as only one block is used.
 */
static uint64_t
mode_stat(struct pmem_bench *pmb, struct operation_info *info)
{
	return 0;
}

/*
 * mode_rand -- if mode is random returns index of a random chunk
 */
static uint64_t
mode_rand(struct pmem_bench *pmb, struct operation_info *info)
{
	assert(info->index < pmb->n_rand_offsets);
	return info->args->n_ops_per_thread * info->worker->index +
		pmb->rand_offsets[info->index];
}

/*
 * assign_mode_func -- parses "--src-mode" and "--dest-mode" command line
 * arguments and returns one of the above mode functions.
 */
static offset_fn
assign_mode_func(char *option)
{
	enum operation_mode op_mode = parse_op_mode(option);

	switch (op_mode) {
		case OP_MODE_STAT:
			return mode_stat;
		case OP_MODE_SEQ:
			return mode_seq;
		case OP_MODE_RAND:
			return mode_rand;
		default:
			return nullptr;
	}
}

/*
 * libc_memcpy -- copy using libc memcpy() function
 * followed by pmem_flush().
 */
static int
libc_memcpy(void *dest, void *source, size_t len)
{
	memcpy(dest, source, len);

	pmem_flush(dest, len);

	return 0;
}

/*
 * libc_memcpy_persist -- copy using libc memcpy() function
 * followed by pmem_persist().
 */
static int
libc_memcpy_persist(void *dest, void *source, size_t len)
{
	memcpy(dest, source, len);

	pmem_persist(dest, len);

	return 0;
}

/*
 * lipmem_memcpy_nodrain -- copy using libpmem pmem_memcpy_no_drain()
 * function without pmem_persist().
 */
static int
libpmem_memcpy_nodrain(void *dest, void *source, size_t len)
{
	pmem_memcpy_nodrain(dest, source, len);

	return 0;
}

/*
 * libpmem_memcpy_persist -- copy using libpmem pmem_memcpy_persist() function.
 */
static int
libpmem_memcpy_persist(void *dest, void *source, size_t len)
{
	pmem_memcpy_persist(dest, source, len);

	return 0;
}

/*
 * assign_size -- assigns file and buffer size
 * depending on the operation mode and type.
 */
static int
assign_size(struct pmem_bench *pmb, struct benchmark_args *args,
	    enum operation_type *op_type)
{
	*op_type = parse_op_type(pmb->pargs->operation);

	if (*op_type == OP_TYPE_UNKNOWN) {
		fprintf(stderr, "Invalid operation argument '%s'",
			pmb->pargs->operation);
		return -1;
	}
	enum operation_mode op_mode_src = parse_op_mode(pmb->pargs->src_mode);
	if (op_mode_src == OP_MODE_UNKNOWN) {
		fprintf(stderr, "Invalid source mode argument '%s'",
			pmb->pargs->src_mode);
		return -1;
	}
	enum operation_mode op_mode_dest = parse_op_mode(pmb->pargs->dest_mode);
	if (op_mode_dest == OP_MODE_UNKNOWN) {
		fprintf(stderr, "Invalid destination mode argument '%s'",
			pmb->pargs->dest_mode);
		return -1;
	}

	size_t large = args->n_ops_per_thread * pmb->pargs->chunk_size *
		args->n_threads;
	size_t little = pmb->pargs->chunk_size;

	if (*op_type == OP_TYPE_WRITE) {
		pmb->bsize = op_mode_src == OP_MODE_STAT ? little : large;
		pmb->fsize = op_mode_dest == OP_MODE_STAT ? little : large;

		if (pmb->pargs->src_off != 0)
			pmb->bsize += MAX_OFFSET;
		if (pmb->pargs->dest_off != 0)
			pmb->fsize += MAX_OFFSET;
	} else {
		pmb->fsize = op_mode_src == OP_MODE_STAT ? little : large;
		pmb->bsize = op_mode_dest == OP_MODE_STAT ? little : large;

		if (pmb->pargs->src_off != 0)
			pmb->fsize += MAX_OFFSET;
		if (pmb->pargs->dest_off != 0)
			pmb->bsize += MAX_OFFSET;
	}

	return 0;
}

/*
 * pmem_memcpy_init -- benchmark initialization
 *
 * Parses command line arguments, allocates persistent memory, and maps it.
 */
static int
pmem_memcpy_init(struct benchmark *bench, struct benchmark_args *args)
{
	assert(bench != nullptr);
	assert(args != nullptr);
	int ret = 0;
	size_t file_size = 0;
	int flags = 0;

	enum file_type type = util_file_get_type(args->fname);
	if (type == OTHER_ERROR) {
		fprintf(stderr, "could not check type of file %s\n",
			args->fname);
		return -1;
	}

	auto *pmb = (struct pmem_bench *)malloc(sizeof(struct pmem_bench));
	assert(pmb != nullptr);

	pmb->pargs = (struct pmem_args *)args->opts;
	assert(pmb->pargs != nullptr);

	pmb->pargs->chunk_size = args->dsize;

	enum operation_type op_type;
	/*
	 * Assign file and buffer size depending on the operation type
	 * (READ from PMEM or WRITE to PMEM)
	 */
	if (assign_size(pmb, args, &op_type) != 0) {
		ret = -1;
		goto err_free_pmb;
	}
	pmb->buf =
		(unsigned char *)util_aligned_malloc(FLUSH_ALIGN, pmb->bsize);
	if (pmb->buf == nullptr) {
		perror("posix_memalign");
		ret = -1;
		goto err_free_pmb;
	}

	pmb->n_rand_offsets = args->n_ops_per_thread * args->n_threads;
	assert(pmb->n_rand_offsets != 0);
	pmb->rand_offsets = (unsigned *)malloc(pmb->n_rand_offsets *
					       sizeof(*pmb->rand_offsets));

	if (pmb->rand_offsets == nullptr) {
		perror("malloc");
		ret = -1;
		goto err_free_pmb_buf;
	}

	for (size_t i = 0; i < pmb->n_rand_offsets; ++i)
		pmb->rand_offsets[i] = rand() % args->n_ops_per_thread;

	if (type != TYPE_DEVDAX) {
		file_size = pmb->fsize;
		flags = PMEM_FILE_CREATE | PMEM_FILE_EXCL;
	}

	/* create a pmem file and memory map it */
	pmb->pmem_addr = (unsigned char *)pmem_map_file(
		args->fname, file_size, flags, args->fmode, nullptr, nullptr);
	if (pmb->pmem_addr == nullptr) {
		perror(args->fname);
		ret = -1;
		goto err_free_pmb_rand_offsets;
	}

	if (op_type == OP_TYPE_READ) {
		pmb->src_addr = pmb->pmem_addr;
		pmb->dest_addr = pmb->buf;
	} else {
		pmb->src_addr = pmb->buf;
		pmb->dest_addr = pmb->pmem_addr;
	}

	/* set proper func_src() and func_dest() depending on benchmark args */
	if ((pmb->func_src = assign_mode_func(pmb->pargs->src_mode)) ==
	    nullptr) {
		fprintf(stderr, "wrong src_mode parameter -- '%s'",
			pmb->pargs->src_mode);
		ret = -1;
		goto err_unmap;
	}

	if ((pmb->func_dest = assign_mode_func(pmb->pargs->dest_mode)) ==
	    nullptr) {
		fprintf(stderr, "wrong dest_mode parameter -- '%s'",
			pmb->pargs->dest_mode);
		ret = -1;
		goto err_unmap;
	}

	if (pmb->pargs->memcpy) {
		pmb->func_op =
			pmb->pargs->persist ? libc_memcpy_persist : libc_memcpy;
	} else {
		pmb->func_op = pmb->pargs->persist ? libpmem_memcpy_persist
						   : libpmem_memcpy_nodrain;
	}

	if (!pmb->pargs->no_warmup) {
		memset(pmb->buf, 0, pmb->bsize);
		pmem_memset_persist(pmb->pmem_addr, 0, pmb->fsize);
	}

	pmembench_set_priv(bench, pmb);

	return 0;

err_unmap:
	pmem_unmap(pmb->pmem_addr, pmb->fsize);
err_free_pmb_rand_offsets:
	free(pmb->rand_offsets);
err_free_pmb_buf:
	util_aligned_free(pmb->buf);
err_free_pmb:
	free(pmb);

	return ret;
}

/*
 * pmem_memcpy_operation -- actual benchmark operation
 *
 * Depending on the memcpy flag "-m" tested operation will be memcpy()
 * or pmem_memcpy_persist().
 */
static int
pmem_memcpy_operation(struct benchmark *bench, struct operation_info *info)
{
	auto *pmb = (struct pmem_bench *)pmembench_get_priv(bench);

	size_t src_index = pmb->func_src(pmb, info);

	size_t dest_index = pmb->func_dest(pmb, info);

	void *source = pmb->src_addr + src_index * pmb->pargs->chunk_size +
		pmb->pargs->src_off;
	void *dest = pmb->dest_addr + dest_index * pmb->pargs->chunk_size +
		pmb->pargs->dest_off;
	size_t len = pmb->pargs->chunk_size;

	pmb->func_op(dest, source, len);
	return 0;
}

/*
 * pmem_memcpy_exit -- benchmark cleanup
 */
static int
pmem_memcpy_exit(struct benchmark *bench, struct benchmark_args *args)
{
	auto *pmb = (struct pmem_bench *)pmembench_get_priv(bench);
	pmem_unmap(pmb->pmem_addr, pmb->fsize);
	util_aligned_free(pmb->buf);
	free(pmb->rand_offsets);
	free(pmb);
	return 0;
}

/* structure to define command line arguments */
static struct benchmark_clo pmem_memcpy_clo[8];

/* Stores information about benchmark. */
static struct benchmark_info pmem_memcpy_bench;
CONSTRUCTOR(pmem_memcpy_constructor)
void
pmem_memcpy_constructor(void)
{
	pmem_memcpy_clo[0].opt_short = 'o';
	pmem_memcpy_clo[0].opt_long = "operation";
	pmem_memcpy_clo[0].descr = "Operation type - write, read";
	pmem_memcpy_clo[0].type = CLO_TYPE_STR;
	pmem_memcpy_clo[0].off = clo_field_offset(struct pmem_args, operation);
	pmem_memcpy_clo[0].def = "write";

	pmem_memcpy_clo[1].opt_short = 'S';
	pmem_memcpy_clo[1].opt_long = "src-offset";
	pmem_memcpy_clo[1].descr = "Source cache line alignment"
				   " offset";
	pmem_memcpy_clo[1].type = CLO_TYPE_UINT;
	pmem_memcpy_clo[1].off = clo_field_offset(struct pmem_args, src_off);
	pmem_memcpy_clo[1].def = "0";
	pmem_memcpy_clo[1].type_uint.size =
		clo_field_size(struct pmem_args, src_off);
	pmem_memcpy_clo[1].type_uint.base = CLO_INT_BASE_DEC;
	pmem_memcpy_clo[1].type_uint.min = 0;
	pmem_memcpy_clo[1].type_uint.max = MAX_OFFSET;

	pmem_memcpy_clo[2].opt_short = 'D';
	pmem_memcpy_clo[2].opt_long = "dest-offset";
	pmem_memcpy_clo[2].descr = "Destination cache line "
				   "alignment offset";
	pmem_memcpy_clo[2].type = CLO_TYPE_UINT;
	pmem_memcpy_clo[2].off = clo_field_offset(struct pmem_args, dest_off);
	pmem_memcpy_clo[2].def = "0";
	pmem_memcpy_clo[2].type_uint.size =
		clo_field_size(struct pmem_args, dest_off);
	pmem_memcpy_clo[2].type_uint.base = CLO_INT_BASE_DEC;
	pmem_memcpy_clo[2].type_uint.min = 0;
	pmem_memcpy_clo[2].type_uint.max = MAX_OFFSET;

	pmem_memcpy_clo[3].opt_short = 0;
	pmem_memcpy_clo[3].opt_long = "src-mode";
	pmem_memcpy_clo[3].descr = "Source reading mode";
	pmem_memcpy_clo[3].type = CLO_TYPE_STR;
	pmem_memcpy_clo[3].off = clo_field_offset(struct pmem_args, src_mode);
	pmem_memcpy_clo[3].def = "seq";

	pmem_memcpy_clo[4].opt_short = 0;
	pmem_memcpy_clo[4].opt_long = "dest-mode";
	pmem_memcpy_clo[4].descr = "Destination writing mode";
	pmem_memcpy_clo[4].type = CLO_TYPE_STR;
	pmem_memcpy_clo[4].off = clo_field_offset(struct pmem_args, dest_mode);
	pmem_memcpy_clo[4].def = "seq";

	pmem_memcpy_clo[5].opt_short = 'm';
	pmem_memcpy_clo[5].opt_long = "libc-memcpy";
	pmem_memcpy_clo[5].descr = "Use libc memcpy()";
	pmem_memcpy_clo[5].type = CLO_TYPE_FLAG;
	pmem_memcpy_clo[5].off = clo_field_offset(struct pmem_args, memcpy);
	pmem_memcpy_clo[5].def = "false";

	pmem_memcpy_clo[6].opt_short = 'p';
	pmem_memcpy_clo[6].opt_long = "persist";
	pmem_memcpy_clo[6].descr = "Use pmem_persist()";
	pmem_memcpy_clo[6].type = CLO_TYPE_FLAG;
	pmem_memcpy_clo[6].off = clo_field_offset(struct pmem_args, persist);
	pmem_memcpy_clo[6].def = "true";

	pmem_memcpy_clo[7].opt_short = 'w';
	pmem_memcpy_clo[7].opt_long = "no-warmup";
	pmem_memcpy_clo[7].descr = "Don't do warmup";
	pmem_memcpy_clo[7].def = "false";
	pmem_memcpy_clo[7].type = CLO_TYPE_FLAG;
	pmem_memcpy_clo[7].off = clo_field_offset(struct pmem_args, no_warmup);

	pmem_memcpy_bench.name = "pmem_memcpy";
	pmem_memcpy_bench.brief = "Benchmark for"
				  "pmem_memcpy_persist() and "
				  "pmem_memcpy_nodrain()"
				  "operations";
	pmem_memcpy_bench.init = pmem_memcpy_init;
	pmem_memcpy_bench.exit = pmem_memcpy_exit;
	pmem_memcpy_bench.multithread = true;
	pmem_memcpy_bench.multiops = true;
	pmem_memcpy_bench.operation = pmem_memcpy_operation;
	pmem_memcpy_bench.measure_time = true;
	pmem_memcpy_bench.clos = pmem_memcpy_clo;
	pmem_memcpy_bench.nclos = ARRAY_SIZE(pmem_memcpy_clo);
	pmem_memcpy_bench.opts_size = sizeof(struct pmem_args);
	pmem_memcpy_bench.rm_file = true;
	pmem_memcpy_bench.allow_poolset = false;
	pmem_memcpy_bench.print_bandwidth = true;
	REGISTER_BENCHMARK(pmem_memcpy_bench);
};
