/*
 * Copyright (c) 2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
#include "heap_layout.h"
#include "bucket.h"
#include "lane.h"
#include "list.h"
#include "obj.h"
#include "heap.h"
#include "pmalloc.h"
#include "unittest.h"

#define	MOCK_POOL_SIZE PMEMOBJ_MIN_POOL

#define	CHUNK_FIRST	0
#define	CHUNK_SECOND	1
#define	CHUNK_THIRD	2
#define	CHUNK_NEW_SIZE_IDX 1
#define	MAX_BLOCKS 3

struct mock_pop {
	PMEMobjpool p;
	void *heap;
};

void
test_heap()
{
	struct mock_pop *mpop = Malloc(MOCK_POOL_SIZE);
	PMEMobjpool *pop = &mpop->p;
	memset(pop, 0, MOCK_POOL_SIZE);
	pop->heap_size = MOCK_POOL_SIZE - sizeof (PMEMobjpool);
	pop->heap_offset = (uint64_t)((uint64_t)&mpop->heap - (uint64_t)mpop);
	pop->persist = (persist_fn)pmem_msync;

	ASSERT(heap_check(pop) != 0);
	ASSERT(heap_init(pop) == 0);
	ASSERT(heap_boot(pop) == 0);
	ASSERT(pop->heap != NULL);

	struct bucket *b_small = heap_get_best_bucket(pop, 0);
	struct bucket *b_big = heap_get_best_bucket(pop, 1024);

	ASSERT(bucket_unit_size(b_small) < bucket_unit_size(b_big));

	struct bucket *b_def = heap_get_default_bucket(pop);
	ASSERT(bucket_unit_size(b_def) == CHUNKSIZE);

	ASSERT(!bucket_is_empty(b_small));
	ASSERT(!bucket_is_empty(b_big));
	ASSERT(!bucket_is_empty(b_def));

	uint32_t zone_id[MAX_BLOCKS] = {0};
	uint32_t chunk_id[MAX_BLOCKS] = {0};
	uint32_t size_idx[MAX_BLOCKS] = {1, 1, 1};
	uint16_t block_off[MAX_BLOCKS] = {0};

	struct chunk_header *hdr[MAX_BLOCKS] = {NULL, NULL, NULL};
	for (int i = 0; i < MAX_BLOCKS; ++i) {
		heap_get_block_from_bucket(pop, b_def, &chunk_id[i],
			&zone_id[i], size_idx[i], &block_off[i]);
		ASSERT(block_off[i] == 0);
		ASSERT(chunk_id[i] != 0);
	}

	uint32_t prev = chunk_id[1];
	hdr[0] = heap_get_prev_chunk(pop, &prev, 0);
	ASSERT(prev == chunk_id[0]);

	uint32_t mid = chunk_id[0];
	hdr[1] = heap_get_next_chunk(pop, &mid, 0);
	ASSERT(mid == chunk_id[1]);

	uint32_t next = chunk_id[1];
	hdr[2] = heap_get_next_chunk(pop, &next, 0);
	ASSERT(next == chunk_id[2]);

	heap_coalesce(pop, hdr, MAX_BLOCKS);

	ASSERT(hdr[0]->size_idx == MAX_BLOCKS);

	ASSERT(heap_check(pop) == 0);
	ASSERT(heap_cleanup(pop) == 0);
	ASSERT(pop->heap == NULL);

	Free(mpop);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_heap");

	test_heap();

	DONE(NULL);
}
