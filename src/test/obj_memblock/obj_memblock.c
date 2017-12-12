/*
 * Copyright 2016-2017, Intel Corporation
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
 * obj_memblock.c -- unit test for memblock interface
 */
#include "memblock.h"
#include "memops.h"
#include "obj.h"
#include "unittest.h"
#include "heap.h"

#define NCHUNKS 10

static PMEMobjpool *pop;

FUNC_MOCK(operation_add_typed_entry, void, struct operation_context *ctx,
	void *ptr, uint64_t value,
	enum operation_type type, enum operation_entry_type en_type)
	FUNC_MOCK_RUN_DEFAULT {
		uint64_t *pval = ptr;
		switch (type) {
			case OPERATION_SET:
				*pval = value;
				break;
			case OPERATION_AND:
				*pval &= value;
				break;
			case OPERATION_OR:
				*pval |= value;
				break;
			default:
				UT_ASSERT(0);
		}
	}
FUNC_MOCK_END

FUNC_MOCK(operation_add_entry, void, struct operation_context *ctx, void *ptr,
	uint64_t value, enum operation_type type)
	FUNC_MOCK_RUN_DEFAULT {
		/* just call the mock above - the entry type doesn't matter */
		operation_add_typed_entry(ctx, ptr, value, type,
			ENTRY_TRANSIENT);
	}
FUNC_MOCK_END

static void
test_detect(void)
{
	struct memory_block mhuge_used = { .chunk_id = 0, 0, 0, 0 };
	struct memory_block mhuge_free = { .chunk_id = 1, 0, 0, 0 };
	struct memory_block mrun = { .chunk_id = 2, 0, 0, 0 };

	struct heap_layout *layout = pop->heap.layout;
	layout->zone0.chunk_headers[0].size_idx = 1;
	layout->zone0.chunk_headers[0].type = CHUNK_TYPE_USED;

	layout->zone0.chunk_headers[1].size_idx = 1;
	layout->zone0.chunk_headers[1].type = CHUNK_TYPE_FREE;

	layout->zone0.chunk_headers[2].size_idx = 1;
	layout->zone0.chunk_headers[2].type = CHUNK_TYPE_RUN;

	memblock_rebuild_state(&pop->heap, &mhuge_used);
	memblock_rebuild_state(&pop->heap, &mhuge_free);
	memblock_rebuild_state(&pop->heap, &mrun);

	UT_ASSERTeq(mhuge_used.type, MEMORY_BLOCK_HUGE);
	UT_ASSERTeq(mhuge_free.type, MEMORY_BLOCK_HUGE);
	UT_ASSERTeq(mrun.type, MEMORY_BLOCK_RUN);
}

static void
test_block_size(void)
{
	struct memory_block mhuge = { .chunk_id = 0, 0, 0, 0 };
	struct memory_block mrun = { .chunk_id = 1, 0, 0, 0 };

	struct palloc_heap *heap = &pop->heap;
	struct heap_layout *layout = heap->layout;

	layout->zone0.chunk_headers[0].size_idx = 1;
	layout->zone0.chunk_headers[0].type = CHUNK_TYPE_USED;

	layout->zone0.chunk_headers[1].size_idx = 1;
	layout->zone0.chunk_headers[1].type = CHUNK_TYPE_RUN;
	struct chunk_run *run = (struct chunk_run *)
		&layout->zone0.chunks[1];
	run->block_size = 1234;

	memblock_rebuild_state(&pop->heap, &mhuge);
	memblock_rebuild_state(&pop->heap, &mrun);

	UT_ASSERTne(mhuge.m_ops, NULL);
	UT_ASSERTne(mrun.m_ops, NULL);
	UT_ASSERTeq(mhuge.m_ops->block_size(&mhuge), CHUNKSIZE);
	UT_ASSERTeq(mrun.m_ops->block_size(&mrun), 1234);
}

static void
test_prep_hdr(void)
{
	struct memory_block mhuge_used = { .chunk_id = 0, 0, .size_idx = 1, 0 };
	struct memory_block mhuge_free = { .chunk_id = 1, 0, .size_idx = 1, 0 };
	struct memory_block mrun_used = { .chunk_id = 2, 0,
		.size_idx = 4, .block_off = 0 };
	struct memory_block mrun_free = { .chunk_id = 2, 0,
		.size_idx = 4, .block_off = 4 };
	struct memory_block mrun_large_used = { .chunk_id = 2, 0,
		.size_idx = 64, .block_off = 64 };
	struct memory_block mrun_large_free = { .chunk_id = 2, 0,
		.size_idx = 64, .block_off = 128 };

	struct palloc_heap *heap = &pop->heap;
	struct heap_layout *layout = heap->layout;

	layout->zone0.chunk_headers[0].size_idx = 1;
	layout->zone0.chunk_headers[0].type = CHUNK_TYPE_USED;

	layout->zone0.chunk_headers[1].size_idx = 1;
	layout->zone0.chunk_headers[1].type = CHUNK_TYPE_FREE;

	layout->zone0.chunk_headers[2].size_idx = 1;
	layout->zone0.chunk_headers[2].type = CHUNK_TYPE_RUN;

	struct chunk_run *run = (struct chunk_run *)&layout->zone0.chunks[2];

	run->bitmap[0] = 0b1111;
	run->bitmap[1] = ~0ULL;
	run->bitmap[2] = 0ULL;

	memblock_rebuild_state(heap, &mhuge_used);
	memblock_rebuild_state(heap, &mhuge_free);
	memblock_rebuild_state(heap, &mrun_used);
	memblock_rebuild_state(heap, &mrun_free);
	memblock_rebuild_state(heap, &mrun_large_used);
	memblock_rebuild_state(heap, &mrun_large_free);

	UT_ASSERTne(mhuge_used.m_ops, NULL);
	mhuge_used.m_ops->prep_hdr(&mhuge_used, MEMBLOCK_FREE, NULL);
	UT_ASSERTeq(layout->zone0.chunk_headers[0].type, CHUNK_TYPE_FREE);

	mhuge_free.m_ops->prep_hdr(&mhuge_free, MEMBLOCK_ALLOCATED, NULL);
	UT_ASSERTeq(layout->zone0.chunk_headers[1].type, CHUNK_TYPE_USED);

	mrun_used.m_ops->prep_hdr(&mrun_used, MEMBLOCK_FREE, NULL);
	UT_ASSERTeq(run->bitmap[0], 0ULL);

	mrun_free.m_ops->prep_hdr(&mrun_free, MEMBLOCK_ALLOCATED, NULL);
	UT_ASSERTeq(run->bitmap[0], 0b11110000);

	mrun_large_used.m_ops->prep_hdr(&mrun_large_used, MEMBLOCK_FREE, NULL);
	UT_ASSERTeq(run->bitmap[1], 0ULL);

	mrun_large_free.m_ops->prep_hdr(&mrun_large_free,
		MEMBLOCK_ALLOCATED, NULL);
	UT_ASSERTeq(run->bitmap[2], ~0ULL);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_memblock");
	PMEMobjpool pool;
	pop = &pool;

	pop->heap.layout = ZALLOC(sizeof(struct heap_layout) +
		NCHUNKS * sizeof(struct chunk));

	test_detect();
	test_block_size();
	test_prep_hdr();

	FREE(pop->heap.layout);

	DONE(NULL);
}
