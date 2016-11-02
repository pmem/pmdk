/*
 * Copyright 2015-2017, Intel Corporation
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

#define MOCK_POOL_SIZE PMEMOBJ_MIN_POOL

#define MAX_BLOCKS 3

#define TEST_RUN_ID 5

struct mock_pop {
	PMEMobjpool p;
	void *heap;
};

static void
obj_heap_persist(void *ctx, const void *ptr, size_t sz)
{
	UT_ASSERTeq(pmem_msync(ptr, sz), 0);
}

static void *
obj_heap_memset_persist(void *ctx, void *ptr, int c, size_t sz)
{
	memset(ptr, c, sz);
	UT_ASSERTeq(pmem_msync(ptr, sz), 0);
	return ptr;
}

static void
test_heap()
{
	struct mock_pop *mpop = MMAP_ANON_ALIGNED(MOCK_POOL_SIZE,
		Ut_mmap_align);
	PMEMobjpool *pop = &mpop->p;
	memset(pop, 0, MOCK_POOL_SIZE);
	pop->size = MOCK_POOL_SIZE;
	pop->heap_size = MOCK_POOL_SIZE - sizeof(PMEMobjpool);
	pop->heap_offset = (uint64_t)((uint64_t)&mpop->heap - (uint64_t)mpop);
	pop->p_ops.persist = obj_heap_persist;
	pop->p_ops.memset_persist = obj_heap_memset_persist;
	pop->p_ops.base = pop;
	pop->p_ops.pool_size = pop->size;

	void *heap_start = (char *)pop + pop->heap_offset;
	uint64_t heap_size = pop->heap_size;
	struct palloc_heap *heap = &pop->heap;
	struct pmem_ops *p_ops = &pop->p_ops;

	UT_ASSERT(heap_check(heap_start, heap_size) != 0);
	UT_ASSERT(heap_init(heap_start, heap_size, p_ops) == 0);
	UT_ASSERT(heap_boot(heap, heap_start, heap_size, TEST_RUN_ID,
		pop, p_ops) == 0);
	UT_ASSERT(heap_buckets_init(heap) == 0);
	UT_ASSERT(pop->heap.rt != NULL);

	struct bucket *b_small = heap_get_best_bucket(heap, 1);
	struct bucket *b_big = heap_get_best_bucket(heap, 2048);

	UT_ASSERT(b_small->unit_size < b_big->unit_size);

	struct bucket *b_def = heap_get_best_bucket(heap, CHUNKSIZE);
	UT_ASSERT(b_def->unit_size == CHUNKSIZE);

	/* new small buckets should be empty */
	UT_ASSERT(b_small->type == BUCKET_RUN);
	UT_ASSERT(b_big->type == BUCKET_RUN);

	struct memory_block blocks[MAX_BLOCKS] = {
		{0, 0, 1, 0},
		{0, 0, 1, 0},
		{0, 0, 1, 0}
	};

	for (int i = 0; i < MAX_BLOCKS; ++i) {
		heap_get_bestfit_block(heap, b_def, &blocks[i]);
		UT_ASSERT(blocks[i].block_off == 0);
	}

	struct memory_block old_run = {0, 0, 1, 0};
	struct memory_block new_run = {0, 0, 0, 0};
	struct bucket *b_run = heap_get_best_bucket(heap, 1024);

	/*
	 * Allocate blocks from a run until one run is exhausted an another is
	 * created.
	 */
	UT_ASSERTne(heap_get_bestfit_block(heap, b_run, &old_run), ENOMEM);
	UT_ASSERT(MEMBLOCK_OPS(RUN, &old_run)->is_claimed(&old_run, heap));
	do {
		new_run.chunk_id = 0;
		new_run.block_off = 0;
		new_run.size_idx = 1;
		UT_ASSERTne(heap_get_bestfit_block(heap, b_run, &new_run),
			ENOMEM);
		UT_ASSERTne(new_run.size_idx, 0);
	} while (old_run.chunk_id == new_run.chunk_id);

	/* the old block should be unclaimed now */
	UT_ASSERT(!MEMBLOCK_OPS(RUN, &old_run)->is_claimed(&old_run, heap));

	UT_ASSERT(heap_check(heap_start, heap_size) == 0);
	heap_cleanup(heap);
	UT_ASSERT(heap->rt == NULL);

	MUNMAP_ANON_ALIGNED(mpop, MOCK_POOL_SIZE);
}

