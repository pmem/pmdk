// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2021, Intel Corporation */

/*
 * obj_heap.c -- unit test for heap
 *
 * operations are: 't', 'b', 'r', 'c', 'h', 'a', 'n', 's'
 * t: do test_heap, test_recycler
 * b: do fault_injection in function container_new_ravl
 * r: do fault_injection in function recycler_new
 * c: do fault_injection in function container_new_seglists
 * h: do fault_injection in function heap_boot
 * a: do fault_injection in function alloc_class_new
 * n: do fault_injection in function alloc_class_collection_new
 * s: do fault_injection in function stats_new
 */
#include "libpmemobj.h"
#include "palloc.h"
#include "heap.h"
#include "recycler.h"
#include "obj.h"
#include "unittest.h"
#include "util.h"
#include "container_ravl.h"
#include "container_seglists.h"
#include "container.h"
#include "alloc_class.h"
#include "valgrind_internal.h"
#include "set.h"

#define MOCK_POOL_SIZE PMEMOBJ_MIN_POOL

#define MAX_BLOCKS 3

struct mock_pop {
	PMEMobjpool p;
	void *heap;
};

static int
obj_heap_persist(void *ctx, const void *ptr, size_t sz, unsigned flags)
{
	UT_ASSERTeq(pmem_msync(ptr, sz), 0);

	return 0;
}

static int
obj_heap_flush(void *ctx, const void *ptr, size_t sz, unsigned flags)
{
	UT_ASSERTeq(pmem_msync(ptr, sz), 0);

	return 0;
}

static void
obj_heap_drain(void *ctx)
{
}

static void *
obj_heap_memset(void *ctx, void *ptr, int c, size_t sz, unsigned flags)
{
	memset(ptr, c, sz);
	UT_ASSERTeq(pmem_msync(ptr, sz), 0);
	return ptr;
}

static void
init_run_with_score(struct heap_layout *l, uint32_t chunk_id, int score)
{
	l->zone0.chunk_headers[chunk_id].size_idx = 1;
	l->zone0.chunk_headers[chunk_id].type = CHUNK_TYPE_RUN;
	l->zone0.chunk_headers[chunk_id].flags = 0;

	struct chunk_run *run = (struct chunk_run *)
		&l->zone0.chunks[chunk_id];
	VALGRIND_DO_MAKE_MEM_UNDEFINED(run, sizeof(*run));

	run->hdr.alignment = 0;
	run->hdr.block_size = 1024;
	memset(run->content, 0xFF, RUN_DEFAULT_BITMAP_SIZE);
	UT_ASSERTeq(score % 64, 0);
	score /= 64;

	uint64_t *bitmap = (uint64_t *)run->content;
	for (; score >= 0; --score) {
		bitmap[score] = 0;
	}
}

static void
init_run_with_max_block(struct heap_layout *l, uint32_t chunk_id)
{
	l->zone0.chunk_headers[chunk_id].size_idx = 1;
	l->zone0.chunk_headers[chunk_id].type = CHUNK_TYPE_RUN;
	l->zone0.chunk_headers[chunk_id].flags = 0;

	struct chunk_run *run = (struct chunk_run *)
		&l->zone0.chunks[chunk_id];
	VALGRIND_DO_MAKE_MEM_UNDEFINED(run, sizeof(*run));

	uint64_t *bitmap = (uint64_t *)run->content;
	run->hdr.block_size = 1024;
	run->hdr.alignment = 0;
	memset(bitmap, 0xFF, RUN_DEFAULT_BITMAP_SIZE);

	/* the biggest block is 10 bits */
	bitmap[3] =
	0b1000001110111000111111110000111111000000000011111111110000000011;
}

