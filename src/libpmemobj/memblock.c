/*
 * Copyright 2016, Intel Corporation
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
 * memblock.c -- implementation of memory block
 *
 * Memory block is a representation of persistent object that resides in the
 * heap. A valid memory block must be either a huge (free or used) chunk or a
 * block inside a run.
 *
 * Huge blocks are 1:1 correlated with the chunk headers in the zone whereas
 * run blocks are represented by bits in corresponding chunk bitmap.
 *
 * This file contains implementations of abstract operations on memory blocks.
 * Instead of storing the mbops structure inside each memory block the correct
 * method implementation is chosen at runtime.
 */

#include <stdlib.h>
#include <inttypes.h>
#include "libpmemobj.h"
#include "out.h"
#include "util.h"
#include "sys_util.h"
#include "lane.h"
#include "redo.h"
#include "memops.h"
#include "pmalloc.h"
#include "list.h"
#include "obj.h"
#include "heap_layout.h"
#include "memblock.h"
#include "heap.h"
#include "valgrind_internal.h"

/*
 * memblock_autodetect_type -- looks for the corresponding chunk header and
 *	depending on the chunks type returns the right memory block type.
 */
enum memory_block_type
memblock_autodetect_type(struct memory_block *m, struct heap_layout *h)
{
	enum memory_block_type ret;

	switch (ZID_TO_ZONE(h, m->zone_id)->chunk_headers[m->chunk_id].type) {
		case CHUNK_TYPE_RUN:
			ret = MEMORY_BLOCK_RUN;
			break;
		case CHUNK_TYPE_FREE:
		case CHUNK_TYPE_USED:
		case CHUNK_TYPE_FOOTER:
			ret = MEMORY_BLOCK_HUGE;
			break;
		default:
			/* unreachable */
			FATAL("possible zone chunks metadata corruption");
	}
	return ret;
}

/*
 * huge_block_size -- returns the compile-time constant which defines the
 *	huge memory block size.
 */
size_t
huge_block_size(struct memory_block *m, struct heap_layout *h)
{
	return CHUNKSIZE;
}

/*
 * run_block_size -- looks for the right chunk and returns the block size
 *	information that is attached to the run block metadata.
 */
size_t
run_block_size(struct memory_block *m, struct heap_layout *h)
{
	struct zone *z = ZID_TO_ZONE(h, m->zone_id);
	struct chunk_run *run = (struct chunk_run *)&z->chunks[m->chunk_id];

	return run->block_size;
}

/*
 * huge_block_offset -- huge chunks do not use the offset information of the
 *	memory blocks and must always be zeroed.
 */
uint16_t
huge_block_offset(struct memory_block *m, PMEMobjpool *pop, void *ptr)
{
	return 0;
}

/*
 * run_block_offset -- calculates the block offset based on the number of bytes
 *	between the beginning of the chunk and the allocation data.
 *
 * Because the block offset is not represented in bytes but in 'unit size',
 * the number of bytes must also be divided by the chunks block size.
 * A non-zero remainder would mean that either the caller provided incorrect
 * pointer or the allocation algorithm created an invalid allocation block.
 */
uint16_t
run_block_offset(struct memory_block *m, PMEMobjpool *pop, void *ptr)
{
	size_t block_size = MEMBLOCK_OPS(RUN, &m)->block_size(m, pop->hlayout);

	void *data = heap_get_block_data(pop, *m);
	uintptr_t diff = (uintptr_t)ptr - (uintptr_t)data;
	ASSERT(diff <= RUNSIZE);
	ASSERT((size_t)diff / block_size <= UINT16_MAX);
	ASSERT(diff % block_size == 0);
	uint16_t block_off = (uint16_t)((size_t)diff / block_size);

	return block_off;
}

/*
 * chunk_get_chunk_hdr_value -- (internal) get value of a header for redo log
 */
static uint64_t
chunk_get_chunk_hdr_value(uint16_t type, uint32_t size_idx)
{
	uint64_t val;
	COMPILE_ERROR_ON(sizeof(struct chunk_header) != sizeof(uint64_t));

	struct chunk_header hdr;
	hdr.type = type;
	hdr.size_idx = size_idx;
	hdr.flags = 0;
	memcpy(&val, &hdr, sizeof(val));

	return val;
}

/*
 * huge_prep_operation_hdr -- prepares the new value of a chunk header that will
 *	be set after the operation concludes.
 */
void
huge_prep_operation_hdr(struct memory_block *m, PMEMobjpool *pop,
	enum memblock_hdr_op op, struct operation_context *ctx)
{
	struct zone *z = ZID_TO_ZONE(pop->hlayout, m->zone_id);
	struct chunk_header *hdr = &z->chunk_headers[m->chunk_id];

