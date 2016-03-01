/*
 * Copyright 2016, Intel Corporation
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
 * pmem_flush.c -- benchmark implementation for pmem_persist and pmem_msync
 */
#include <libpmem.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <sys/mman.h>

#include "benchmark.h"

#define	PAGE_4K ((uintptr_t)1 << 12)
#define	PAGE_2M ((uintptr_t)1 << 21)

/*
 * pmem_args -- benchmark specific arguments
 */
struct pmem_args
{
	char *operation;	/* msync, dummy_msync, persist, ... */
	char *mode;		/* stat, seq, rand */
	bool no_warmup;		/* don't do warmup */
};

/*
 * pmem_bench -- benchmark context
 */
struct pmem_bench
{
	uint64_t *offsets;	/* write offsets */
	size_t n_offsets;	/* number of elements in offsets array */
	size_t fsize;		/* The size of the allocated PMEM */

	struct pmem_args *pargs;	/* prog_args structure */

	void *pmem_addr;	/* PMEM base address */
	size_t pmem_len;	/* length of PMEM mapping */

	void *invalid_addr;	/* invalid pages */
	void *nondirty_addr;	/* non-dirty pages */

	void *pmem_addr_aligned;	/* PMEM pages - 2M aligned */
	void *invalid_addr_aligned;	/* invalid pages - 2M aligned */
	void *nondirty_addr_aligned;	/* non-dirty pages - 2M aligned */

	/* the actual benchmark operation */
	int (*func_op) (struct pmem_bench *pmb, void *addr, size_t len);
};


/*
 * mode_seq -- if copy mode is sequential, returns index of a chunk.
 */
static uint64_t
mode_seq(struct pmem_bench *pmb, uint64_t index)
{
	return index;
}

/*
 * mode_stat -- if mode is static, the offset is always 0
 */
static uint64_t
mode_stat(struct pmem_bench *pmb, uint64_t index)
{
	return 0;
}

/*
 * mode_rand -- if mode is random, returns index of a random chunk
 */
static uint64_t
mode_rand(struct pmem_bench *pmb, uint64_t index)
{
	return rand() % pmb->n_offsets;
}

/*
 * operation_mode -- the mode of the copy process
 *
 *	* static     - write always the same chunk,
 *	* sequential - write chunk by chunk,
 *	* random     - write to chunks selected randomly.
 */
struct mode_t {
	const char *mode;
	uint64_t (*func_mode) (struct pmem_bench *pmb, uint64_t index);
};

static struct mode_t modes[] = {
	{ "stat", mode_stat },
	{ "seq", mode_seq },
	{ "rand", mode_rand },
};

#define	MODES (sizeof (modes) / sizeof (modes[0]))

/*
 * parse_op_mode -- parses command line "--mode"
 * and returns proper operation mode index.
 */
static int
parse_op_mode(const char *arg)
{
	for (int i = 0; i < MODES; i++) {
		if (strcmp(arg, modes[i].mode) == 0)
			return i;
	}
	return -1;
}

/*
 * flush_noop -- dummy flush, does nothing
 */
static int
flush_noop(struct pmem_bench *pmb, void *addr, size_t len)
{
	return 0;
}

/*
 * flush_persist -- flush data to persistence using pmem_persist()
 */
static int
flush_persist(struct pmem_bench *pmb, void *addr, size_t len)
{
	pmem_persist(addr, len);
	return 0;
}

/*
 * flush_persist4K -- always flush entire 4K page(s) using pmem_persist()
 */
static int
flush_persist_4K(struct pmem_bench *pmb, void *addr, size_t len)
{
	/* increase len by the amount we gain when we round addr down */
	len += (uintptr_t)addr & (PAGE_4K - 1);

	/* then round up to the nearest multiple of 4K */
	len = (len + PAGE_4K - 1) & ~(PAGE_4K - 1);

	/* round addr down to page boundary */
	uintptr_t uptr = (uintptr_t)addr & ~(PAGE_4K - 1);

	pmem_persist((void *)uptr, len);
	return 0;
}

/*
 * flush_persist2M -- always flush entire 2M page(s) using pmem_persist()
 */
static int
flush_persist_2M(struct pmem_bench *pmb, void *addr, size_t len)
{
	/* increase len by the amount we gain when we round addr down */
	len += (uintptr_t)addr & (PAGE_2M - 1);

	/* then round up to the nearest multiple of 4K */
	len = (len + PAGE_2M - 1) & ~(PAGE_2M - 1);

	/* round addr down to page boundary */
	uintptr_t uptr = (uintptr_t)addr & ~(PAGE_2M - 1);

	pmem_persist((void *)uptr, len);
	return 0;
}

/*
 * flush_msync -- flush data to persistence using pmem_msync()
 */
static int
flush_msync(struct pmem_bench *pmb, void *addr, size_t len)
{
	pmem_msync(addr, len);
	return 0;
}

/*
 * flush_msync_async -- emulate dummy msync() using MS_ASYNC flag
 */