static void
test_container(struct block_container *bc, struct palloc_heap *heap)
{
	UT_ASSERTne(bc, NULL);

	struct memory_block a = {1, 0, 1, 4};
	struct memory_block b = {1, 0, 2, 8};
	struct memory_block c = {1, 0, 3, 16};
	struct memory_block d = {1, 0, 5, 32};

	init_run_with_score(heap->layout, 1, 128);
	memblock_rebuild_state(heap, &a);
	memblock_rebuild_state(heap, &b);
	memblock_rebuild_state(heap, &c);
	memblock_rebuild_state(heap, &d);

	int ret;
	ret = bc->c_ops->insert(bc, &a);
	UT_ASSERTeq(ret, 0);

	ret = bc->c_ops->insert(bc, &b);
	UT_ASSERTeq(ret, 0);

	ret = bc->c_ops->insert(bc, &c);
	UT_ASSERTeq(ret, 0);

	ret = bc->c_ops->insert(bc, &d);
	UT_ASSERTeq(ret, 0);

	struct memory_block invalid_ret = {0, 0, 6, 0};
	ret = bc->c_ops->get_rm_bestfit(bc, &invalid_ret);
	UT_ASSERTeq(ret, ENOMEM);

	struct memory_block b_ret = {0, 0, 2, 0};
	ret = bc->c_ops->get_rm_bestfit(bc, &b_ret);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(b_ret.chunk_id, b.chunk_id);

	struct memory_block a_ret = {0, 0, 1, 0};
	ret = bc->c_ops->get_rm_bestfit(bc, &a_ret);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(a_ret.chunk_id, a.chunk_id);

	struct memory_block c_ret = {0, 0, 3, 0};
	ret = bc->c_ops->get_rm_bestfit(bc, &c_ret);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(c_ret.chunk_id, c.chunk_id);

	struct memory_block d_ret = {0, 0, 4, 0}; /* less one than target */
	ret = bc->c_ops->get_rm_bestfit(bc, &d_ret);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(d_ret.chunk_id, d.chunk_id);

	ret = bc->c_ops->get_rm_bestfit(bc, &c_ret);
	UT_ASSERTeq(ret, ENOMEM);

	ret = bc->c_ops->insert(bc, &a);
	UT_ASSERTeq(ret, 0);

	ret = bc->c_ops->insert(bc, &b);
	UT_ASSERTeq(ret, 0);

	ret = bc->c_ops->insert(bc, &c);
	UT_ASSERTeq(ret, 0);

	bc->c_ops->rm_all(bc);
	ret = bc->c_ops->is_empty(bc);
	UT_ASSERTeq(ret, 1);

	ret = bc->c_ops->get_rm_bestfit(bc, &c_ret);
	UT_ASSERTeq(ret, ENOMEM);

	bc->c_ops->destroy(bc);
}

static void
do_fault_injection_new_ravl()
{
	if (!pmemobj_fault_injection_enabled())
		return;

	pmemobj_inject_fault_at(PMEM_MALLOC, 1, "container_new_ravl");

	struct block_container *bc = container_new_ravl(NULL);
	UT_ASSERTeq(bc, NULL);
	UT_ASSERTeq(errno, ENOMEM);
}

static void
do_fault_injection_new_seglists()
{
	if (!pmemobj_fault_injection_enabled())
		return;

	pmemobj_inject_fault_at(PMEM_MALLOC, 1, "container_new_seglists");

	struct block_container *bc = container_new_seglists(NULL);
	UT_ASSERTeq(bc, NULL);
	UT_ASSERTeq(errno, ENOMEM);
}

static void
do_fault_injection_heap_boot()
{
	if (!pmemobj_fault_injection_enabled())
		return;

	struct mock_pop *mpop = MMAP_ANON_ALIGNED(MOCK_POOL_SIZE,
			Ut_mmap_align);
	PMEMobjpool *pop = &mpop->p;
	pop->p_ops.persist = obj_heap_persist;
	uint64_t heap_size = MOCK_POOL_SIZE - sizeof(PMEMobjpool);
	struct pmem_ops *p_ops = &pop->p_ops;

	pmemobj_inject_fault_at(PMEM_MALLOC, 1, "heap_boot");

	int r = heap_boot(NULL, NULL, heap_size, &pop->heap_size, NULL, p_ops,
			NULL, NULL);
	UT_ASSERTne(r, 0);
	UT_ASSERTeq(errno, ENOMEM);
}