	/*
	 * Depending on the operation that needs to be performed a new chunk
	 * header needs to be prepared with the new chunk state.
	 */
	uint64_t val = chunk_get_chunk_hdr_value(
		op == HDR_OP_ALLOC ? CHUNK_TYPE_USED : CHUNK_TYPE_FREE,
		m->size_idx);

	operation_add_entry(ctx, hdr, val, OPERATION_SET);

	VALGRIND_DO_MAKE_MEM_NOACCESS(pop, hdr + 1,
			(hdr->size_idx - 1) *
			sizeof(struct chunk_header));

	/*
	 * In the case of chunks larger than one unit the footer must be
	 * created immediately AFTER the persistent state is safely updated.
	 */
	if (m->size_idx == 1)
		return;

	struct chunk_header *footer = hdr + m->size_idx - 1;
	VALGRIND_DO_MAKE_MEM_UNDEFINED(pop, footer, sizeof(*footer));

	val = chunk_get_chunk_hdr_value(CHUNK_TYPE_FOOTER, m->size_idx);

	/*
	 * It's only safe to write the footer AFTER the persistent part of
	 * the operation have been successfully processed because the footer
	 * pointer might point to a currently valid persistent state
	 * of a different chunk.
	 * The footer entry change is updated as transient because it will
	 * be recreated at heap boot regardless - it's just needed for runtime
	 * operations.
	 */
	operation_add_typed_entry(ctx,
		footer, val, OPERATION_SET, ENTRY_TRANSIENT);
}

/*
 * run_prep_operation_hdr -- prepares the new value for a select few bytes of
 *	a run bitmap that will be set after the operation concludes.
 *
 * It's VERY important to keep in mind that the particular value of the
 * bitmap this method is modifying must not be changed after this function
 * is called and before the operation is processed.
 */
void
run_prep_operation_hdr(struct memory_block *m, PMEMobjpool *pop,
	enum memblock_hdr_op op, struct operation_context *ctx)
{
	struct zone *z = ZID_TO_ZONE(pop->hlayout, m->zone_id);

	struct chunk_run *r = (struct chunk_run *)&z->chunks[m->chunk_id];
	/*
	 * Free blocks are represented by clear bits and used blocks by set
	 * bits - which is the reverse of the commonly used scheme.
	 *
	 * Here a bit mask is prepared that flips the bits that represent the
	 * memory block provided by the caller - because both the size index and
	 * the block offset are tied 1:1 to the bitmap this operation is
	 * relatively simple.
	 */
	uint64_t bmask = ((1ULL << m->size_idx) - 1ULL) <<
			(m->block_off % BITS_PER_VALUE);

	/*
	 * The run bitmap is composed of several 8 byte values, so a proper
	 * element of the bitmap array must be selected.
	 */
	int bpos = m->block_off / BITS_PER_VALUE;

	/* the bit mask is applied immediately by the add entry operations */
	if (op == HDR_OP_ALLOC)
		operation_add_entry(ctx, &r->bitmap[bpos],
			bmask, OPERATION_OR);
	else
		operation_add_entry(ctx, &r->bitmap[bpos],
			~bmask, OPERATION_AND);
}

/*
 * huge_lock -- because huge memory blocks are always allocated from a single
 *	bucket there's no reason to lock them - the bucket itself is protected.
 */
void
huge_lock(struct memory_block *m, PMEMobjpool *pop)
{
	/* no-op */
}

/*
 * run_lock -- gets the runtime mutex from the heap and lock it.
 *
 */
void
run_lock(struct memory_block *m, PMEMobjpool *pop)
{
	util_mutex_lock(heap_get_run_lock(pop, m->chunk_id));
}

/*
 * huge_unlock -- do nothing, explanation above in huge_lock.
 */
void
huge_unlock(struct memory_block *m, PMEMobjpool *pop)
{
	/* no-op */
}

/*
 * run_unlock -- gets the runtime mutex from the heap and unlocks it.
 */
void
run_unlock(struct memory_block *m, PMEMobjpool *pop)
{
	util_mutex_unlock(heap_get_run_lock(pop, m->chunk_id));
}

const struct memory_block_ops mb_ops[MAX_MEMORY_BLOCK] = {
	[MEMORY_BLOCK_HUGE] = {
		.block_size = huge_block_size,
		.block_offset = huge_block_offset,
		.prep_hdr = huge_prep_operation_hdr,
		.lock = huge_lock,
		.unlock = huge_unlock,
	},
	[MEMORY_BLOCK_RUN] = {
		.block_size = run_block_size,
		.block_offset = run_block_offset,
		.prep_hdr = run_prep_operation_hdr,
		.lock = run_lock,
		.unlock = run_unlock,
	}
};
