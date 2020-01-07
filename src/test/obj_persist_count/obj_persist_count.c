// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */

/*
 * obj_persist_count.c -- counting number of persists
 */
#define _GNU_SOURCE

#include "obj.h"
#include "pmalloc.h"
#include "unittest.h"

struct ops_counter {
	unsigned n_cl_stores;
	unsigned n_drain;
	unsigned n_pmem_persist;
	unsigned n_pmem_msync;
	unsigned n_pmem_flush;
	unsigned n_pmem_drain;
	unsigned n_flush_from_pmem_memcpy;
	unsigned n_flush_from_pmem_memset;
	unsigned n_drain_from_pmem_memcpy;
	unsigned n_drain_from_pmem_memset;
	unsigned n_pot_cache_misses;
};

static struct ops_counter ops_counter;
static struct ops_counter tx_counter;

#define FLUSH_ALIGN ((uintptr_t)64)
#define MOVNT_THRESHOLD	256

static unsigned
cl_flushed(const void *addr, size_t len, uintptr_t alignment)
{
	uintptr_t start = (uintptr_t)addr & ~(alignment - 1);
	uintptr_t end = ((uintptr_t)addr + len + alignment - 1) &
			~(alignment - 1);

	return (unsigned)(end - start) / FLUSH_ALIGN;
}

#define PMEM_F_MEM_MOVNT (PMEM_F_MEM_WC | PMEM_F_MEM_NONTEMPORAL)
#define PMEM_F_MEM_MOV   (PMEM_F_MEM_WB | PMEM_F_MEM_TEMPORAL)

static unsigned
bulk_cl_changed(const void *addr, size_t len, unsigned flags)
{
	uintptr_t start = (uintptr_t)addr & ~(FLUSH_ALIGN - 1);
	uintptr_t end = ((uintptr_t)addr + len + FLUSH_ALIGN - 1) &
			~(FLUSH_ALIGN - 1);

	unsigned cl_changed = (unsigned)(end - start) / FLUSH_ALIGN;

	int wc; /* write combining */
	if (flags & PMEM_F_MEM_NOFLUSH)
		wc = 0; /* NOFLUSH always uses temporal instructions */
	else if (flags & PMEM_F_MEM_MOVNT)
		wc = 1;
	else if (flags & PMEM_F_MEM_MOV)
		wc = 0;
	else if (len < MOVNT_THRESHOLD)
		wc = 0;
	else
		wc = 1;

	/* count number of potential cache misses */
	if (!wc) {
		/*
		 * When we don't use write combining, it means all
		 * cache lines may be missing.
		 */
		ops_counter.n_pot_cache_misses += cl_changed;
	} else {
		/*
		 * When we use write combining there won't be any cache misses,
		 * with an exception of unaligned beginning or end.
		 */
		if (start != (uintptr_t)addr)
			ops_counter.n_pot_cache_misses++;
		if (end != ((uintptr_t)addr + len) &&
				start + FLUSH_ALIGN != end)
			ops_counter.n_pot_cache_misses++;
	}

	return cl_changed;
}

static void
flush_cl(const void *addr, size_t len)
{
	unsigned flushed = cl_flushed(addr, len, FLUSH_ALIGN);
	ops_counter.n_cl_stores += flushed;
	ops_counter.n_pot_cache_misses += flushed;
}

static void
flush_msync(const void *addr, size_t len)
{
	unsigned flushed = cl_flushed(addr, len, Pagesize);
	ops_counter.n_cl_stores += flushed;
	ops_counter.n_pot_cache_misses += flushed;
}

FUNC_MOCK(pmem_persist, void, const void *addr, size_t len)
	FUNC_MOCK_RUN_DEFAULT {
		ops_counter.n_pmem_persist++;
		flush_cl(addr, len);
		ops_counter.n_drain++;

		_FUNC_REAL(pmem_persist)(addr, len);
	}
FUNC_MOCK_END