static void
do_fault_injection_recycler()
{
	if (!pmemobj_fault_injection_enabled())
		return;

	pmemobj_inject_fault_at(PMEM_MALLOC, 1, "recycler_new");

	size_t active_arenas = 1;
	struct recycler *r = recycler_new(NULL, 0, &active_arenas);
	UT_ASSERTeq(r, NULL);
	UT_ASSERTeq(errno, ENOMEM);
}

static void
do_fault_injection_class_new(int i)
{
	if (!pmemobj_fault_injection_enabled())
		return;

	pmemobj_inject_fault_at(PMEM_MALLOC, i, "alloc_class_new");

	struct alloc_class_collection *c = alloc_class_collection_new();
	UT_ASSERTeq(c, NULL);
	UT_ASSERTeq(errno, ENOMEM);
}

static void
do_fault_injection_class_collection_new()
{
	if (!pmemobj_fault_injection_enabled())
		return;

	pmemobj_inject_fault_at(PMEM_MALLOC, 1, "alloc_class_collection_new");

	struct alloc_class_collection *c = alloc_class_collection_new();
	UT_ASSERTeq(c, NULL);
	UT_ASSERTeq(errno, ENOMEM);
}

static void
do_fault_injection_stats()
{
	if (!pmemobj_fault_injection_enabled())
		return;

	pmemobj_inject_fault_at(PMEM_MALLOC, 1, "stats_new");
	struct stats *s = stats_new(NULL);
	UT_ASSERTeq(s, NULL);
	UT_ASSERTeq(errno, ENOMEM);
}

static void
test_heap(void)
{
	struct mock_pop *mpop = MMAP_ANON_ALIGNED(MOCK_POOL_SIZE,
		Ut_mmap_align);
	PMEMobjpool *pop = &mpop->p;
	memset(pop, 0, MOCK_POOL_SIZE);
	pop->heap_offset = (uint64_t)((uint64_t)&mpop->heap - (uint64_t)mpop);
	pop->p_ops.persist = obj_heap_persist;
	pop->p_ops.flush = obj_heap_flush;
	pop->p_ops.drain = obj_heap_drain;
	pop->p_ops.memset = obj_heap_memset;
	pop->p_ops.base = pop;
	pop->set = MALLOC(sizeof(*(pop->set)));
	pop->set->options = 0;
	pop->set->directory_based = 0;

	struct stats *s = stats_new(pop);
	UT_ASSERTne(s, NULL);

	void *heap_start = (char *)pop + pop->heap_offset;
	uint64_t heap_size = MOCK_POOL_SIZE - sizeof(PMEMobjpool);
	struct palloc_heap *heap = &pop->heap;
	struct pmem_ops *p_ops = &pop->p_ops;

	UT_ASSERT(heap_check(heap_start, heap_size) != 0);
	UT_ASSERT(heap_init(heap_start, heap_size,
		&pop->heap_size, p_ops) == 0);
	UT_ASSERT(heap_boot(heap, heap_start, heap_size,
		&pop->heap_size,
		pop, p_ops, s, pop->set) == 0);
	UT_ASSERT(heap_buckets_init(heap) == 0);
	UT_ASSERT(pop->heap.rt != NULL);

	test_container((struct block_container *)container_new_ravl(heap),
		heap);

	test_container((struct block_container *)container_new_seglists(heap),
		heap);

	struct alloc_class *c_small = heap_get_best_class(heap, 1);
	struct alloc_class *c_big = heap_get_best_class(heap, 2048);

	UT_ASSERT(c_small->unit_size < c_big->unit_size);

	/* new small buckets should be empty */
	UT_ASSERT(c_big->type == CLASS_RUN);

	struct memory_block blocks[MAX_BLOCKS] = {
		{0, 0, 1, 0},
		{0, 0, 1, 0},
		{0, 0, 1, 0}
	};

	struct bucket *b_def = heap_bucket_acquire(heap,
		DEFAULT_ALLOC_CLASS_ID, HEAP_ARENA_PER_THREAD);

	for (int i = 0; i < MAX_BLOCKS; ++i) {
		heap_get_bestfit_block(heap, b_def, &blocks[i]);
		UT_ASSERT(blocks[i].block_off == 0);
	}
	heap_bucket_release(heap, b_def);

	struct memory_block old_run = {0, 0, 1, 0};
	struct memory_block new_run = {0, 0, 0, 0};
	struct alloc_class *c_run = heap_get_best_class(heap, 1024);
	struct bucket *b_run = heap_bucket_acquire(heap, c_run->id,
			HEAP_ARENA_PER_THREAD);

	/*
	 * Allocate blocks from a run until one run is exhausted.
	 */
	UT_ASSERTne(heap_get_bestfit_block(heap, b_run, &old_run), ENOMEM);

	do {
		new_run.chunk_id = 0;
		new_run.block_off = 0;
		new_run.size_idx = 1;
		UT_ASSERTne(heap_get_bestfit_block(heap, b_run, &new_run),
			ENOMEM);
		UT_ASSERTne(new_run.size_idx, 0);
	} while (old_run.block_off != new_run.block_off);

	heap_bucket_release(heap, b_run);

	stats_delete(pop, s);
	UT_ASSERT(heap_check(heap_start, heap_size) == 0);
	heap_cleanup(heap);
	UT_ASSERT(heap->rt == NULL);

	FREE(pop->set);
	MUNMAP_ANON_ALIGNED(mpop, MOCK_POOL_SIZE);
}

