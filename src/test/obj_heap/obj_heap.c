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
 * obj_heap.c -- unit test for bucket
 */
#include "heap.h"
#include "obj.h"
#include "unittest.h"
#include "util.h"

#define MOCK_POOL_SIZE PMEMOBJ_MIN_POOL

#define MAX_BLOCKS 3

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
	UT_ASSERT(heap_boot(heap, heap_start, heap_size, pop, p_ops) == 0);
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

	struct memory_block prev;
	heap_get_adjacent_free_block(heap, b_def, &prev, blocks[1], 1);
	UT_ASSERT(prev.chunk_id == blocks[0].chunk_id);
	struct memory_block cnt;
	heap_get_adjacent_free_block(heap, b_def, &cnt, blocks[0], 0);
	UT_ASSERT(cnt.chunk_id == blocks[1].chunk_id);

	struct memory_block next;
	heap_get_adjacent_free_block(heap, b_def, &next, blocks[1], 0);
	UT_ASSERT(next.chunk_id == blocks[2].chunk_id);

	UT_ASSERT(heap_check(heap_start, heap_size) == 0);
	heap_cleanup(heap);
	UT_ASSERT(heap->rt == NULL);

	MUNMAP_ANON_ALIGNED(mpop, MOCK_POOL_SIZE);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_heap");

	test_heap();

	DONE(NULL);
}