FUNC_MOCK(pmem_msync, int, const void *addr, size_t len)
	FUNC_MOCK_RUN_DEFAULT {
		ops_counter.n_pmem_msync++;
		flush_msync(addr, len);
		ops_counter.n_drain++;

		return _FUNC_REAL(pmem_msync)(addr, len);
	}
FUNC_MOCK_END

FUNC_MOCK(pmem_flush, void, const void *addr, size_t len)
	FUNC_MOCK_RUN_DEFAULT {
		ops_counter.n_pmem_flush++;
		flush_cl(addr, len);
		_FUNC_REAL(pmem_flush)(addr, len);
	}
FUNC_MOCK_END

FUNC_MOCK(pmem_drain, void, void)
	FUNC_MOCK_RUN_DEFAULT {
		ops_counter.n_pmem_drain++;
		ops_counter.n_drain++;
		_FUNC_REAL(pmem_drain)();
	}
FUNC_MOCK_END

static void
memcpy_nodrain_count(void *dest, const void *src, size_t len, unsigned flags)
{
	unsigned cl_stores = bulk_cl_changed(dest, len, flags);
	if (!(flags & PMEM_F_MEM_NOFLUSH))
		ops_counter.n_flush_from_pmem_memcpy += cl_stores;
	ops_counter.n_cl_stores += cl_stores;
}

static void
memcpy_persist_count(void *dest, const void *src, size_t len, unsigned flags)
{
	memcpy_nodrain_count(dest, src, len, flags);

	ops_counter.n_drain_from_pmem_memcpy++;
	ops_counter.n_drain++;
}

FUNC_MOCK(pmem_memcpy_persist, void *, void *dest, const void *src, size_t len)
	FUNC_MOCK_RUN_DEFAULT {
		memcpy_persist_count(dest, src, len, 0);

		return _FUNC_REAL(pmem_memcpy_persist)(dest, src, len);
	}
FUNC_MOCK_END

FUNC_MOCK(pmem_memcpy_nodrain, void *, void *dest, const void *src, size_t len)
	FUNC_MOCK_RUN_DEFAULT {
		memcpy_nodrain_count(dest, src, len, 0);

		return _FUNC_REAL(pmem_memcpy_nodrain)(dest, src, len);
	}
FUNC_MOCK_END

static unsigned
sanitize_flags(unsigned flags)
{
	if (flags & PMEM_F_MEM_NOFLUSH) {
		/* NOFLUSH implies NODRAIN */
		flags |= PMEM_F_MEM_NODRAIN;
	}

	return flags;
}

FUNC_MOCK(pmem_memcpy, void *, void *dest, const void *src, size_t len,
		unsigned flags)
	FUNC_MOCK_RUN_DEFAULT {
		flags = sanitize_flags(flags);

		if (flags & PMEM_F_MEM_NODRAIN)
			memcpy_nodrain_count(dest, src, len, flags);
		else
			memcpy_persist_count(dest, src, len, flags);

		return _FUNC_REAL(pmem_memcpy)(dest, src, len, flags);
	}
FUNC_MOCK_END

FUNC_MOCK(pmem_memmove_persist, void *, void *dest, const void *src, size_t len)
	FUNC_MOCK_RUN_DEFAULT {
		memcpy_persist_count(dest, src, len, 0);

		return _FUNC_REAL(pmem_memmove_persist)(dest, src, len);
	}
FUNC_MOCK_END

FUNC_MOCK(pmem_memmove_nodrain, void *, void *dest, const void *src, size_t len)
	FUNC_MOCK_RUN_DEFAULT {
		memcpy_nodrain_count(dest, src, len, 0);

		return _FUNC_REAL(pmem_memmove_nodrain)(dest, src, len);
	}
FUNC_MOCK_END