/*
 * test_heap_with_size -- tests scenarios with not-nicely aligned sizes
 */
static void
test_heap_with_size()
{
	/*
	 * To trigger bug with incorrect metadata alignment we need to
	 * use a size that uses exactly the size used in bugged zone size
	 * calculations.
	 */
	size_t size = PMEMOBJ_MIN_POOL + sizeof(struct zone_header) +
		sizeof(struct chunk_header) * MAX_CHUNK +
		sizeof(PMEMobjpool);

	struct mock_pop *mpop = MMAP_ANON_ALIGNED(size,
		Ut_mmap_align);
	PMEMobjpool *pop = &mpop->p;
	memset(pop, 0, size);
	pop->heap_offset = (uint64_t)((uint64_t)&mpop->heap - (uint64_t)mpop);
	pop->p_ops.persist = obj_heap_persist;
	pop->p_ops.flush = obj_heap_flush;
	pop->p_ops.drain = obj_heap_drain;
	pop->p_ops.memset = obj_heap_memset;
	pop->p_ops.base = pop;
	pop->set = MALLOC(sizeof(*(pop->set)));
	pop->set->options = 0;
	pop->set->directory_based = 0;

	void *heap_start = (char *)pop + pop->heap_offset;
	uint64_t heap_size = size - sizeof(PMEMobjpool);
	struct palloc_heap *heap = &pop->heap;
	struct pmem_ops *p_ops = &pop->p_ops;

	UT_ASSERT(heap_check(heap_start, heap_size) != 0);
	UT_ASSERT(heap_init(heap_start, heap_size,
		&pop->heap_size, p_ops) == 0);
	UT_ASSERT(heap_boot(heap, heap_start, heap_size,
		&pop->heap_size,
		pop, p_ops, NULL, pop->set) == 0);
	UT_ASSERT(heap_buckets_init(heap) == 0);
	UT_ASSERT(pop->heap.rt != NULL);

	struct bucket *b_def = heap_bucket_acquire(heap,
		DEFAULT_ALLOC_CLASS_ID, HEAP_ARENA_PER_THREAD);

	struct memory_block mb;
	mb.size_idx = 1;
	while (heap_get_bestfit_block(heap, b_def, &mb) == 0)
		;

	/* mb should now be the last chunk in the heap */
	char *ptr = mb.m_ops->get_real_data(&mb);
	size_t s = mb.m_ops->get_real_size(&mb);

	/* last chunk should be within the heap and accessible */
	UT_ASSERT((size_t)ptr + s <= (size_t)mpop + size);

	VALGRIND_DO_MAKE_MEM_DEFINED(ptr, s);
	memset(ptr, 0xc, s);

	heap_bucket_release(heap, b_def);

	UT_ASSERT(heap_check(heap_start, heap_size) == 0);
	heap_cleanup(heap);
	UT_ASSERT(heap->rt == NULL);

	FREE(pop->set);
	MUNMAP_ANON_ALIGNED(mpop, size);
}

