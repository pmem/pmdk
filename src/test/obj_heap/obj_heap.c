/*
 * Copyright 2015-2016, Intel Corporation
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
#include "libpmem.h"
#include "libpmemobj.h"
#include "util.h"
#include "redo.h"
#include "memops.h"
#include "heap_layout.h"
#include "memblock.h"
#include "heap.h"
#include "bucket.h"
#include "lane.h"
#include "pmalloc.h"
#include "list.h"
#include "obj.h"
#include "unittest.h"

#define MOCK_POOL_SIZE PMEMOBJ_MIN_POOL

#define MAX_BLOCKS 3

struct mock_pop {
	PMEMobjpool p;
	void *heap;
};

static void
obj_heap_persist(PMEMobjpool *pop, const void *ptr, size_t sz)
{
	pmem_msync(ptr, sz);
}

static void
test_heap()
{
	struct mock_pop *mpop = Malloc(MOCK_POOL_SIZE);
	PMEMobjpool *pop = &mpop->p;
	memset(pop, 0, MOCK_POOL_SIZE);
	pop->size = MOCK_POOL_SIZE;
	pop->heap_size = MOCK_POOL_SIZE - sizeof(PMEMobjpool);
	pop->heap_offset = (uint64_t)((uint64_t)&mpop->heap - (uint64_t)mpop);
	pop->persist = obj_heap_persist;

	UT_ASSERT(heap_check(pop) != 0);
	UT_ASSERT(heap_init(pop) == 0);
	UT_ASSERT(heap_boot(pop) == 0);
	UT_ASSERT(pop->heap != NULL);

	struct bucket *b_small = heap_get_best_bucket(pop, 1);
	struct bucket *b_big = heap_get_best_bucket(pop, 2048);

	UT_ASSERT(b_small->unit_size < b_big->unit_size);

	struct bucket *b_def = heap_get_best_bucket(pop, CHUNKSIZE);
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
		heap_get_bestfit_block(pop, b_def, &blocks[i]);
		UT_ASSERT(blocks[i].block_off == 0);
	}

	struct memory_block *blocksp[MAX_BLOCKS] = {NULL};

	struct memory_block prev;
	heap_get_adjacent_free_block(pop, b_def, &prev, blocks[1], 1);
	UT_ASSERT(prev.chunk_id == blocks[0].chunk_id);
	blocksp[0] = &prev;

	struct memory_block cnt;
	heap_get_adjacent_free_block(pop, b_def, &cnt, blocks[0], 0);
	UT_ASSERT(cnt.chunk_id == blocks[1].chunk_id);
	blocksp[1] = &cnt;

	struct memory_block next;
	heap_get_adjacent_free_block(pop, b_def, &next, blocks[1], 0);
	UT_ASSERT(next.chunk_id == blocks[2].chunk_id);
	blocksp[2] = &next;

	struct operation_context ctx;
	operation_init(pop, &ctx, NULL);
	struct memory_block result =
		heap_coalesce(pop, blocksp, MAX_BLOCKS, HDR_OP_FREE, &ctx);
	operation_process(&ctx);

	UT_ASSERT(result.size_idx == 3);
	UT_ASSERT(result.chunk_id == prev.chunk_id);

	UT_ASSERT(heap_check(pop) == 0);
	heap_cleanup(pop);
	UT_ASSERT(pop->heap == NULL);

	Free(mpop);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_heap");

	test_heap();

	DONE(NULL);
}