FUNC_MOCK(pmem_memmove, void *, void *dest, const void *src, size_t len,
		unsigned flags)
	FUNC_MOCK_RUN_DEFAULT {
		flags = sanitize_flags(flags);

		if (flags & PMEM_F_MEM_NODRAIN)
			memcpy_nodrain_count(dest, src, len, flags);
		else
			memcpy_persist_count(dest, src, len, flags);

		return _FUNC_REAL(pmem_memmove)(dest, src, len, flags);
	}
FUNC_MOCK_END

static void
memset_nodrain_count(void *dest, size_t len, unsigned flags)
{
	unsigned cl_set = bulk_cl_changed(dest, len, flags);
	if (!(flags & PMEM_F_MEM_NOFLUSH))
		ops_counter.n_flush_from_pmem_memset += cl_set;
	ops_counter.n_cl_stores += cl_set;
}

static void
memset_persist_count(void *dest, size_t len, unsigned flags)
{
	memset_nodrain_count(dest, len, flags);

	ops_counter.n_drain_from_pmem_memset++;
	ops_counter.n_drain++;
}

FUNC_MOCK(pmem_memset_persist, void *, void *dest, int c, size_t len)
	FUNC_MOCK_RUN_DEFAULT {
		memset_persist_count(dest, len, 0);

		return _FUNC_REAL(pmem_memset_persist)(dest, c, len);
	}
FUNC_MOCK_END

FUNC_MOCK(pmem_memset_nodrain, void *, void *dest, int c, size_t len)
	FUNC_MOCK_RUN_DEFAULT {
		memset_nodrain_count(dest, len, 0);

		return _FUNC_REAL(pmem_memset_nodrain)(dest, c, len);
	}
FUNC_MOCK_END

FUNC_MOCK(pmem_memset, void *, void *dest, int c, size_t len, unsigned flags)
	FUNC_MOCK_RUN_DEFAULT {
		flags = sanitize_flags(flags);

		if (flags & PMEM_F_MEM_NODRAIN)
			memset_nodrain_count(dest, len, flags);
		else
			memset_persist_count(dest, len, flags);

		return _FUNC_REAL(pmem_memset)(dest, c, len, flags);
	}
FUNC_MOCK_END

/*
 * reset_counters -- zero all counters
 */
static void
reset_counters(void)
{
	memset(&ops_counter, 0, sizeof(ops_counter));
}

/*
 * print_reset_counters -- print and then zero all counters
 */
static void
print_reset_counters(const char *task, unsigned tx)
{
#define CNT(name) (ops_counter.name - tx * tx_counter.name)
	UT_OUT(
		"%-14s %-7d %-10d %-12d %-10d %-10d %-10d %-15d %-17d %-15d %-17d %-23d",
		task,
		CNT(n_cl_stores),
		CNT(n_drain),
		CNT(n_pmem_persist),
		CNT(n_pmem_msync),
		CNT(n_pmem_flush),
		CNT(n_pmem_drain),
		CNT(n_flush_from_pmem_memcpy),
		CNT(n_drain_from_pmem_memcpy),
		CNT(n_flush_from_pmem_memset),
		CNT(n_drain_from_pmem_memset),
		CNT(n_pot_cache_misses));
#undef CNT
	reset_counters();
}

#define LARGE_SNAPSHOT ((1 << 10) * 10)

struct foo_large {
	uint8_t snapshot[LARGE_SNAPSHOT];
};

struct foo {
	int val;
	uint64_t dest;