static void
test_recycler(void)
{
	struct mock_pop *mpop = MMAP_ANON_ALIGNED(MOCK_POOL_SIZE,
		Ut_mmap_align);
	PMEMobjpool *pop = &mpop->p;
	memset(pop, 0, MOCK_POOL_SIZE);
	pop->heap_offset = (uint64_t)((uint64_t)&mpop->heap - (uint64_t)mpop);
	pop->p_ops.persist = obj_heap_persist;
	pop->p_ops.flush = obj_heap_flush;
	pop->p_ops.drain = obj_heap_drain;
	pop->p_ops.memset = obj_heap_memset;
	pop->p_ops.base = pop;
	pop->set = MALLOC(sizeof(*(pop->set)));
	pop->set->options = 0;
	pop->set->directory_based = 0;

	void *heap_start = (char *)pop + pop->heap_offset;
	uint64_t heap_size = MOCK_POOL_SIZE - sizeof(PMEMobjpool);
	struct palloc_heap *heap = &pop->heap;
	struct pmem_ops *p_ops = &pop->p_ops;

	struct stats *s = stats_new(pop);
	UT_ASSERTne(s, NULL);

	UT_ASSERT(heap_check(heap_start, heap_size) != 0);
	UT_ASSERT(heap_init(heap_start, heap_size,
		&pop->heap_size, p_ops) == 0);
	UT_ASSERT(heap_boot(heap, heap_start, heap_size,
		&pop->heap_size,
		pop, p_ops, s, pop->set) == 0);
	UT_ASSERT(heap_buckets_init(heap) == 0);
	UT_ASSERT(pop->heap.rt != NULL);

	/* trigger heap bucket populate */
	struct memory_block m = MEMORY_BLOCK_NONE;
	m.size_idx = 1;
	struct bucket *b = heap_bucket_acquire(heap,
		DEFAULT_ALLOC_CLASS_ID,
		HEAP_ARENA_PER_THREAD);
	UT_ASSERT(heap_get_bestfit_block(heap, b, &m) == 0);
	heap_bucket_release(heap, b);

	int ret;

	size_t active_arenas = 1;
	struct recycler *r = recycler_new(&pop->heap, 10000 /* never recalc */,
		&active_arenas);

	UT_ASSERTne(r, NULL);

	init_run_with_score(pop->heap.layout, 0, 64);
	init_run_with_score(pop->heap.layout, 1, 128);

	init_run_with_score(pop->heap.layout, 15, 0);

	struct memory_block mrun = {0, 0, 1, 0};
	struct memory_block mrun2 = {1, 0, 1, 0};

	memblock_rebuild_state(&pop->heap, &mrun);
	memblock_rebuild_state(&pop->heap, &mrun2);

	ret = recycler_put(r,
		recycler_element_new(&pop->heap, &mrun));
	UT_ASSERTeq(ret, 0);
	ret = recycler_put(r,
		recycler_element_new(&pop->heap, &mrun2));
	UT_ASSERTeq(ret, 0);

	struct memory_block mrun_ret = MEMORY_BLOCK_NONE;
	mrun_ret.size_idx = 1;
	struct memory_block mrun2_ret = MEMORY_BLOCK_NONE;
	mrun2_ret.size_idx = 1;

	ret = recycler_get(r, &mrun_ret);
	UT_ASSERTeq(ret, 0);
	ret = recycler_get(r, &mrun2_ret);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(mrun2.chunk_id, mrun2_ret.chunk_id);
	UT_ASSERTeq(mrun.chunk_id, mrun_ret.chunk_id);

	init_run_with_score(pop->heap.layout, 7, 64);
	init_run_with_score(pop->heap.layout, 2, 128);
	init_run_with_score(pop->heap.layout, 5, 192);
	init_run_with_score(pop->heap.layout, 10, 256);

	mrun.chunk_id = 7;
	mrun2.chunk_id = 2;
	struct memory_block mrun3 = {5, 0, 1, 0};
	struct memory_block mrun4 = {10, 0, 1, 0};
	memblock_rebuild_state(&pop->heap, &mrun3);
	memblock_rebuild_state(&pop->heap, &mrun4);

	mrun_ret.size_idx = 1;
	mrun2_ret.size_idx = 1;
	struct memory_block mrun3_ret = MEMORY_BLOCK_NONE;
	mrun3_ret.size_idx = 1;
	struct memory_block mrun4_ret = MEMORY_BLOCK_NONE;
	mrun4_ret.size_idx = 1;

	ret = recycler_put(r,
		recycler_element_new(&pop->heap, &mrun));
	UT_ASSERTeq(ret, 0);
	ret = recycler_put(r,
		recycler_element_new(&pop->heap, &mrun2));
	UT_ASSERTeq(ret, 0);
	ret = recycler_put(r,
		recycler_element_new(&pop->heap, &mrun3));
	UT_ASSERTeq(ret, 0);
	ret = recycler_put(r,
		recycler_element_new(&pop->heap, &mrun4));
	UT_ASSERTeq(ret, 0);

	ret = recycler_get(r, &mrun_ret);
	UT_ASSERTeq(ret, 0);
	ret = recycler_get(r, &mrun2_ret);
	UT_ASSERTeq(ret, 0);
	ret = recycler_get(r, &mrun3_ret);
	UT_ASSERTeq(ret, 0);
	ret = recycler_get(r, &mrun4_ret);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(mrun.chunk_id, mrun_ret.chunk_id);
	UT_ASSERTeq(mrun2.chunk_id, mrun2_ret.chunk_id);
	UT_ASSERTeq(mrun3.chunk_id, mrun3_ret.chunk_id);
	UT_ASSERTeq(mrun4.chunk_id, mrun4_ret.chunk_id);

	init_run_with_max_block(pop->heap.layout, 1);
	struct memory_block mrun5 = {1, 0, 1, 0};
	memblock_rebuild_state(&pop->heap, &mrun5);

	ret = recycler_put(r,
		recycler_element_new(&pop->heap, &mrun5));
	UT_ASSERTeq(ret, 0);

	struct memory_block mrun5_ret = MEMORY_BLOCK_NONE;
	mrun5_ret.size_idx = 11;
	ret = recycler_get(r, &mrun5_ret);
	UT_ASSERTeq(ret, ENOMEM);

	mrun5_ret = MEMORY_BLOCK_NONE;
	mrun5_ret.size_idx = 10;
	ret = recycler_get(r, &mrun5_ret);
	UT_ASSERTeq(ret, 0);

	recycler_delete(r);

	stats_delete(pop, s);
	heap_cleanup(heap);
	UT_ASSERT(heap->rt == NULL);

	FREE(pop->set);
	MUNMAP_ANON_ALIGNED(mpop, MOCK_POOL_SIZE);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_heap");

	if (argc < 2)
		UT_FATAL("usage: %s path <t|b|r|c|h|a|n|s>", argv[0]);

	switch (argv[1][0]) {
	case 't':
		test_heap();
		test_heap_with_size();
		test_recycler();
		break;
	case 'b':
		do_fault_injection_new_ravl();
		break;
	case 'r':
		do_fault_injection_recycler();
		break;
	case 'c':
		do_fault_injection_new_seglists();
		break;
	case 'h':
		do_fault_injection_heap_boot();
		break;
	case 'a':
		/* first call alloc_class_new */
		do_fault_injection_class_new(1);
		/* second call alloc_class_new */
		do_fault_injection_class_new(2);
		break;
	case 'n':
		do_fault_injection_class_collection_new();
		break;
	case 's':
		do_fault_injection_stats();
		break;
	default:
		UT_FATAL("unknown operation");
	}

	DONE(NULL);
}
