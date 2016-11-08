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
 * palloc.c -- implementation of pmalloc POSIX-like API
 *
 * This is the front-end part of the persistent memory allocator. It uses both
 * transient and persistent representation of the heap to provide memory blocks
 * in a reasonable time and with an acceptable common-case fragmentation.
 */

#include "heap_layout.h"
#include "heap.h"
#include "out.h"
#include "sys_util.h"
#include "palloc.h"
#include "valgrind_internal.h"

/*
 * Number of bytes between beginning of memory block and beginning of user data.
 */
#define ALLOC_OFF (PALLOC_DATA_OFF + sizeof(struct allocation_header))

#define USABLE_SIZE(_a)\
((_a)->size - sizeof(struct allocation_header))

#define MEMORY_BLOCK_IS_EMPTY(_m)\
((_m).size_idx == 0)

#define PMALLOC_OFF_TO_PTR(heap, off) ((void *)((char *)((heap)->base) + (off)))
#define PMALLOC_PTR_TO_OFF(heap, ptr)\
	((uintptr_t)(ptr) - (uintptr_t)(heap->base))

#define ALLOC_GET_HEADER(_heap, _off) (struct allocation_header *)\
((char *)PMALLOC_OFF_TO_PTR((_heap), (_off)) - ALLOC_OFF)

/*
 * alloc_write_header -- (internal) creates allocation header
 */
