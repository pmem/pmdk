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
 * pmalloc.c -- implementation of pmalloc POSIX-like API
 *
 * This is the front-end part of the persistent memory allocator. It uses both
 * transient and persistent representation of the heap to provide memory blocks
 * in a reasonable time and with an acceptable common-case fragmentation.
 */

#include <errno.h>
#include <pthread.h>

#include "libpmemobj.h"
#include "redo.h"
#include "memops.h"
#include "pmalloc.h"
#include "lane.h"
#include "obj.h"
#include "out.h"
#include "heap_layout.h"
#include "memblock.h"
#include "heap.h"
#include "bucket.h"
#include "valgrind_internal.h"

/*
 * Number of bytes between end of allocation header and beginning of user data.
 */
#define DATA_OFF OBJ_OOB_SIZE

/*
 * Number of bytes between beginning of memory block and beginning of user data.
 */
#define ALLOC_OFF (DATA_OFF + sizeof(struct allocation_header))

#define USABLE_SIZE(_a)\
((_a)->size - sizeof(struct allocation_header))

#define MEMORY_BLOCK_IS_EMPTY(_m)\
((_m).size_idx == 0)

#define ALLOC_GET_HEADER(_pop, _off) (struct allocation_header *)\
((char *)OBJ_OFF_TO_PTR((_pop), (_off)) - ALLOC_OFF)

/*
 * alloc_write_header -- (internal) creates allocation header
 */
static void
alloc_write_header(PMEMobjpool *pop, struct allocation_header *alloc,
	struct memory_block m, uint64_t size)
{
	VALGRIND_ADD_TO_TX(alloc, sizeof(*alloc));
	alloc->chunk_id = m.chunk_id;
	alloc->size = size;
	alloc->zone_id = m.zone_id;
	VALGRIND_REMOVE_FROM_TX(alloc, sizeof(*alloc));
}

/*
 * get_mblock_from_alloc -- (internal) returns allocation memory block
 */
static struct memory_block
get_mblock_from_alloc(PMEMobjpool *pop, struct allocation_header *alloc)
{
	struct memory_block m = {
		alloc->chunk_id,
		alloc->zone_id,
		0,
		0
	};

	uint64_t unit_size = MEMBLOCK_OPS(AUTO, &m)->block_size(&m,
		pop->hlayout);
	m.block_off = MEMBLOCK_OPS(AUTO, &m)->block_offset(&m, pop, alloc);
	m.size_idx = CALC_SIZE_IDX(unit_size, alloc->size);

	return m;
}

/*
 * alloc_reserve_block -- (internal) reserves a memory block in volatile state
 *
 * The first step in the allocation of a new block is reserving it in the
 * transient heap - which is represented by the bucket abstraction.
 *
 * To provide optimal scaling for multi-threaded applications and reduce
 * fragmentation the appropriate bucket is chosen depending on the current
 * thread context and to which allocation class the requested size falls into.
 *
 * Once the bucket is selected, just enough memory is reserved for the requested
 * size. The underlying block allocation algorithm (best-fit, next-fit, ...)
 * varies depending on the bucket container.
 *
 * Because the heap in general tries to avoid lock-contention on buckets,
 * the threads might, in near OOM cases, be unable to allocate requested memory
 * from their assigned buckets. To combat this there's one common collection
 * of buckets that threads can fallback to. The auxiliary bucket will 'steal'
 * memory from other caches if that's required to satisfy the current caller
 * needs.
 *
 * Once this method completes no further locking is required on the transient
 * part of the heap during the allocation process.
 */
