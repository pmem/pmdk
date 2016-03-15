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
 * pmalloc.c -- persistent malloc implementation
 */

#include <errno.h>

#include "libpmemobj.h"
#include "util.h"
#include "redo.h"
#include "memops.h"
#include "pmalloc.h"
#include "lane.h"
#include "list.h"
#include "obj.h"
#include "out.h"
#include "heap.h"
#include "bucket.h"
#include "heap_layout.h"
#include "valgrind_internal.h"

enum alloc_op_redo {
	ALLOC_OP_REDO_PTR_OFFSET,
	ALLOC_OP_REDO_HEADER,

	MAX_ALLOC_OP_REDO
};

/*
 * Number of bytes between end of allocation header and beginning of user data.
 */
#define	DATA_OFF OBJ_OOB_SIZE

/*
 * Number of bytes between beginning of memory block and beginning of user data.
 */
#define	ALLOC_OFF (DATA_OFF + sizeof (struct allocation_header))

#define	USABLE_SIZE(_a)\
((_a)->size - sizeof (struct allocation_header))

#define	MEMORY_BLOCK_IS_EMPTY(_m)\
((_m).size_idx == 0)

#define	ALLOC_GET_HEADER(_pop, _off) (struct allocation_header *)\
((char *)OBJ_OFF_TO_PTR((_pop), (_off)) - ALLOC_OFF)

/*
 * alloc_write_header -- (internal) creates allocation header
 */
static void
alloc_write_header(PMEMobjpool *pop, struct allocation_header *alloc,
	struct memory_block m, uint64_t size)
{
	VALGRIND_ADD_TO_TX(alloc, sizeof (*alloc));
	alloc->chunk_id = m.chunk_id;
	alloc->size = size;
	alloc->zone_id = m.zone_id;
	VALGRIND_REMOVE_FROM_TX(alloc, sizeof (*alloc));
	pop->persist(pop, alloc, sizeof (*alloc));
}

/*
 * calc_block_offset -- (internal) calculates the block offset of allocation
 */
static uint16_t
calc_block_offset(PMEMobjpool *pop, struct allocation_header *alloc,
	size_t unit_size)
{
	uint16_t block_off = 0;
	if (unit_size != CHUNKSIZE) {
		struct memory_block m = {alloc->chunk_id, alloc->zone_id, 0, 0};
		void *data = heap_get_block_data(pop, m);
		uintptr_t diff = (uintptr_t)alloc - (uintptr_t)data;
		ASSERT(diff <= RUNSIZE);
		ASSERT((size_t)diff / unit_size <= UINT16_MAX);
		ASSERT(diff % unit_size == 0);
		block_off = (uint16_t)((size_t)diff / unit_size);
	}

	return block_off;
}

/*
 * get_mblock_from_alloc -- (internal) returns allocation memory block
 */
static struct memory_block
get_mblock_from_alloc(PMEMobjpool *pop, struct allocation_header *alloc)
{
	struct memory_block mblock = {
		alloc->chunk_id,
		alloc->zone_id,
		0,
		0
	};

	uint64_t unit_size = heap_get_chunk_block_size(pop, mblock);
	mblock.block_off = calc_block_offset(pop, alloc, unit_size);
	mblock.size_idx = CALC_SIZE_IDX(unit_size, alloc->size);

	return mblock;
}

/*
 * alloc_reserve_block -- (internal) reserves a memory block in volatile state
 */
static int
alloc_reserve_block(PMEMobjpool *pop, struct memory_block *m, size_t sizeh)
{
	struct bucket *b = heap_get_best_bucket(pop, sizeh);

	m->size_idx = b->calc_units(b, sizeh);

	int err = heap_get_bestfit_block(pop, b, m);

	if (err == ENOMEM && b->type == BUCKET_HUGE)
		return ENOMEM; /* there's only one huge bucket */

	if (err == ENOMEM) {
		/*
		 * There's no more available memory in the common heap and in
		 * this lane cache, fallback to the auxiliary (shared) bucket.
		 */
		b = heap_get_auxiliary_bucket(pop, sizeh);
		err = heap_get_bestfit_block(pop, b, m);
	}

	if (err == ENOMEM) {
		/*
		 * The auxiliary bucket cannot satisfy our request, borrow
		 * memory from other caches.
		 */
		heap_drain_to_auxiliary(pop, b, m->size_idx);
		err = heap_get_bestfit_block(pop, b, m);
	}

	if (err == ENOMEM) {
		/* we are completely out of memory */
		return ENOMEM;
	}

	return 0;
}

/*
 * alloc_prep_block -- (internal) prepares a memory block for allocation
 */