static void
alloc_write_header(struct palloc_heap *heap, struct allocation_header *alloc,
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
get_mblock_from_alloc(struct palloc_heap *heap,
		struct allocation_header *alloc)
{
	struct memory_block m = {
		alloc->chunk_id,
		alloc->zone_id,
		0,
		0
	};

	uint64_t unit_size = MEMBLOCK_OPS(AUTO, &m)->
			block_size(&m, heap->layout);
	m.block_off = MEMBLOCK_OPS(AUTO, &m)->block_offset(&m, heap, alloc);
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
alloc_reserve_block(struct palloc_heap *heap, struct memory_block *m,
		size_t sizeh)
{
	struct bucket *b = heap_get_best_bucket(heap, sizeh);

	/*
	 * The caller provided size in bytes, but buckets operate in
	 * 'size indexes' which are multiples of the block size in the bucket.
	 *
	 * For example, to allocate 500 bytes from a bucket that provides 256
	 * byte blocks two memory 'units' are required.
	 */
	m->size_idx = b->calc_units(b, sizeh);

	int err = heap_get_bestfit_block(heap, b, m);

	if (err == ENOMEM && b->type == BUCKET_HUGE)
		return ENOMEM; /* there's only one huge bucket */

	if (err == ENOMEM) {
		/*
		 * There's no more available memory in the common heap and in
		 * this lane cache, fallback to the auxiliary (shared) bucket.
		 */
		b = heap_get_auxiliary_bucket(heap, sizeh);
		err = heap_get_bestfit_block(heap, b, m);
	}

	if (err == ENOMEM) {
		/*
		 * The auxiliary bucket cannot satisfy our request, borrow
		 * memory from other caches.
		 */
		heap_drain_to_auxiliary(heap, b, m->size_idx);
		err = heap_get_bestfit_block(heap, b, m);
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
alloc_prep_block(struct palloc_heap *heap, struct memory_block m,
	palloc_constr constructor, void *arg, uint64_t *offset_value)
{
	void *block_data = heap_get_block_data(heap, m);
	void *userdatap = (char *)block_data + ALLOC_OFF;

	uint64_t unit_size = MEMBLOCK_OPS(AUTO, &m)->
			block_size(&m, heap->layout);

	uint64_t real_size = unit_size * m.size_idx;

	ASSERT((uint64_t)block_data % ALLOC_BLOCK_SIZE == 0);
	ASSERT((uint64_t)userdatap % ALLOC_BLOCK_SIZE == 0);

	/* mark everything (including headers) as accessible */
	VALGRIND_DO_MAKE_MEM_UNDEFINED(block_data, real_size);
	/* mark space as allocated */
	VALGRIND_DO_MEMPOOL_ALLOC(heap->layout, userdatap,
			real_size - ALLOC_OFF);

	alloc_write_header(heap, block_data, m, real_size);

	int ret;
	if (constructor != NULL &&
		(ret = constructor(heap->base, userdatap,
			real_size - ALLOC_OFF, arg)) != 0) {

		/*
		 * If canceled, revert the block back to the free state in vg
		 * machinery. Because the free operation is only performed on
		 * the user data, the allocation header is made inaccessible
		 * in a separate call.
		 */
		VALGRIND_DO_MEMPOOL_FREE(heap->layout, userdatap);
		VALGRIND_DO_MAKE_MEM_NOACCESS(block_data, ALLOC_OFF);

		/*
		 * During this method there are several stores to pmem that are
		 * not immediately flushed and in case of a cancellation those
		 * stores are no longer relevant anyway.
		 */
		VALGRIND_SET_CLEAN(block_data, ALLOC_OFF);

		return ret;
	}

	/* flushes both the alloc and oob headers */
	pmemops_persist(&heap->p_ops, block_data, ALLOC_OFF);

	/*
	 * To avoid determining the user data pointer twice this method is also
	 * responsible for calculating the offset of the object in the pool that
	 * will be used to set the offset destination pointer provided by the
	 * caller.
	 */
	*offset_value = PMALLOC_PTR_TO_OFF(heap, userdatap);

	return 0;
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
palloc_operation(struct palloc_heap *heap,
	uint64_t off, uint64_t *dest_off, size_t size,
	palloc_constr constructor, void *arg,
	struct operation_context *ctx)
{
	struct bucket *b = NULL;
	struct allocation_header *alloc = NULL;
	struct memory_block existing_block = {0, 0, 0, 0};
	struct memory_block new_block = {0, 0, 0, 0};
	struct memory_block reclaimed_block = {0, 0, 0, 0};

	int ret = 0;

	/*
	 * These two lock are responsible for protecting the metadata for the
	 * persistent representation of a chunk. Depending on the operation and
	 * the type of a chunk, they might be NULL.
	 */
	pthread_mutex_t *existing_block_lock = NULL;
	pthread_mutex_t *new_block_lock = NULL;

	size_t sizeh = size + sizeof(struct allocation_header);

	/*
	 * The offset of an existing block can be nonzero which means this
	 * operation is either free or a realloc - either way the offset of the
	 * object needs to be translated into structure that all of the heap
	 * methods operate in.
	 */
	if (off != 0) {
		alloc = ALLOC_GET_HEADER(heap, off);
		existing_block = get_mblock_from_alloc(heap, alloc);
		/*
		 * This lock must be held until the operation is processed
		 * successfully, because other threads might operate on the
		 * same bitmap value.
		 */
		existing_block_lock = MEMBLOCK_OPS(AUTO, &existing_block)->
				get_lock(&existing_block, heap);
		if (existing_block_lock != NULL)
			util_mutex_lock(existing_block_lock);

#ifdef DEBUG
		if (MEMBLOCK_OPS(AUTO,
			&existing_block)->get_state(&existing_block, heap) !=
				MEMBLOCK_ALLOCATED) {
			ERR("Double free or heap corruption");
			ASSERT(0);
		}
#endif /* DEBUG */

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
		b = heap_get_chunk_bucket(heap, alloc->chunk_id,
				alloc->zone_id);
	}

	/* if allocation or reallocation, reserve new memory */
	if (size != 0) {
		/* reallocation to exactly the same size, which is a no-op */
		if (alloc != NULL && alloc->size == sizeh)
			goto out;

		errno = alloc_reserve_block(heap, &new_block, sizeh);
		if (errno != 0) {
			ret = -1;
			goto out;
		}
	}


	/*
	 * The offset value which is to be written to the destination pointer
	 * provided by the caller.
	 */
	uint64_t offset_value = 0;

	/* lock and persistently free the existing memory block */
	if (!MEMORY_BLOCK_IS_EMPTY(existing_block)) {
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
		reclaimed_block = heap_free_block(heap, b, existing_block, ctx);
		offset_value = 0;
	}

	if (!MEMORY_BLOCK_IS_EMPTY(new_block)) {
		if (alloc_prep_block(heap, new_block, constructor,
				arg, &offset_value) != 0) {
			/*
			 * Constructor returned non-zero value which means
			 * the memory block reservation has to be rolled back.
			 */
			struct bucket *new_bucket = heap_get_chunk_bucket(heap,
				new_block.chunk_id, new_block.zone_id);
			ASSERTne(new_bucket, NULL);

			/*
			 * Omitting the context in this method results in
			 * coalescing of blocks without affecting the persistent
			 * heap state.
			 */
			new_block = heap_free_block(heap, new_bucket,
					new_block, NULL);
			CNT_OP(new_bucket, insert, heap, new_block);

			if (new_bucket->type == BUCKET_RUN)
				heap_degrade_run_if_empty(heap,
					new_bucket, new_block);

			errno = ECANCELED;
			ret = -1;
			goto out;
		}

		/*
		 * This lock must be held for the duration between the creation
		 * of the allocation metadata updates in the operation context
		 * and the operation processing. This is because a different
		 * thread might operate on the same 8-byte value of the run
		 * bitmap and override allocation performed by this thread.
		 */
		new_block_lock = MEMBLOCK_OPS(AUTO, &new_block)->
			get_lock(&new_block, heap);

		/* the locks might be identical in the case of realloc */
		if (new_block_lock == existing_block_lock)
			new_block_lock = NULL;

		if (new_block_lock != NULL)
			util_mutex_lock(new_block_lock);

#ifdef DEBUG
		if (MEMBLOCK_OPS(AUTO,
			&new_block)->get_state(&new_block, heap) !=
				MEMBLOCK_FREE) {
			ERR("Double free or heap corruption");
			ASSERT(0);
		}
#endif /* DEBUG */

		/*
		 * The actual required metadata modifications are chunk-type
		 * dependent, but it always is a modification of a single 8 byte
		 * value - either modification of few bits in a bitmap or
		 * changing a chunk type from free to used.
		 */
		MEMBLOCK_OPS(AUTO, &new_block)->prep_hdr(&new_block,
				heap, MEMBLOCK_ALLOCATED, ctx);
	}

	/* not in-place realloc */
	if (!MEMORY_BLOCK_IS_EMPTY(existing_block) &&
		!MEMORY_BLOCK_IS_EMPTY(new_block)) {
		size_t old_size = alloc->size;
		size_t to_cpy = old_size > sizeh ? sizeh : old_size;
		VALGRIND_ADD_TO_TX(PMALLOC_OFF_TO_PTR(heap, offset_value),
			to_cpy - ALLOC_OFF);
		pmemops_memcpy_persist(&heap->p_ops,
			PMALLOC_OFF_TO_PTR(heap, offset_value),
			PMALLOC_OFF_TO_PTR(heap, off),
			to_cpy - ALLOC_OFF);
		VALGRIND_REMOVE_FROM_TX(PMALLOC_OFF_TO_PTR(heap, offset_value),
			to_cpy - ALLOC_OFF);
	}

	/*
	 * If the caller provided a destination value to update, it needs to be
	 * modified atomically alongside the heap metadata, and so the operation
	 * context must be used.
	 * The actual offset value depends on whether the operation type.
	 */
	if (dest_off != NULL)
		operation_add_entry(ctx, dest_off, offset_value, OPERATION_SET);

	operation_process(ctx);

	/*
	 * After the operation succeeded, the persistent state is all in order
	 * but in some cases it might not be in-sync with the its transient
	 * representation.
	 */
	if (!MEMORY_BLOCK_IS_EMPTY(existing_block)) {
		VALGRIND_DO_MEMPOOL_FREE(heap->layout,
			(char *)heap_get_block_data(heap, existing_block)
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
			CNT_OP(b, insert, heap, reclaimed_block);

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
				heap_degrade_run_if_empty(heap, b,
					reclaimed_block);
		}
	}

out:
	if (new_block_lock != NULL)
		util_mutex_unlock(new_block_lock);

	if (existing_block_lock != NULL)
		util_mutex_unlock(existing_block_lock);

	return ret;
}

/*
 * palloc_usable_size -- returns the number of bytes in the memory block
 */
size_t
palloc_usable_size(struct palloc_heap *heap, uint64_t off)
{
	return USABLE_SIZE(ALLOC_GET_HEADER(heap, off));
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
 * palloc_first -- returns the first object from the heap.
 */
uint64_t
palloc_first(struct palloc_heap *heap)
{
	uint64_t off_search = UINT64_MAX;
	struct memory_block m = {0, 0, 0, 0};

	heap_foreach_object(heap, pmalloc_search_cb, &off_search, m);

	if (off_search == UINT64_MAX)
		return 0;

	return off_search + sizeof(struct allocation_header);
}

/*
 * palloc_next -- returns the next object relative to 'off'.
 */
uint64_t
palloc_next(struct palloc_heap *heap, uint64_t off)
{
	struct allocation_header *alloc = ALLOC_GET_HEADER(heap, off);
	struct memory_block m = get_mblock_from_alloc(heap, alloc);

	uint64_t off_search = off - ALLOC_OFF;

	heap_foreach_object(heap, pmalloc_search_cb, &off_search, m);

	if (off_search == (off - ALLOC_OFF) ||
		off_search == 0 ||
		off_search == UINT64_MAX)
		return 0;

	return off_search + sizeof(struct allocation_header);
}

/*
 * palloc_boot -- initializes allocator section
 */
int
palloc_boot(struct palloc_heap *heap, void *heap_start, uint64_t heap_size,
		void *base, struct pmem_ops *p_ops)
{
	return heap_boot(heap, heap_start, heap_size, base, p_ops);
}

/*
 * palloc_init -- initializes palloc heap
 */
int
palloc_init(void *heap_start, uint64_t heap_size, struct pmem_ops *p_ops)
{
	return heap_init(heap_start, heap_size, p_ops);
}

/*
 * palloc_heap_end -- returns first address after heap
 */
void *
palloc_heap_end(struct palloc_heap *h)
{
	return heap_end(h);
}

/*
 * palloc_heap_check -- verifies heap state
 */
int
palloc_heap_check(void *heap_start, uint64_t heap_size)
{
	return heap_check(heap_start, heap_size);
}

/*
 * palloc_heap_check_remote -- verifies state of remote replica
 */
int
palloc_heap_check_remote(void *heap_start, uint64_t heap_size,
		struct remote_ops *ops)
{
	return heap_check_remote(heap_start, heap_size, ops);
}

/*
 * palloc_heap_cleanup -- cleanups the volatile heap state
 */
void
palloc_heap_cleanup(struct palloc_heap *heap)
{
	heap_cleanup(heap);
}

#ifdef USE_VG_MEMCHECK
/*
 * palloc_vg_register_object -- registers object in Valgrind
 */
void
palloc_vg_register_object(struct palloc_heap *heap, void *addr, size_t size)
{
	size_t headers = sizeof(struct allocation_header) + PALLOC_DATA_OFF;

	VALGRIND_DO_MEMPOOL_ALLOC(heap->layout, addr, size);
	VALGRIND_DO_MAKE_MEM_DEFINED((char *)addr - headers, size + headers);
}

/*
 * palloc_heap_vg_open -- notifies Valgrind about heap layout
 */
void
palloc_heap_vg_open(void *heap_start, uint64_t heap_size)
{
	heap_vg_open(heap_start, heap_size);
}
#endif