static int
alloc_reserve_block(PMEMobjpool *pop, struct memory_block *m, size_t sizeh)
{
	struct bucket *b = heap_get_best_bucket(pop, sizeh);

	/*
	 * The caller provided size in bytes, but buckets operate in
	 * 'size indexes' which are multiples of the block size in the bucket.
	 *
	 * For example, to allocate 500 bytes from a bucket that provides 256
	 * byte blocks two memory 'units' are required.
	 */
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
 *
 * Once the block is fully reserved and it's guaranteed that no one else will
 * be able to write to this memory region it is safe to write the allocation
 * header and call the object construction function.
 *
 * Because the memory block at this stage is only reserved in transient state
 * there's no need to worry about fail-safety of this method because in case
 * of a crash the memory will be back in the free blocks collection.
 */
static int
alloc_prep_block(PMEMobjpool *pop, struct memory_block m,
	pmalloc_constr constructor, void *arg, uint64_t *offset_value)
{
	void *block_data = heap_get_block_data(pop, m);
	void *userdatap = (char *)block_data + ALLOC_OFF;

	uint64_t unit_size = MEMBLOCK_OPS(AUTO, &m)->block_size(&m,
		pop->hlayout);

	uint64_t real_size = unit_size * m.size_idx;

	ASSERT((uint64_t)block_data % _POBJ_CL_ALIGNMENT == 0);

	/* mark everything (including headers) as accessible */
	VALGRIND_DO_MAKE_MEM_UNDEFINED(pop, block_data, real_size);
	/* mark space as allocated */
	VALGRIND_DO_MEMPOOL_ALLOC(pop, userdatap, real_size - ALLOC_OFF);

	alloc_write_header(pop, block_data, m, real_size);

	int ret = 0;
	if (constructor != NULL &&
		(ret = constructor(pop, userdatap,
			real_size - ALLOC_OFF, arg)) != 0) {

		/*
		 * If canceled, revert the block back to the free state in vg
		 * machinery. Because the free operation is only performed on
		 * the user data, the allocation header is made inaccessible
		 * in a separate call.
		 */
		VALGRIND_DO_MEMPOOL_FREE(pop, userdatap);
		VALGRIND_DO_MAKE_MEM_NOACCESS(pop, block_data, ALLOC_OFF);

		/*
		 * During this method there are several stores to pmem that are
		 * not immediately flushed and in case of a cancelation those
		 * stores are no longer relevant anyway.
		 */
		VALGRIND_SET_CLEAN(block_data, ALLOC_OFF);

		return ret;
	}

	/* flushes both the alloc and oob headers */
	pop->persist(pop, block_data, ALLOC_OFF);

#ifdef USE_VG_MEMCHECK
	if (On_valgrind) {
		struct oob_header *pobj = (struct oob_header *)
			((char *)block_data + sizeof(struct allocation_header));

		/*
		 * The first few bytes of the oobh are unused and double as
		 * an object guard which will cause valgrind to issue an error
		 * whenever the unused memory is accessed.
		 */
		VALGRIND_DO_MAKE_MEM_NOACCESS(pop, pobj->unused,
			sizeof(pobj->unused));
	}
#endif

	/*
	 * To avoid determining the user data pointer twice this method is also
	 * responsible for calculating the offset of the object in the pool that
	 * will be used to set the offset destination pointer provided by the
	 * caller.
	 */
	*offset_value = OBJ_PTR_TO_OFF(pop, userdatap);

	return ret;
}

/*
 * palloc_operation -- persistent memory operation. Takes a NULL pointer
 *	or an existing memory block and modifies it to occupy, at least, 'size'
 *	number of bytes.
 *
 * The malloc, free and realloc routines are implemented in the context of this
 * common operation which encompasses all of the functionality usually done
 * separately in those methods.
 *
 * The first thing that needs to be done is determining which memory blocks
 * will be affected by the operation - this varies depending on the whether the
 * operation will need to modify or free an existing block and/or allocate
 * a new one.
 *
 * Simplified allocation process flow is as follows:
 *	- reserve a new block in the transient heap
 *	- prepare the new block
 *	- create redo log of required modifications
 *		- chunk metadata
 *		- offset of the new object
 *	- commit and process the redo log
 *
 * And similarly, the deallocation process:
 *	- create redo log of required modifications
 *		- reverse the chunk metadata back to the 'free' state
 *		- set the destination of the object offset to zero
 *	- commit and process the redo log
 *	- return the memory block back to the free blocks transient heap
 *
 * Reallocation is a combination of the above, which one additional step
 * of copying the old content in the meantime.
 */
int
palloc_operation(PMEMobjpool *pop,
	uint64_t off, uint64_t *dest_off, size_t size,
	pmalloc_constr constructor, void *arg,
	struct operation_entry *entries, size_t nentries)
{
	struct bucket *b = NULL;
	struct allocation_header *alloc = NULL;
	struct memory_block existing_block = {0, 0, 0, 0};
	struct memory_block new_block = {0, 0, 0, 0};
	struct memory_block reclaimed_block = {0, 0, 0, 0};

	size_t sizeh = size + sizeof(struct allocation_header);

	int ret = 0;

	/*
	 * The lane is always held for the entire duration of the process.
	 * This might seem a bit excessive at first glance, because the actual
	 * lane usage is limited to the scope in which operation_context
	 * exists, and in fact could be entirely omitted in some cases.
	 *
	 * The reason here is the potential ordering problem between lane locks
	 * and run locks, which in some fringe cases could result in a deadlock.
	 */
	struct lane_section *lane;
	lane_hold(pop, &lane, LANE_SECTION_ALLOCATOR);

	/*
	 * The offset of an existing block can be nonzero which means this
	 * operation is either free or a realloc - either way the offset of the
	 * object needs to be translated into structure that all of the heap
	 * methods operate in.
	 */
	if (off != 0) {
		alloc = ALLOC_GET_HEADER(pop, off);
		/*
		 * The memory block must return back to the originating bucket,
		 * otherwise coalescing of neighbouring blocks will be rendered
		 * impossible.
		 *
		 * If the block was allocated in a different incarnation of the
		 * heap (i.e. the application was restarted) and the chunk from
		 * which the allocation comes from was not yet processed, the
		 * originating bucket does not exists and all of the otherwise
		 * necessary volatile heap modifications won't be performed for
		 * this memory block.
		 */
		b = heap_get_chunk_bucket(pop, alloc->chunk_id, alloc->zone_id);
		existing_block = get_mblock_from_alloc(pop, alloc);
	}

	/* if allocation or reallocation, reserve new memory */
	if (size != 0) {
		/* reallocation to exactly the same size, which is a no-op */
		if (alloc != NULL && alloc->size == sizeh)
			goto out;

		if ((errno = alloc_reserve_block(pop,
			&new_block, sizeh)) != 0) {
			ret = -1;
			goto out;
		}
	}

	struct lane_alloc_layout *sec =
		(struct lane_alloc_layout *)lane->layout;

	/*
	 * The operation collects all of the required memory modifications that
	 * need to happen in an atomic way (all of them or none), and abstracts
	 * away the storage type (transient/persistent) and the underlying
	 * implementation of how it's actually performed - in some cases using
	 * the redo log is unnecessary and the allocation process can be sped up
	 * a bit by completely omitting that whole machinery.
	 *
	 * The modifications are not visible until the context is processed.
	 */
	struct operation_context ctx;
	operation_init(pop, &ctx, sec->redo);

	operation_add_entries(&ctx, entries, nentries);

	/*
	 * The offset value which is to be written to the destination pointer
	 * provided by the caller.
	 */
	uint64_t offset_value = 0;

	/* lock and persistently free the existing memory block */
	if (!MEMORY_BLOCK_IS_EMPTY(existing_block)) {
#ifdef DEBUG
		if (!heap_block_is_allocated(pop, existing_block)) {
			ERR("Double free or heap corruption");
			ASSERT(0);
		}
#endif /* DEBUG */

		/*
		 * This lock must be held until the operation is processed
		 * successfully, because other threads might operate on the
		 * same bitmap value.
		 */
		MEMBLOCK_OPS(AUTO, &existing_block)->lock(&existing_block, pop);

		/*
		 * This method will insert new entries into the operation
		 * context which will, after processing, update the chunk
		 * metadata to 'free' - it also takes care of all the necessary
		 * coalescing of blocks.
		 * Even though the transient state of the heap is used during
		 * this method to locate neighbouring blocks, it isn't modified.
		 *
		 * The rb block is the coalesced memory block that the free
		 * resulted in, to prevent volatile memory leak it needs to be
		 * inserted into the corresponding bucket.
		 */
		reclaimed_block = heap_free_block(pop, b, existing_block, &ctx);
		offset_value = 0;
	}

	if (!MEMORY_BLOCK_IS_EMPTY(new_block)) {
#ifdef DEBUG
		if (heap_block_is_allocated(pop, new_block)) {
			ERR("heap corruption");
			ASSERT(0);
		}
#endif /* DEBUG */

		if (alloc_prep_block(pop, new_block, constructor,
				arg, &offset_value) != 0) {
			/*
			 * Constructor returned non-zero value which means
			 * the memory block reservation has to be rolled back.
			 */
			struct bucket *new_bucket = heap_get_chunk_bucket(pop,
				new_block.chunk_id, new_block.zone_id);
			ASSERTne(new_bucket, NULL);

			/*
			 * Omitting the context in this method results in
			 * coalescing of blocks without affecting the persistent
			 * heap state.
			 */
			new_block = heap_free_block(pop, new_bucket,
					new_block, NULL);
			CNT_OP(new_bucket, insert, pop, new_block);

			if (new_bucket->type == BUCKET_RUN)
				heap_degrade_run_if_empty(pop,
					new_bucket, new_block);

			ret = -1;
			errno = ECANCELED;
			goto out;
		}

		/*
		 * This lock must be held for the duration between the creation
		 * of the allocation metadata updates in the operation context
		 * and the operation processing. This is because a different
		 * thread might operate on the same 8-byte value of the run
		 * bitmap and override allocation performed by this thread.
		 */
		MEMBLOCK_OPS(AUTO, &new_block)->lock(&new_block, pop);

		/*
		 * The actual required metadata modifications are chunk-type
		 * dependent, but it always is a modification of a single 8 byte
		 * value - either modification of few bits in a bitmap or
		 * changing a chunk type from free to used.
		 */
		MEMBLOCK_OPS(AUTO, &new_block)->prep_hdr(&new_block,
				pop, HDR_OP_ALLOC, &ctx);
	}

	/* not in-place realloc */
	if (!MEMORY_BLOCK_IS_EMPTY(existing_block) &&
		!MEMORY_BLOCK_IS_EMPTY(new_block)) {
		size_t old_size = alloc->size;
		size_t to_cpy = old_size > sizeh ? sizeh : old_size;
		pop->memcpy_persist(pop,
			OBJ_OFF_TO_PTR(pop, offset_value),
			OBJ_OFF_TO_PTR(pop, off),
			to_cpy - ALLOC_OFF);
	}

	/*
	 * If the caller provided a destination value to update, it needs to be
	 * modified atomically alongside the heap metadata, and so the operation
	 * context must be used.
	 * The actual offset value depends on whether the operation type.
	 */
	if (dest_off != NULL)
		operation_add_entry(&ctx, dest_off,
			offset_value, OPERATION_SET);

	operation_process(&ctx);

	/*
	 * After the operation succeeded, the persistent state is all in order
	 * but in some cases it might not be in-sync with the its transient
	 * representation.
	 */

	if (!MEMORY_BLOCK_IS_EMPTY(new_block)) {
		/* new block run lock */
		MEMBLOCK_OPS(AUTO, &new_block)->unlock(&new_block, pop);
	}

	if (!MEMORY_BLOCK_IS_EMPTY(existing_block)) {
		/* existing (freed) run lock */
		MEMBLOCK_OPS(AUTO, &existing_block)->unlock(
			&existing_block, pop);

		VALGRIND_DO_MEMPOOL_FREE(pop,
			(char *)heap_get_block_data(pop, existing_block)
			+ ALLOC_OFF);

		/* we might have been operating on inactive run */
		if (b != NULL) {
			/*
			 * Even though the initial condition is to check
			 * whether the existing block exists it's important to
			 * use the 'reclaimed block' - it is the coalesced one
			 * and reflects the current persistent heap state,
			 * whereas the existing block reflects the state from
			 * before this operation started.
			 */
			CNT_OP(b, insert, pop, reclaimed_block);
#ifdef DEBUG
			if (heap_block_is_allocated(pop, reclaimed_block)) {
				ERR("heap corruption");
				ASSERT(0);
			}
#endif /* DEBUG */
			/*
			 * Degrading of a run means turning it back into a chunk
			 * in case it's no longer needed.
			 * It might be tempting to defer this operation until
			 * such time that the chunk is actually needed, but
			 * right now the decision is to keep the persistent heap
			 * state as clean as possible - and that means not
			 * leaving unused data around.
			 */
			if (b->type == BUCKET_RUN)
				heap_degrade_run_if_empty(pop, b,
					reclaimed_block);
		}
	}

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

	return off_search + sizeof(struct allocation_header);
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

	return off_search + sizeof(struct allocation_header);
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
	struct lane_alloc_layout *sec =
		(struct lane_alloc_layout *)section;

	redo_log_recover(pop->redo, sec->redo, ALLOC_REDO_LOG_SIZE);

	return 0;
}

/*
 * lane_allocator_check -- consistency check of allocator lane section
 */
static int
lane_allocator_check(PMEMobjpool *pop, struct lane_section_layout *section)
{
	LOG(3, "allocator lane %p", section);

	struct lane_alloc_layout *sec =
		(struct lane_alloc_layout *)section;

	int ret = redo_log_check(pop->redo, sec->redo, ALLOC_REDO_LOG_SIZE);
	if (ret != 0)
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