static void
init_run_with_score(struct heap_layout *l, uint32_t chunk_id, int score)
{
	l->zone0.chunk_headers[chunk_id].size_idx = 1;
	l->zone0.chunk_headers[chunk_id].type = CHUNK_TYPE_RUN;

	struct chunk_run *run = (struct chunk_run *)
		&l->zone0.chunks[chunk_id];

	run->block_size = 1024;
	memset(run->bitmap, 0xFF, sizeof(run->bitmap));
	UT_ASSERT(score % 64 == 0);
	score /= 64;

	for (; score > 0; --score) {
		run->bitmap[score] = 0;
	}
}

static void
test_recycler()
{
	struct mock_pop *mpop = MMAP_ANON_ALIGNED(MOCK_POOL_SIZE, Ut_pagesize);
	PMEMobjpool *pop = &mpop->p;
	memset(pop, 0, MOCK_POOL_SIZE);
	pop->size = MOCK_POOL_SIZE;
	pop->heap_size = MOCK_POOL_SIZE - sizeof(PMEMobjpool);
	pop->heap_offset = (uint64_t)((uint64_t)&mpop->heap - (uint64_t)mpop);
	pop->p_ops.persist = obj_heap_persist;
	pop->p_ops.memset_persist = obj_heap_memset_persist;
	pop->p_ops.base = pop;
	pop->p_ops.pool_size = pop->size;

	void *heap_start = (char *)pop + pop->heap_offset;
	uint64_t heap_size = pop->heap_size;
	struct palloc_heap *heap = &pop->heap;
	struct pmem_ops *p_ops = &pop->p_ops;

	UT_ASSERT(heap_check(heap_start, heap_size) != 0);
	UT_ASSERT(heap_init(heap_start, heap_size, p_ops) == 0);
	UT_ASSERT(heap_boot(heap, heap_start, heap_size, TEST_RUN_ID,
		pop, p_ops) == 0);
	UT_ASSERT(heap_buckets_init(heap) == 0);
	UT_ASSERT(pop->heap.rt != NULL);

	int ret;

	struct recycler *r = recycler_new(&pop->heap);
	UT_ASSERTne(r, NULL);

	init_run_with_score(pop->heap.layout, 0, 0);
	init_run_with_score(pop->heap.layout, 0, 64);

	struct memory_block mrun = {0, 0, 1, 0};
	struct memory_block mrun2 = {1, 0, 1, 0};

	ret = recycler_put(r, &mrun);
	UT_ASSERTeq(ret, 0);
	ret = recycler_put(r, &mrun2);
	UT_ASSERTeq(ret, 0);

	struct memory_block mrun_ret;
	struct memory_block mrun2_ret;

	ret = recycler_get(r, &mrun2_ret);
	UT_ASSERTeq(ret, 0);
	ret = recycler_get(r, &mrun_ret);
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
	struct memory_block mrun3_ret;
	struct memory_block mrun4_ret;

	ret = recycler_put(r, &mrun);
	UT_ASSERTeq(ret, 0);
	ret = recycler_put(r, &mrun2);
	UT_ASSERTeq(ret, 0);
	ret = recycler_put(r, &mrun3);
	UT_ASSERTeq(ret, 0);
	ret = recycler_put(r, &mrun4);
	UT_ASSERTeq(ret, 0);

	ret = recycler_get(r, &mrun3_ret);
	UT_ASSERTeq(ret, 0);
	ret = recycler_get(r, &mrun_ret);
	UT_ASSERTeq(ret, 0);
	ret = recycler_get(r, &mrun4_ret);
	UT_ASSERTeq(ret, 0);
	ret = recycler_get(r, &mrun2_ret);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(mrun.chunk_id, mrun_ret.chunk_id);
	UT_ASSERTeq(mrun2.chunk_id, mrun2_ret.chunk_id);
	UT_ASSERTeq(mrun3.chunk_id, mrun3_ret.chunk_id);
	UT_ASSERTeq(mrun4.chunk_id, mrun4_ret.chunk_id);

	heap_cleanup(heap);
	UT_ASSERT(heap->rt == NULL);

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
