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
 * pmalloc.c -- persistent malloc implementation
 */

#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "libpmemobj.h"
#include "util.h"
#include "pmalloc.h"
#include "list.h"
#include "obj.h"
#include "out.h"
#include "heap.h"
#include "bucket.h"
#include "redo.h"
#include "heap_layout.h"
#include "lane.h"
#include "out.h"

enum pmalloc_redo {
	PMALLOC_REDO_PTR_OFFSET,
	PMALLOC_REDO_HEADER,

	MAX_PMALLOC_REDO
};

enum pfree_redo {
	PFREE_REDO_PTR_OFFSET,
	PFREE_REDO_HEADER,

	MAX_PFREE_REDO
};

/*
 * alloc_write_header -- (internal) creates allocation header
 */
void
alloc_write_header(PMEMobjpool *pop, struct allocation_header *alloc,
	uint32_t chunk_id, uint32_t zone_id, uint64_t size)
{
	alloc->chunk_id = chunk_id;
	alloc->size = size;
	alloc->zone_id = zone_id;

	pop->persist(alloc, sizeof (*alloc));
}

/*
 * alloc_get_header -- (internal) calculates the address of allocation header
 */
struct allocation_header *
alloc_get_header(PMEMobjpool *pop, uint64_t off)
{
	void *ptr = (void *)pop + off;
	struct allocation_header *alloc = ptr - sizeof (*alloc);

	return alloc;
}

/*
 * pop_offset -- (internal) calculates offset of ptr in the pool
 */
static uint64_t
pop_offset(PMEMobjpool *pop, void *ptr)
{
	return (uint64_t)ptr - (uint64_t)pop;
}

/*
 * pmalloc -- allocates a new block of memory
 *
 * The pool offset is written persistently into the off variable.
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
pmalloc(PMEMobjpool *pop, uint64_t *off, size_t size)
{
	return pmalloc_construct(pop, off, size, NULL, NULL, 0);
}

/*
 * pmalloc_construct -- allocates a new block of memory with a constructor
 *
 * The block offset is written persistently into the off variable, but only
 * after the constructor function has been called.
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
pmalloc_construct(PMEMobjpool *pop, uint64_t *off, size_t size,
	void (*constructor)(void *ptr, void *arg), void *arg, uint64_t data_off)
{
	size_t sizeh = size + sizeof (struct allocation_header);

	struct bucket *b = heap_get_best_bucket(pop, sizeh);

	int err = 0;
	uint32_t units = bucket_calc_units(b, sizeh);

	uint32_t chunk_id = 0;
	uint32_t zone_id = 0;
	uint32_t size_idx = units;
	uint16_t block_off = 0;

	if ((err = heap_get_block_from_bucket(pop, b, &chunk_id, &zone_id,
		size_idx, &block_off)) != 0)
		return err;

	uint64_t op_result = 0;

	void *block_data = heap_get_block_data(pop, chunk_id, zone_id,
				block_off);
	void *datap = block_data + sizeof (struct allocation_header);
	if (constructor != NULL)
		constructor(datap + data_off, arg);

	if ((err = heap_lock_if_run(pop, chunk_id, zone_id)) != 0)
		return err;

	void *hdr = heap_get_block_header(pop, chunk_id, zone_id, size_idx,
			block_off, HEAP_OP_ALLOC, &op_result);

	uint64_t real_size = bucket_unit_size(b) * units;

	alloc_write_header(pop, block_data, chunk_id, zone_id, real_size);

	struct lane_section *lane;
	if ((err = lane_hold(pop, &lane, LANE_SECTION_ALLOCATOR)) != 0)
		goto err_lane_hold;

	struct allocator_lane_section *sec =
		(struct allocator_lane_section *)lane->layout;

	redo_log_store(pop, sec->redo, PMALLOC_REDO_PTR_OFFSET,
		pop_offset(pop, off), pop_offset(pop, datap));
	redo_log_store_last(pop, sec->redo, PMALLOC_REDO_HEADER,
		pop_offset(pop, hdr), op_result);

	redo_log_process(pop, sec->redo, MAX_PMALLOC_REDO);

	if (lane_release(pop) != 0) {
		LOG(1, "Failed to release the lane");
		ASSERT(0);
	}

	if (heap_unlock_if_run(pop, chunk_id, zone_id) != 0) {
		LOG(1, "Failed to release run lock");
		ASSERT(0);
	}

	return 0;

err_lane_hold:
	if (heap_unlock_if_run(pop, chunk_id, zone_id) != 0) {
		LOG(1, "Failed to release run lock");
		ASSERT(0);
	}

	if (bucket_insert_block(b, chunk_id, zone_id, units, block_off) != 0) {
		LOG(1, "Failed to recover heap volatile state");
		ASSERT(0);
	}

	return err;
}

/*
 * prealloc -- resizes in-place a previously allocated memory block
 *
 * The block offset is written persistently into the off variable.
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
prealloc(PMEMobjpool *pop, uint64_t *off, size_t size)
{
	/* XXX */

	return ENOSYS;
}

/*
 * prealloc_construct -- resizes an existing memory block with a constructor
 *
 * The block offset is written persistently into the off variable, but only
 * after the constructor function has been called.
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
prealloc_construct(PMEMobjpool *pop, uint64_t *off, size_t size,
	void (*constructor)(void *ptr, void *arg), void *arg,
	uint64_t data_off)
{
	/* XXX */

	return ENOSYS;
}


/*
 * pmalloc_usable_size -- returns the number of bytes in the memory block
 */
