/*
 * Copyright 2015-2018, Intel Corporation
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
 * obj_heap.c -- unit test for heap
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

	run->block_size = 1024;
	memset(run->bitmap, 0xFF, sizeof(run->bitmap));
	UT_ASSERTeq(score % 64, 0);
	score /= 64;

	for (; score > 0; --score) {
		run->bitmap[score] = 0;
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

	run->block_size = 1024;
	memset(run->bitmap, 0xFF, sizeof(run->bitmap));

	/* the biggest block is 10 bits */
	run->bitmap[3] =
	0b1000001110111000111111110000111111000000000011111111110000000011;
}

static void
test_alloc_class_bitmap_correctness(void)
{
	struct alloc_class_run_proto proto;
	alloc_class_generate_run_proto(&proto, RUNSIZE / 10, 1);
	/* 54 set (not available for allocations), and 10 clear (available) */
	uint64_t bitmap_lastval =
	0b1111111111111111111111111111111111111111111111111111110000000000;

	UT_ASSERTeq(proto.bitmap_lastval, bitmap_lastval);
}

static void
test_container(struct block_container *bc, struct palloc_heap *heap)
{
	UT_ASSERTne(bc, NULL);

	struct memory_block a = {1, 0, 1, 0};
	struct memory_block b = {2, 0, 2, 0};
	struct memory_block c = {3, 0, 3, 0};
	struct memory_block d = {5, 0, 5, 0};
	init_run_with_score(heap->layout, 1, 128);
	init_run_with_score(heap->layout, 2, 128);
	init_run_with_score(heap->layout, 3, 128);
	init_run_with_score(heap->layout, 5, 128);
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
test_heap(void)
{
	struct mock_pop *mpop = MMAP_ANON_ALIGNED(MOCK_POOL_SIZE,
		Ut_mmap_align);
	PMEMobjpool *pop = &mpop->p;
	memset(pop, 0, MOCK_POOL_SIZE);
	pop->heap_offset = (uint64_t)((uint64_t)&mpop->heap - (uint64_t)mpop);
	pop->p_ops.persist = obj_heap_persist;
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

	test_alloc_class_bitmap_correctness();

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

	struct bucket *b_def = heap_bucket_acquire_by_id(heap,
		DEFAULT_ALLOC_CLASS_ID);

	for (int i = 0; i < MAX_BLOCKS; ++i) {
		heap_get_bestfit_block(heap, b_def, &blocks[i]);
		UT_ASSERT(blocks[i].block_off == 0);
	}
	heap_bucket_release(heap, b_def);

	struct memory_block old_run = {0, 0, 1, 0};
	struct memory_block new_run = {0, 0, 0, 0};
	struct alloc_class *c_run = heap_get_best_class(heap, 1024);
	struct bucket *b_run = heap_bucket_acquire(heap, c_run);

	/*
	 * Allocate blocks from a run until one run is exhausted.
	 */
	UT_ASSERTne(heap_get_bestfit_block(heap, b_run, &old_run), ENOMEM);
	int *nresv = bucket_current_resvp(b_run);

	do {
		new_run.chunk_id = 0;
		new_run.block_off = 0;
		new_run.size_idx = 1;
		UT_ASSERTne(heap_get_bestfit_block(heap, b_run, &new_run),
			ENOMEM);
		UT_ASSERTne(new_run.size_idx, 0);
		*nresv = 0;
	} while (old_run.block_off != new_run.block_off);
	*nresv = 0;

	heap_bucket_release(heap, b_run);

	stats_delete(pop, s);
	UT_ASSERT(heap_check(heap_start, heap_size) == 0);
	heap_cleanup(heap);
	UT_ASSERT(heap->rt == NULL);

	FREE(pop->set);
	MUNMAP_ANON_ALIGNED(mpop, MOCK_POOL_SIZE);
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
	struct bucket *b = heap_bucket_acquire_by_id(heap,
		DEFAULT_ALLOC_CLASS_ID);
	UT_ASSERT(heap_get_bestfit_block(heap, b, &m) == 0);
	heap_bucket_release(heap, b);

	int ret;

	struct recycler *r = recycler_new(&pop->heap, 10000 /* never recalc */);
	UT_ASSERTne(r, NULL);

	init_run_with_score(pop->heap.layout, 0, 64);
	init_run_with_score(pop->heap.layout, 1, 128);

	init_run_with_score(pop->heap.layout, 15, 0);

	struct memory_block mrun = {0, 0, 1, 0};
	struct memory_block mrun2 = {1, 0, 1, 0};

	memblock_rebuild_state(&pop->heap, &mrun);
	memblock_rebuild_state(&pop->heap, &mrun2);

	ret = recycler_put(r, &mrun,
		recycler_calc_score(&pop->heap, &mrun, NULL));
	UT_ASSERTeq(ret, 0);
	ret = recycler_put(r, &mrun2,
		recycler_calc_score(&pop->heap, &mrun2, NULL));
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

	init_run_with_score(pop->heap.layout, 7, 256);
	init_run_with_score(pop->heap.layout, 2, 64);
	init_run_with_score(pop->heap.layout, 5, 512);
	init_run_with_score(pop->heap.layout, 10, 128);

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

	ret = recycler_put(r, &mrun,
		recycler_calc_score(&pop->heap, &mrun, NULL));
	UT_ASSERTeq(ret, 0);
	ret = recycler_put(r, &mrun2,
		recycler_calc_score(&pop->heap, &mrun2, NULL));
	UT_ASSERTeq(ret, 0);
	ret = recycler_put(r, &mrun3,
		recycler_calc_score(&pop->heap, &mrun3, NULL));
	UT_ASSERTeq(ret, 0);
	ret = recycler_put(r, &mrun4,
		recycler_calc_score(&pop->heap, &mrun4, NULL));
	UT_ASSERTeq(ret, 0);

	ret = recycler_get(r, &mrun2_ret);
	UT_ASSERTeq(ret, 0);
	ret = recycler_get(r, &mrun4_ret);
	UT_ASSERTeq(ret, 0);
	ret = recycler_get(r, &mrun_ret);
	UT_ASSERTeq(ret, 0);
	ret = recycler_get(r, &mrun3_ret);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(mrun.chunk_id, mrun_ret.chunk_id);
	UT_ASSERTeq(mrun2.chunk_id, mrun2_ret.chunk_id);
	UT_ASSERTeq(mrun3.chunk_id, mrun3_ret.chunk_id);
	UT_ASSERTeq(mrun4.chunk_id, mrun4_ret.chunk_id);

	init_run_with_max_block(pop->heap.layout, 1);
	struct memory_block mrun5 = {1, 0, 1, 0};
	memblock_rebuild_state(&pop->heap, &mrun5);

	ret = recycler_put(r, &mrun5,
		recycler_calc_score(&pop->heap, &mrun5, NULL));
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

	test_heap();
	test_recycler();

	DONE(NULL);
}