static int
alloc_prep_block(PMEMobjpool *pop, struct memory_block m,
	pmalloc_constr constructor, void *arg, uint64_t *offset_value)
{
	void *block_data = heap_get_block_data(pop, m);
	void *datap = (char *)block_data +
		sizeof (struct allocation_header);
	void *userdatap = (char *)datap + DATA_OFF;
	uint64_t unit_size = heap_get_chunk_block_size(pop, m);
	uint64_t real_size = unit_size * m.size_idx;

	ASSERT((uint64_t)block_data % _POBJ_CL_ALIGNMENT == 0);

	/* mark everything (including headers) as accessible */
	VALGRIND_DO_MAKE_MEM_UNDEFINED(pop, block_data, real_size);
	/* mark space as allocated */
	VALGRIND_DO_MEMPOOL_ALLOC(pop, userdatap, real_size - ALLOC_OFF);

	alloc_write_header(pop, block_data, m, real_size);

	int ret = 0;
	if (constructor != NULL)
		ret = constructor(pop, userdatap, real_size - ALLOC_OFF, arg);

	if (!ret)
		*offset_value = OBJ_PTR_TO_OFF(pop, userdatap);

	return ret;
}

/*
 * palloc_operation -- persistent memory operation. Takes a NULL pointer
 *	or an existing memory block and modifies it to occupy, at least, 'size'
 *	number of bytes.
 */
int
palloc_operation(PMEMobjpool *pop,
	uint64_t off, uint64_t *dest_off, size_t size,
	pmalloc_constr constructor, void *arg,
	struct operation_entry *entries, size_t nentries)
{
	struct bucket *b = NULL;
	struct allocation_header *alloc = NULL;
	struct memory_block m = {0, 0, 0, 0}; /* existing memory block */
	struct memory_block nb = {0, 0, 0, 0}; /* new memory block */
	struct memory_block rb = {0, 0, 0, 0}; /* reclaimed memory block */

	size_t sizeh = size + sizeof (struct allocation_header);

	int ret = 0;

	struct lane_section *lane;
	lane_hold(pop, &lane, LANE_SECTION_ALLOCATOR);

	if (off != 0) {
		alloc = ALLOC_GET_HEADER(pop, off);
		b = heap_get_chunk_bucket(pop, alloc->chunk_id, alloc->zone_id);
		m = get_mblock_from_alloc(pop, alloc);
	}

	/* if allocation or reallocation, reserve new memory */
	if (size != 0) {
		if (alloc != NULL && alloc->size == sizeh) /* no-op */
			goto out;

		if ((errno = alloc_reserve_block(pop, &nb, sizeh)) != 0) {
			ret = -1;
			goto out;
		}
	}

	struct allocator_lane_section *sec =
		(struct allocator_lane_section *)lane->layout;

	struct operation_context *ctx = operation_init(pop, sec->redo);
	if (ctx == NULL) {
		ERR("Failed to initialize memory operation context");
		errno = ENOMEM;
		ret = -1;
		goto out;
	}

	operation_add_entries(ctx, entries, nentries);

	uint64_t offset_value = 0; /* the resulting offset */

	/* lock and persistently free the existing memory block */
	if (!MEMORY_BLOCK_IS_EMPTY(m)) {
#ifdef DEBUG
		if (!heap_block_is_allocated(pop, m)) {
			ERR("Double free or heap corruption");
			ASSERT(0);
		}
#endif /* DEBUG */

		heap_lock_if_run(pop, m);
		rb = heap_free_block(pop, b, m, ctx);
		offset_value = 0;
	}

	if (!MEMORY_BLOCK_IS_EMPTY(nb)) {
#ifdef DEBUG
		if (heap_block_is_allocated(pop, nb)) {
			ERR("heap corruption");
			ASSERT(0);
		}
#endif /* DEBUG */

		if (alloc_prep_block(pop, nb, constructor,
				arg, &offset_value) != 0) {
			/*
			 * Constructor returned non-zero value which means
			 * the memory block reservation has to be rolled back.
			 */
			struct bucket *newb = heap_get_chunk_bucket(pop,
				nb.chunk_id, nb.zone_id);
			ASSERTne(newb, NULL);
			nb = heap_free_block(pop, newb, nb, NULL);
			CNT_OP(newb, insert, pop, nb);

			operation_delete(ctx);
			if (newb->type == BUCKET_RUN)
				heap_degrade_run_if_empty(pop, newb, nb);

			ret = -1;
			errno = ECANCELED;
			goto out;
		}

		heap_lock_if_run(pop, nb);

		heap_prep_block_header_operation(pop, nb, HEAP_OP_ALLOC, ctx);
	}

	/* not in-place realloc */
	if (!MEMORY_BLOCK_IS_EMPTY(m) && !MEMORY_BLOCK_IS_EMPTY(nb)) {
		size_t old_size = alloc->size;
		size_t to_cpy = old_size > sizeh ? sizeh : old_size;
		pop->memcpy_persist(pop,
			OBJ_OFF_TO_PTR(pop, offset_value),
			OBJ_OFF_TO_PTR(pop, off),
			to_cpy - ALLOC_OFF);
	}