static int
flush_msync_async(struct pmem_bench *pmb, void *addr, size_t len)
{
	/* increase len by the amount we gain when we round addr down */
	len += (uintptr_t)addr & (PAGE_4K - 1);

	/* round addr down to page boundary */
	uintptr_t uptr = (uintptr_t)addr & ~(PAGE_4K - 1);

	msync((void *)uptr, len, MS_ASYNC);
	return 0;
}

/*
 * flush_msync_0 -- emulate dummy msync() using zero length
 */
static int
flush_msync_0(struct pmem_bench *pmb, void *addr, size_t len)
{
	/* increase len by the amount we gain when we round addr down */
	len += (uintptr_t)addr & (PAGE_4K - 1);

	/* round addr down to page boundary */
	uintptr_t uptr = (uintptr_t)addr & ~(PAGE_4K - 1);

	msync((void *)uptr, 0, MS_SYNC);
	return 0;
}

/*
 * flush_msync_err -- emulate dummy msync() using invalid flags
 */
static int
flush_msync_err(struct pmem_bench *pmb, void *addr, size_t len)
{
	/* increase len by the amount we gain when we round addr down */
	len += (uintptr_t)addr & (PAGE_4K - 1);

	/* round addr down to page boundary */
	uintptr_t uptr = (uintptr_t)addr & ~(PAGE_4K - 1);

	msync((void *)uptr, len, MS_SYNC|MS_ASYNC);
	return 0;
}

/*
 * flush_msync_nodirty -- call msync() on non-dirty pages
 */
static int
flush_msync_nodirty(struct pmem_bench *pmb, void *addr, size_t len)
{
	uintptr_t uptr = (uintptr_t)addr - (uintptr_t)pmb->pmem_addr_aligned;
	uptr += (uintptr_t)pmb->nondirty_addr_aligned;

	/* increase len by the amount we gain when we round addr down */
	len += (uintptr_t)uptr & (PAGE_4K - 1);

	/* round addr down to page boundary */
	uptr &= ~(PAGE_4K - 1);

	pmem_msync((void *)uptr, len);
	return 0;
}

/*
 * flush_msync_invalid -- emulate dummy msync() using invalid address
 */
static int
flush_msync_invalid(struct pmem_bench *pmb, void *addr, size_t len)
{
	uintptr_t uptr = (uintptr_t)addr - (uintptr_t)pmb->pmem_addr_aligned;
	uptr += (uintptr_t)pmb->invalid_addr_aligned;

	/* increase len by the amount we gain when we round addr down */
	len += (uintptr_t)uptr & (PAGE_4K - 1);

	/* round addr down to page boundary */
	uptr &= ~(PAGE_4K - 1);

	pmem_msync((void *)uptr, len);
	return 0;
}

struct op_t {
	const char *opname;
	int (*func_op) (struct pmem_bench *pmb, void *addr, size_t len);
};

static struct op_t ops[] = {
	{ "noop", flush_noop },
	{ "persist", flush_persist },
	{ "persist_4K", flush_persist_4K },
	{ "persist_2M", flush_persist_2M },
	{ "msync", flush_msync },
	{ "msync_async", flush_msync_async },
	{ "msync_0", flush_msync_0 },
	{ "msync_err", flush_msync_err },
	{ "msync_nodirty", flush_msync_nodirty },
	{ "msync_invalid", flush_msync_invalid },
};

#define	NOPS (sizeof (ops) / sizeof (ops[0]))

/*
 * parse_op_type -- parses command line "--operation" argument
 * and returns proper operation type.
 */
static int
parse_op_type(const char *arg)
{
	for (int i = 0; i < NOPS; i++) {
		if (strcmp(arg, ops[i].opname) == 0)
			return i;
	}
	return -1;
}

/*
 * pmem_flush_init -- benchmark initialization
 *
 * Parses command line arguments, allocates persistent memory, and maps it.
 */