	PMEMoid bar;
	PMEMoid bar2;
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_persist_count");

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(path, "persist_count",
			PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	UT_OUT(
		"%-14s %-7s %-10s %-12s %-10s %-10s %-10s %-15s %-17s %-15s %-17s %-23s",
		"task",
		"cl(all)",
		"drain(all)",
		"pmem_persist",
		"pmem_msync",
		"pmem_flush",
		"pmem_drain",
		"pmem_memcpy_cls",
		"pmem_memcpy_drain",
		"pmem_memset_cls",
		"pmem_memset_drain",
		"potential_cache_misses");

	print_reset_counters("pool_create", 0);

	/* allocate one structure to create a run */
	pmemobj_alloc(pop, NULL, sizeof(struct foo), 0, NULL, NULL);
	reset_counters();

	PMEMoid root = pmemobj_root(pop, sizeof(struct foo));
	UT_ASSERT(!OID_IS_NULL(root));
	print_reset_counters("root_alloc", 0);

	PMEMoid oid;
	int ret = pmemobj_alloc(pop, &oid, sizeof(struct foo), 0, NULL, NULL);
	UT_ASSERTeq(ret, 0);
	print_reset_counters("atomic_alloc", 0);

	pmemobj_free(&oid);
	print_reset_counters("atomic_free", 0);

	struct foo *f = pmemobj_direct(root);

	TX_BEGIN(pop) {
	} TX_END
	memcpy(&tx_counter, &ops_counter, sizeof(ops_counter));
	print_reset_counters("tx_begin_end", 0);

	TX_BEGIN(pop) {
		f->bar = pmemobj_tx_alloc(sizeof(struct foo), 0);
		UT_ASSERT(!OID_IS_NULL(f->bar));
	} TX_END
	print_reset_counters("tx_alloc", 1);

	TX_BEGIN(pop) {
		f->bar2 = pmemobj_tx_alloc(sizeof(struct foo), 0);
		UT_ASSERT(!OID_IS_NULL(f->bar2));
	} TX_END
	print_reset_counters("tx_alloc_next", 1);

	TX_BEGIN(pop) {
		pmemobj_tx_free(f->bar);
	} TX_END
	print_reset_counters("tx_free", 1);

	TX_BEGIN(pop) {
		pmemobj_tx_free(f->bar2);
	} TX_END
	print_reset_counters("tx_free_next", 1);

	TX_BEGIN(pop) {
		pmemobj_tx_xadd_range_direct(&f->val, sizeof(f->val),
			POBJ_XADD_NO_FLUSH);
	} TX_END
	print_reset_counters("tx_add", 1);

	TX_BEGIN(pop) {
		pmemobj_tx_xadd_range_direct(&f->val, sizeof(f->val),
			POBJ_XADD_NO_FLUSH);
	} TX_END
	print_reset_counters("tx_add_next", 1);

	PMEMoid large_foo;
	pmemobj_zalloc(pop, &large_foo, sizeof(struct foo_large), 0);
	UT_ASSERT(!OID_IS_NULL(large_foo));
	reset_counters();

	struct foo_large *flarge = pmemobj_direct(large_foo);

	TX_BEGIN(pop) {
		pmemobj_tx_xadd_range_direct(&flarge->snapshot,
			sizeof(flarge->snapshot),
			POBJ_XADD_NO_FLUSH);
	} TX_END
	print_reset_counters("tx_add_large", 1);

	TX_BEGIN(pop) {
		pmemobj_tx_xadd_range_direct(&flarge->snapshot,
			sizeof(flarge->snapshot),
			POBJ_XADD_NO_FLUSH);
	} TX_END
	print_reset_counters("tx_add_lnext", 1);

	pmalloc(pop, &f->dest, sizeof(f->val), 0, 0);
	print_reset_counters("pmalloc", 0);

	pfree(pop, &f->dest);
	print_reset_counters("pfree", 0);

	uint64_t stack_var;
	pmalloc(pop, &stack_var, sizeof(f->val), 0, 0);
	print_reset_counters("pmalloc_stack", 0);

	pfree(pop, &stack_var);
	print_reset_counters("pfree_stack", 0);

	pmemobj_close(pop);

	DONE(NULL);
}

#ifdef _MSC_VER
/*
 * Since libpmemobj is linked statically, we need to invoke its ctor/dtor.
 */
MSVC_CONSTR(libpmemobj_init)
MSVC_DESTR(libpmemobj_fini)
#endif