	if (dest_off != NULL)
		operation_add_entry(ctx, dest_off, offset_value, OPERATION_SET);

	operation_process(ctx);

	if (!MEMORY_BLOCK_IS_EMPTY(nb)) {
		heap_unlock_if_run(pop, nb);
	}

	if (!MEMORY_BLOCK_IS_EMPTY(m)) {
		heap_unlock_if_run(pop, m);

		VALGRIND_DO_MEMPOOL_FREE(pop,
			(char *)heap_get_block_data(pop, m) + ALLOC_OFF);

		/* we might have been operating on inactive run */
		if (b != NULL) {
			CNT_OP(b, insert, pop, rb);
#ifdef DEBUG
			if (heap_block_is_allocated(pop, rb)) {
				ERR("heap corruption");
				ASSERT(0);
			}
#endif /* DEBUG */
			if (b->type == BUCKET_RUN)
				heap_degrade_run_if_empty(pop, b, rb);
		}
	}
	operation_delete(ctx);

out:
	lane_release(pop);

	return ret;
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
	return palloc_operation(pop, 0, off, size, NULL, NULL, NULL, 0);
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
	pmalloc_constr constructor, void *arg)
{
	return palloc_operation(pop, 0, off, size, constructor, arg, NULL, 0);
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
	return palloc_operation(pop, *off, off, size, NULL, 0, NULL, 0);
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
	pmalloc_constr constructor, void *arg)
{
	return palloc_operation(pop, *off, off, size, constructor, arg,
		NULL, 0);
}

/*
 * pmalloc_usable_size -- returns the number of bytes in the memory block
 */
size_t
pmalloc_usable_size(PMEMobjpool *pop, uint64_t off)
{
	return USABLE_SIZE(ALLOC_GET_HEADER(pop, off));
}

/*
 * pfree -- deallocates a memory block previously allocated by pmalloc
 *
 * A zero value is written persistently into the off variable.
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
void
pfree(PMEMobjpool *pop, uint64_t *off)
{
	int ret = palloc_operation(pop, *off, off, 0, NULL, NULL, NULL, 0);
	ASSERTeq(ret, 0);
}

/*
 * pmalloc_search_cb -- (internal) foreach callback. If the argument is equal
 *	to the current object offset then sets the argument to UINT64_MAX.
 *	If the argument is UINT64_MAX it breaks the iteration and sets the
 *	argument to the current object offset.
 */
static int
pmalloc_search_cb(uint64_t off, void *arg)
{
	uint64_t *prev = arg;

	if (*prev == UINT64_MAX) {
		*prev = off;

		return 1;
	}

	if (off == *prev)
		*prev = UINT64_MAX;

	return 0;
}

/*
 * pmalloc_first -- returns the first object from the heap.
 */
uint64_t
pmalloc_first(PMEMobjpool *pop)
{
	uint64_t off_search = UINT64_MAX;
	struct memory_block m = {0, 0, 0, 0};

	heap_foreach_object(pop, pmalloc_search_cb, &off_search, m);

	if (off_search == UINT64_MAX)
		return 0;

	return off_search + sizeof (struct allocation_header);
}

/*
 * pmalloc_next -- returns the next object relative to 'off'.
 */
uint64_t
pmalloc_next(PMEMobjpool *pop, uint64_t off)
{
	struct allocation_header *alloc = ALLOC_GET_HEADER(pop, off);
	struct memory_block m = get_mblock_from_alloc(pop, alloc);

	uint64_t off_search = off - ALLOC_OFF;

	heap_foreach_object(pop, pmalloc_search_cb, &off_search, m);

	if (off_search == (off - ALLOC_OFF) ||
		off_search == 0 ||
		off_search == UINT64_MAX)
		return 0;

	return off_search + sizeof (struct allocation_header);
}

/*
 * lane_allocator_construct -- create allocator lane section
 */
static int
lane_allocator_construct(PMEMobjpool *pop, struct lane_section *section)
{
	return 0;
}

/*
 * lane_allocator_destruct -- destroy allocator lane section
 */
static void
lane_allocator_destruct(PMEMobjpool *pop, struct lane_section *section)
{
	/* nop */
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

/*
 * lane_allocator_init -- initializes allocator section
 */
static int
lane_allocator_boot(PMEMobjpool *pop)
{
	return heap_boot(pop);
}

static struct section_operations allocator_ops = {
	.construct = lane_allocator_construct,
	.destruct = lane_allocator_destruct,
	.recover = lane_allocator_recovery,
	.check = lane_allocator_check,
	.boot = lane_allocator_boot
};

SECTION_PARM(LANE_SECTION_ALLOCATOR, &allocator_ops);