size_t
pmalloc_usable_size(PMEMobjpool *pop, uint64_t off)
{
	return alloc_get_header(pop, off)->size -
		sizeof (struct allocation_header);
}

/*
 * pfree -- deallocates a memory block previously allocated by pmalloc
 *
 * A zero value is written persistently into the off variable.
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
pfree(PMEMobjpool *pop, uint64_t *off)
{
	struct allocation_header *alloc = alloc_get_header(pop, *off);

	struct bucket *b = heap_get_best_bucket(pop, alloc->size);

	int err = 0;
	uint32_t zone_id = alloc->zone_id;
	uint32_t chunk_id = alloc->chunk_id;
	uint32_t size_idx = bucket_calc_units(b, alloc->size);
	uint16_t block_off = 0;
	if (bucket_is_small(b)) {
		void *data = heap_get_block_data(pop, chunk_id,
				zone_id, 0);
		uintptr_t diff = (uintptr_t)alloc - (uintptr_t)data;
		block_off = diff / bucket_unit_size(b);
		ASSERT((uintptr_t)diff % bucket_unit_size(b) == 0);
	}

	if ((err = heap_lock_if_run(pop, chunk_id, zone_id)) != 0)
		return err;

	uint64_t op_result = 0;
	void *hdr = heap_get_block_header(pop, chunk_id, zone_id, size_idx,
			block_off, HEAP_OP_FREE, &op_result);

	struct lane_section *lane;
	if ((err = lane_hold(pop, &lane, LANE_SECTION_ALLOCATOR)) != 0)
		goto error_lane_hold;

	struct allocator_lane_section *sec =
		(struct allocator_lane_section *)lane->layout;

	redo_log_store(pop, sec->redo, PFREE_REDO_PTR_OFFSET,
		pop_offset(pop, off), 0);
	redo_log_store_last(pop, sec->redo, PFREE_REDO_HEADER,
		pop_offset(pop, hdr), op_result);

	redo_log_process(pop, sec->redo, MAX_PFREE_REDO);

	if (lane_release(pop) != 0) {
		LOG(1, "Failed to release the lane");
		ASSERT(0);
	}

	if (heap_unlock_if_run(pop, chunk_id, zone_id) != 0) {
		LOG(1, "Failed to release run lock");
		ASSERT(0);
	}

	struct chunk_header *chunks[3] = {NULL, hdr, NULL};

	/* coalesce chunks */
	if (!bucket_is_small(b)) {
		uint32_t prev_id = alloc->chunk_id;
		struct chunk_header *prev =
			heap_get_prev_chunk(pop, &prev_id, zone_id);
		if (prev != NULL && prev->type == CHUNK_TYPE_FREE &&
			bucket_get_rm_block_exact(b, prev_id, zone_id,
				prev->size_idx, 0) == 0) {
			chunks[0] = prev;
			chunk_id = prev_id;
		}

		uint32_t next_id = alloc->chunk_id;
		struct chunk_header *next =
			heap_get_next_chunk(pop, &next_id, zone_id);
		if (next != NULL && next->type == CHUNK_TYPE_FREE &&
			bucket_get_rm_block_exact(b, next_id, zone_id,
						next->size_idx, 0) == 0) {
			chunks[2] = next;
		}

		if (chunks[0] || chunks[2]) {
			struct chunk_header *chdr =
					heap_coalesce(pop, chunks, 3);
			size_idx = chdr->size_idx;
		}
	}

	/*
	 * There's no point in rolling back redo log changes because the
	 * volatile errors don't break the persistent state.
	 */
	if (bucket_insert_block(b, chunk_id, zone_id, size_idx, block_off)
		!= 0) {
		LOG(1, "Failed to update the heap volatile state");
		ASSERT(0);
	}

	return 0;

error_lane_hold:
	if (heap_unlock_if_run(pop, chunk_id, zone_id) != 0) {
		LOG(1, "Failed to release run lock");
		ASSERT(0);
	}

	return err;
}

/*
 * pgrow -- checks whether the reallocation in-place can be performed
 */
int
pgrow(PMEMobjpool *pop, uint64_t off, size_t size)
{
	/* XXX */
	return ENOSYS;
}

/*
 * lane_allocator_construct -- create allocator lane section
 */
static int
lane_allocator_construct(struct lane_section *section)
{
	/* no-op */

	return 0;
}

/*
 * lane_allocator_destruct -- destroy allocator lane section
 */
static int
lane_allocator_destruct(struct lane_section *section)
{
	/* no-op */

	return 0;
}

/*
 * lane_allocator_recovery -- recovery of allocator lane section
 */
static int
lane_allocator_recovery(PMEMobjpool *pop, struct lane_section_layout *section)
{
	struct allocator_lane_section *sec =
		(struct allocator_lane_section *)section;

	redo_log_recover(pop, sec->redo, MAX_PMALLOC_REDO);

	return 0;
}

/*
 * lane_allocator_check -- consistency check of allocator lane section
 */
static int
lane_allocator_check(PMEMobjpool *pop, struct lane_section_layout *section)
{
	struct allocator_lane_section *sec =
		(struct allocator_lane_section *)section;

	return redo_log_check(pop, sec->redo, MAX_PMALLOC_REDO);
}

struct section_operations allocator_ops = {
	.construct = lane_allocator_construct,
	.destruct = lane_allocator_destruct,
	.recover = lane_allocator_recovery,
	.check = lane_allocator_check
};

SECTION_PARM(LANE_SECTION_ALLOCATOR, &allocator_ops);
