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
#include "lane.h"
#include "redo.h"
#include "list.h"
#include "obj.h"
#include "out.h"
#include "heap.h"
#include "bucket.h"
#include "heap_layout.h"
#include "out.h"
#include "valgrind_internal.h"

enum alloc_op_redo {
	ALLOC_OP_REDO_PTR_OFFSET,
	ALLOC_OP_REDO_HEADER,

	MAX_ALLOC_OP_REDO
};

/*
 * alloc_write_header -- (internal) creates allocation header
 */
void
alloc_write_header(PMEMobjpool *pop, struct allocation_header *alloc,
	uint32_t chunk_id, uint32_t zone_id, uint64_t size)
{
	VALGRIND_ADD_TO_TX(alloc, sizeof (*alloc));
	alloc->chunk_id = chunk_id;
	alloc->size = size;
	alloc->zone_id = zone_id;
	VALGRIND_REMOVE_FROM_TX(alloc, sizeof (*alloc));
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
 * calc_block_offset -- (internal) calculates the block offset of allocation
 */
static uint16_t
calc_block_offset(PMEMobjpool *pop, struct bucket *b,
	struct allocation_header *alloc)
{
	uint16_t block_off = 0;
	if (bucket_is_small(b)) {
		struct memory_block m = {alloc->chunk_id, alloc->zone_id, 0, 0};
		void *data = heap_get_block_data(pop, m);
		uintptr_t diff = (uintptr_t)alloc - (uintptr_t)data;
		block_off = diff / bucket_unit_size(b);
		ASSERT(diff % bucket_unit_size(b) == 0);
	}

	return block_off;
}

/*
 * get_mblock_from_alloc -- (internal) returns allocation memory block
 */
static struct memory_block
get_mblock_from_alloc(PMEMobjpool *pop, struct bucket *b,
	struct allocation_header *alloc)
{
	struct memory_block mblock = {
		alloc->chunk_id,
		alloc->zone_id,
		bucket_calc_units(b, alloc->size),
		calc_block_offset(pop, b, alloc)
	};

	return mblock;
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
	void (*constructor)(PMEMobjpool *pop, void *ptr, void *arg),
	void *arg, uint64_t data_off)
{
	size_t sizeh = size + sizeof (struct allocation_header);

	struct bucket *b = heap_get_best_bucket(pop, sizeh);

	int err = 0;
	uint32_t units = bucket_calc_units(b, sizeh);

	struct memory_block m = {0, 0, units, 0};

	if ((err = heap_get_bestfit_block(pop, b, &m)) != 0)
		return err;

	uint64_t op_result = 0;

	void *block_data = heap_get_block_data(pop, m);
	void *datap = block_data + sizeof (struct allocation_header);

	ASSERT((uint64_t)block_data % _POBJ_CL_ALIGNMENT == 0);

	uint64_t real_size = bucket_unit_size(b) * units;

	alloc_write_header(pop, block_data, m.chunk_id, m.zone_id, real_size);

	if (constructor != NULL)
		constructor(pop, datap + data_off, arg);

	if ((err = heap_lock_if_run(pop, m)) != 0)
		return err;

	void *hdr = heap_get_block_header(pop, m, HEAP_OP_ALLOC, &op_result);

	struct lane_section *lane;
	if ((err = lane_hold(pop, &lane, LANE_SECTION_ALLOCATOR)) != 0)
		goto err_lane_hold;

	struct allocator_lane_section *sec =
		(struct allocator_lane_section *)lane->layout;

	redo_log_store(pop, sec->redo, ALLOC_OP_REDO_PTR_OFFSET,
		pop_offset(pop, off), pop_offset(pop, datap));
	redo_log_store_last(pop, sec->redo, ALLOC_OP_REDO_HEADER,
		pop_offset(pop, hdr), op_result);

	redo_log_process(pop, sec->redo, MAX_ALLOC_OP_REDO);

	if (lane_release(pop) != 0) {
		ERR("Failed to release the lane");
		ASSERT(0);
	}

	if (heap_unlock_if_run(pop, m) != 0) {
		ERR("Failed to release run lock");
		ASSERT(0);
	}

	return 0;

err_lane_hold:
	if (heap_unlock_if_run(pop, m) != 0) {
		ERR("Failed to release run lock");
		ASSERT(0);
	}

	if (bucket_insert_block(b, m) != 0) {
		ERR("Failed to recover heap volatile state");
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
	return prealloc_construct(pop, off, size, NULL, NULL, 0);
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
	void (*constructor)(PMEMobjpool *pop, void *ptr, void *arg), void *arg,
	uint64_t data_off)
{
	if (size <= pmalloc_usable_size(pop, *off))
		return 0;

	size_t sizeh = size + sizeof (struct allocation_header);

	int err = 0;

	struct allocation_header *alloc = alloc_get_header(pop, *off);
	struct bucket *b = heap_get_best_bucket(pop, alloc->size);

	uint32_t add_size_idx = bucket_calc_units(b, sizeh - alloc->size);
	uint32_t new_size_idx = bucket_calc_units(b, sizeh);
	uint64_t real_size = new_size_idx * bucket_unit_size(b);

	struct memory_block cnt = get_mblock_from_alloc(pop, b, alloc);

	if ((err = heap_lock_if_run(pop, cnt)) != 0)
		return err;

	struct memory_block next = {0};
	if ((err = heap_get_adjacent_free_block(pop, &next, cnt, 0)) != 0)
		goto error;

	if (next.size_idx < add_size_idx) {
		err = ENOMEM;
		goto error;
	}

	if ((err = heap_get_exact_block(pop, b, &next,
		add_size_idx)) != 0)
		goto error;

	struct memory_block *blocks[2] = {&cnt, &next};
	uint64_t op_result;
	void *hdr;
	struct memory_block m =
		heap_coalesce(pop, blocks, 2, HEAP_OP_ALLOC, &hdr, &op_result);

	void *block_data = heap_get_block_data(pop, m);
	void *datap = block_data + sizeof (struct allocation_header);
	if (constructor != NULL)
		constructor(pop, datap + data_off, arg);

	struct lane_section *lane;
	if ((err = lane_hold(pop, &lane, LANE_SECTION_ALLOCATOR)) != 0)
		goto error;

	struct allocator_lane_section *sec =
		(struct allocator_lane_section *)lane->layout;

	redo_log_store(pop, sec->redo, ALLOC_OP_REDO_PTR_OFFSET,
		pop_offset(pop, &alloc->size), real_size);
	redo_log_store_last(pop, sec->redo, ALLOC_OP_REDO_HEADER,
		pop_offset(pop, hdr), op_result);

	redo_log_process(pop, sec->redo, MAX_ALLOC_OP_REDO);

	if (lane_release(pop) != 0) {
		ERR("Failed to release the lane");
		ASSERT(0);
	}

	if (heap_unlock_if_run(pop, cnt) != 0) {
		ERR("Failed to release run lock");
		ASSERT(0);
	}

	return 0;

error:
	if (heap_unlock_if_run(pop, cnt) != 0) {
		ERR("Failed to release run lock");
		ASSERT(0);
	}

	return err;
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

	struct memory_block m = get_mblock_from_alloc(pop, b, alloc);

	if ((err = heap_lock_if_run(pop, m)) != 0)
		return err;

	uint64_t op_result;
	void *hdr;
	struct memory_block res = heap_free_block(pop, b, m, &hdr, &op_result);

	struct lane_section *lane;
	if ((err = lane_hold(pop, &lane, LANE_SECTION_ALLOCATOR)) != 0)
		goto error_lane_hold;

	struct allocator_lane_section *sec =
		(struct allocator_lane_section *)lane->layout;

	redo_log_store(pop, sec->redo, ALLOC_OP_REDO_PTR_OFFSET,
		pop_offset(pop, off), 0);
	redo_log_store_last(pop, sec->redo, ALLOC_OP_REDO_HEADER,
		pop_offset(pop, hdr), op_result);

	redo_log_process(pop, sec->redo, MAX_ALLOC_OP_REDO);

	if (lane_release(pop) != 0) {
		ERR("Failed to release the lane");
		ASSERT(0);
	}

	/*
	 * There's no point in rolling back redo log changes because the
	 * volatile errors don't break the persistent state.
	 */
	if (bucket_insert_block(b, res)
		!= 0) {
		ERR("Failed to update the heap volatile state");
		ASSERT(0);
	}

	if (heap_unlock_if_run(pop, m) != 0) {
		ERR("Failed to release run lock");
		ASSERT(0);
	}

	if (bucket_is_small(b) && heap_degrade_run_if_empty(pop, b, res) != 0) {
		ERR("Failed to degrade run");
		ASSERT(0);
	}

	return 0;

error_lane_hold:
	if (heap_unlock_if_run(pop, m) != 0) {
		ERR("Failed to release run lock");
		ASSERT(0);
	}

	return err;
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

	redo_log_recover(pop, sec->redo, MAX_ALLOC_OP_REDO);

	return 0;
}

/*
 * lane_allocator_check -- consistency check of allocator lane section
 */
static int
lane_allocator_check(PMEMobjpool *pop, struct lane_section_layout *section)
{
	LOG(3, "allocator lane %p", section);

	struct allocator_lane_section *sec =
		(struct allocator_lane_section *)section;

	int ret;
	if ((ret = redo_log_check(pop, sec->redo, MAX_ALLOC_OP_REDO)) != 0)
		ERR("allocator lane: redo log check failed");

	return ret;
}

struct section_operations allocator_ops = {
	.construct = lane_allocator_construct,
	.destruct = lane_allocator_destruct,
	.recover = lane_allocator_recovery,
	.check = lane_allocator_check
};

SECTION_PARM(LANE_SECTION_ALLOCATOR, &allocator_ops);