static int
pmem_flush_init(struct benchmark *bench, struct benchmark_args *args)
{
	assert(bench != NULL);
	assert(args != NULL);

	uint64_t (*func_mode) (struct pmem_bench *pmb, uint64_t index);

	struct pmem_bench *pmb = malloc(sizeof (struct pmem_bench));
	assert(pmb != NULL);

	pmb->pargs = args->opts;
	assert(pmb->pargs != NULL);

	int i = parse_op_type(pmb->pargs->operation);
	if (i == -1) {
		fprintf(stderr, "wrong operation: %s\n", pmb->pargs->operation);
		goto err_free_pmb;
	}
	pmb->func_op = ops[i].func_op;

	pmb->n_offsets = args->n_ops_per_thread * args->n_threads;

	pmb->fsize = pmb->n_offsets * args->dsize + (2 * PAGE_2M);

	/* round up to 2M boundary */
	pmb->fsize = (pmb->fsize + PAGE_2M - 1) & ~(PAGE_2M - 1);

	i = parse_op_mode(pmb->pargs->mode);
	if (i == -1) {
		fprintf(stderr, "wrong mode: %s\n", pmb->pargs->mode);
		goto err_free_pmb;
	}
	func_mode = modes[i].func_mode;

	/* populate offsets array */
	pmb->offsets = malloc(pmb->n_offsets * sizeof (*pmb->offsets));

	for (size_t i = 0; i < pmb->n_offsets; ++i)
		pmb->offsets[i] = func_mode(pmb, i);

	/* create a pmem file and memory map it */
	pmb->pmem_addr = pmem_map_file(args->fname, pmb->fsize,
			PMEM_FILE_CREATE|PMEM_FILE_EXCL, args->fmode,
			&pmb->pmem_len, NULL);

	if (pmb->pmem_addr == NULL) {
		perror("pmem_map_file");
		goto err_free_pmb;
	}

	pmb->nondirty_addr = mmap(NULL, pmb->fsize, PROT_READ|PROT_WRITE,
			MAP_PRIVATE|MAP_ANON, -1, 0);

	if (pmb->nondirty_addr == MAP_FAILED) {
		perror("pmem_map1");
		goto err_unmap1;
	}

	pmb->invalid_addr = mmap(NULL, pmb->fsize, PROT_READ|PROT_WRITE,
			MAP_PRIVATE|MAP_ANON, -1, 0);

	if (pmb->invalid_addr == MAP_FAILED) {
		perror("pmem_map2");
		goto err_unmap2;
	}
	munmap(pmb->invalid_addr, pmb->fsize);

	pmb->pmem_addr_aligned =
		(void *)(((uintptr_t)pmb->pmem_addr + PAGE_2M - 1)
		& ~(PAGE_2M - 1));
	pmb->nondirty_addr_aligned =
		(void *)(((uintptr_t)pmb->nondirty_addr + PAGE_2M - 1)
		& ~(PAGE_2M - 1));
	pmb->invalid_addr_aligned =
		(void *)(((uintptr_t)pmb->invalid_addr + PAGE_2M - 1)
		& ~(PAGE_2M - 1));

	pmembench_set_priv(bench, pmb);

	if (!pmb->pargs->no_warmup) {
		size_t off;
		for (off = 0; off < pmb->fsize - PAGE_2M; off += PAGE_4K) {
			*(int *)((char *)pmb->pmem_addr_aligned + off) = 0;
			*(int *)((char *)pmb->nondirty_addr_aligned + off) = 0;
		}
	}

	return 0;

err_unmap2:
	munmap(pmb->nondirty_addr, pmb->fsize);
err_unmap1:
	pmem_unmap(pmb->pmem_addr, pmb->pmem_len);
err_free_pmb:
	free(pmb);

	return -1;
}

/*
 * pmem_flush_exit -- benchmark cleanup
 */
static int
pmem_flush_exit(struct benchmark *bench, struct benchmark_args *args)
{
	struct pmem_bench *pmb = (struct pmem_bench *)pmembench_get_priv(bench);
	pmem_unmap(pmb->pmem_addr, pmb->pmem_len);
	munmap(pmb->nondirty_addr, pmb->pmem_len);
	free(pmb);
	return 0;
}

/*
 * pmem_flush_operation -- actual benchmark operation
 */
static int
pmem_flush_operation(struct benchmark *bench, struct operation_info *info)
{
	struct pmem_bench *pmb = (struct pmem_bench *)pmembench_get_priv(bench);

	int op_idx = info->index;
	assert(op_idx < pmb->n_offsets);

	uint64_t chunk_idx = pmb->offsets[op_idx];
	void *addr = (char *)pmb->pmem_addr_aligned
					+ chunk_idx * info->args->dsize;

	/* store + flush */
	*(int *)addr = *(int *)addr + 1;
	pmb->func_op(pmb, addr, info->args->dsize);
	return 0;
}

/* structure to define command line arguments */
static struct benchmark_clo pmem_flush_clo[] = {
	{
		.opt_short	= 'o',
		.opt_long	= "operation",
		.descr		= "Operation type - persist, msync, ...",
		.type		= CLO_TYPE_STR,
		.off		= clo_field_offset(struct pmem_args, operation),
		.def		= "noop"
	},
	{
		.opt_short	= 0,
		.opt_long	= "mode",
		.descr		= "mode - stat, seq or rand",
		.type		= CLO_TYPE_STR,
		.off		= clo_field_offset(struct pmem_args, mode),
		.def		= "stat",
	},
	{
		.opt_short	= 'w',
		.opt_long	= "no-warmup",
		.descr		= "Don't do warmup",
		.type		= CLO_TYPE_FLAG,
		.off		= clo_field_offset(struct pmem_args, no_warmup),
	},
};

/* Stores information about benchmark. */
static struct benchmark_info pmem_flush_bench = {
	.name		= "pmem_flush",
	.brief		= "Benchmark for pmem_msync() and pmem_persist()",
	.init		= pmem_flush_init,
	.exit		= pmem_flush_exit,
	.multithread	= true,
	.multiops	= true,
	.operation	= pmem_flush_operation,
	.measure_time	= true,
	.clos		= pmem_flush_clo,
	.nclos		= ARRAY_SIZE(pmem_flush_clo),
	.opts_size	= sizeof (struct pmem_args),
	.rm_file	= true,
	.allow_poolset	= false,
};

REGISTER_BENCHMARK(pmem_flush_bench);
