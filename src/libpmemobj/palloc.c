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
 * palloc.c -- implementation of pmalloc POSIX-like API
 *
 * This is the front-end part of the persistent memory allocator. It uses both
 * transient and persistent representation of the heap to provide memory blocks
 * in a reasonable time and with an acceptable common-case fragmentation.
 */

#include "valgrind_internal.h"
#include "heap_layout.h"
#include "heap.h"
#include "alloc_class.h"
#include "out.h"
#include "sys_util.h"
#include "palloc.h"

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
alloc_prep_block(struct palloc_heap *heap, const struct memory_block *m,
	palloc_constr constructor, void *arg,
	uint64_t extra_field, uint16_t flags,
	uint64_t *offset_value)
{
	void *uptr = m->m_ops->get_user_data(m);
	size_t usize = m->m_ops->get_user_size(m);

	VALGRIND_DO_MEMPOOL_ALLOC(heap->layout, uptr, usize);
	VALGRIND_DO_MAKE_MEM_UNDEFINED(uptr, usize);
	VALGRIND_ANNOTATE_NEW_MEMORY(uptr, usize);

	int ret;
	if (constructor != NULL &&
		(ret = constructor(heap->base, uptr, usize, arg)) != 0) {

		/*
		 * If canceled, revert the block back to the free state in vg
		 * machinery.
		 */
		VALGRIND_DO_MEMPOOL_FREE(heap->layout, uptr);

		return ret;
	}

	m->m_ops->write_header(m, extra_field, flags);

	/*
	 * To avoid determining the user data pointer twice this method is also
	 * responsible for calculating the offset of the object in the pool that
	 * will be used to set the offset destination pointer provided by the
	 * caller.
	 */
	*offset_value = HEAP_PTR_TO_OFF(heap, uptr);

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
 * There's an important distinction in the deallocation process - it does not
 * return the memory block to the transient container. That is done once no more
 * memory is available.
 *
 * Reallocation is a combination of the above, which one additional step
 * of copying the old content in the meantime.
 */
int
palloc_operation(struct palloc_heap *heap,
	uint64_t off, uint64_t *dest_off, size_t size,
	palloc_constr constructor, void *arg,
	uint64_t extra_field, uint16_t flags,
	struct operation_context *ctx)
{
	struct memory_block existing_block = MEMORY_BLOCK_NONE;
	struct memory_block new_block = MEMORY_BLOCK_NONE;

	struct bucket *existing_bucket = NULL;
	struct bucket *new_bucket = NULL;
	int ret = 0;

	/*
	 * These two lock are responsible for protecting the metadata for the
	 * persistent representation of a chunk. Depending on the operation and
	 * the type of a chunk, they might be NULL.
	 */
	os_mutex_t *existing_block_lock = NULL;
	os_mutex_t *new_block_lock = NULL;

	/*
	 * The offset value which is to be written to the destination pointer
	 * provided by the caller.
	 */
	uint64_t offset_value = 0;

	/*
	 * The first step in the allocation of a new block is reserving it in
	 * the transient heap - which is represented by the bucket abstraction.
	 *
	 * To provide optimal scaling for multi-threaded applications and reduce
	 * fragmentation the appropriate bucket is chosen depending on the
	 * current thread context and to which allocation class the requested
	 * size falls into.
	 *
	 * Once the bucket is selected, just enough memory is reserved for the
	 * requested size. The underlying block allocation algorithm
	 * (best-fit, next-fit, ...) varies depending on the bucket container.
	 */
	if (size != 0) {
		struct alloc_class *c = heap_get_best_class(heap, size);
		/*
		 * This bucket can only be released after the run lock is
		 * acquired.
		 * The reason for this is that the bucket can revoke the claim
		 * on the run during the heap_get_bestfit_block method which
		 * means the run will become available to others.
		 */
		new_bucket = heap_bucket_acquire(heap, c);

		/*
		 * The caller provided size in bytes, but buckets operate in
		 * 'size indexes' which are multiples of the block size in the
		 * bucket.
		 *
		 * For example, to allocate 500 bytes from a bucket that
		 * provides 256 byte blocks two memory 'units' are required.
		 */
		new_block.size_idx = CALC_SIZE_IDX(c->unit_size,
			size + header_type_to_size[c->header_type]);

		errno = heap_get_bestfit_block(heap, new_bucket, &new_block);
		if (errno != 0) {
			ret = -1;
			goto out;
		}

		/*
		 * The header type is changed in the transient memory block
		 * representation, but the actual header type as represented by
		 * the underlying chunk can be different. Only after the
		 * operation is processed, the transient and persistent
		 * representations will have matching header types.
		 */
		new_block.header_type = c->header_type;

		if (alloc_prep_block(heap, &new_block, constructor, arg,
			extra_field, flags, &offset_value) != 0) {
			/*
			 * Constructor returned non-zero value which means
			 * the memory block reservation has to be rolled back.
			 */
			if (new_block.type == MEMORY_BLOCK_HUGE) {
				new_block = heap_coalesce_huge(
					heap, new_bucket, &new_block);
				new_block.m_ops->prep_hdr(&new_block,
					MEMBLOCK_FREE, ctx);
				operation_process(ctx);
				bucket_insert_block(new_bucket, &new_block);
			}

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
		new_block_lock = new_block.m_ops->get_lock(&new_block);
		if (new_block_lock != NULL)
			util_mutex_lock(new_block_lock);

#ifdef DEBUG
		if (new_block.m_ops->get_state(&new_block) != MEMBLOCK_FREE) {
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
		new_block.m_ops->prep_hdr(&new_block, MEMBLOCK_ALLOCATED, ctx);
	}

	/*
	 * The offset of an existing block can be nonzero which means this
	 * operation is either free or a realloc - either way the offset of the
	 * object needs to be translated into structure that all of the heap
	 * methods operate in.
	 */
	if (off != 0) {
		existing_block = memblock_from_offset(heap, off);

		size_t user_size = existing_block.m_ops
			->get_user_size(&existing_block);

		/* reallocation to exactly the same size, which is a no-op */
		if (user_size == size)
			goto out;

		/* not in-place realloc */
		if (!MEMORY_BLOCK_IS_NONE(new_block)) {
			size_t old_size = user_size;
			size_t to_cpy = old_size > size ? size : old_size;
			VALGRIND_ADD_TO_TX(
				HEAP_OFF_TO_PTR(heap, offset_value),
				to_cpy);
			pmemops_memcpy_persist(&heap->p_ops,
				HEAP_OFF_TO_PTR(heap, offset_value),
				HEAP_OFF_TO_PTR(heap, off),
				to_cpy);
			VALGRIND_REMOVE_FROM_TX(
				HEAP_OFF_TO_PTR(heap, offset_value),
				to_cpy);
		}

		struct memory_block coalesced_block = existing_block;
		if (existing_block.type == MEMORY_BLOCK_HUGE) {
			if (new_bucket && new_bucket->aclass->id ==
						DEFAULT_ALLOC_CLASS_ID) {
				existing_bucket = new_bucket;
				new_bucket = NULL;
			} else {
				existing_bucket =
					heap_bucket_acquire_by_id(heap,
						DEFAULT_ALLOC_CLASS_ID);
			}

			coalesced_block = heap_coalesce_huge(heap,
				existing_bucket, &existing_block);
		}

		/*
		 * This lock must be held until the operation is processed
		 * successfully, because other threads might operate on the
		 * same bitmap value.
		 */
		existing_block_lock = existing_block.m_ops
			->get_lock(&existing_block);

		/* the locks might be identical in the case of realloc */
		if (existing_block_lock == new_block_lock)
			existing_block_lock = NULL;

		if (existing_block_lock != NULL)
			util_mutex_lock(existing_block_lock);

#ifdef DEBUG
		if (existing_block.m_ops->get_state(&existing_block) !=
				MEMBLOCK_ALLOCATED) {
			ERR("Double free or heap corruption");
			ASSERT(0);
		}
#endif /* DEBUG */

		VALGRIND_DO_MEMPOOL_FREE(heap->layout,
			(char *)existing_block.m_ops
				->get_user_data(&existing_block));

		/*
		 * This method will insert new entries into the operation
		 * context which will, after processing, update the chunk
		 * metadata to 'free'.
		 */
		existing_block = coalesced_block;
		existing_block.m_ops->prep_hdr(&existing_block,
			MEMBLOCK_FREE, ctx);
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
	if (existing_block.type == MEMORY_BLOCK_HUGE) {
		bucket_insert_block(existing_bucket, &existing_block);
	}

out:
	if (existing_block_lock != NULL)
		util_mutex_unlock(existing_block_lock);

	if (new_block_lock != NULL)
		util_mutex_unlock(new_block_lock);

	if (existing_bucket != NULL)
		heap_bucket_release(heap, existing_bucket);

	if (new_bucket != NULL)
		heap_bucket_release(heap, new_bucket);

	return ret;
}

/*
 * palloc_usable_size -- returns the number of bytes in the memory block
 */
size_t
palloc_usable_size(struct palloc_heap *heap, uint64_t off)
{
	struct memory_block m = memblock_from_offset(heap, off);

	return m.m_ops->get_user_size(&m);
}

/*
 * palloc_extra -- returns allocation extra field
 */
uint64_t
palloc_extra(struct palloc_heap *heap, uint64_t off)
{
	struct memory_block m = memblock_from_offset(heap, off);

	return m.m_ops->get_extra(&m);
}

/*
 * palloc_flags -- returns allocation flags
 */
uint16_t
palloc_flags(struct palloc_heap *heap, uint64_t off)
{
	struct memory_block m = memblock_from_offset(heap, off);

	return m.m_ops->get_flags(&m);
}

/*
 * pmalloc_search_cb -- (internal) foreach callback.
 */
static int
pmalloc_search_cb(const struct memory_block *m, void *arg)
{
	struct memory_block *out = arg;

	if (MEMORY_BLOCK_EQUALS(*m, *out))
		return 0; /* skip the same object */

	*out = *m;

	return 1;
}

/*
 * palloc_first -- returns the first object from the heap.
 */
uint64_t
palloc_first(struct palloc_heap *heap)
{
	struct memory_block search = MEMORY_BLOCK_NONE;

	heap_foreach_object(heap, pmalloc_search_cb,
		&search, MEMORY_BLOCK_NONE);

	if (MEMORY_BLOCK_IS_NONE(search))
		return 0;

	void *uptr = search.m_ops->get_user_data(&search);

	return HEAP_PTR_TO_OFF(heap, uptr);
}

/*
 * palloc_next -- returns the next object relative to 'off'.
 */
uint64_t
palloc_next(struct palloc_heap *heap, uint64_t off)
{
	struct memory_block m = memblock_from_offset(heap, off);
	struct memory_block search = m;

	heap_foreach_object(heap, pmalloc_search_cb, &search, m);

	if (MEMORY_BLOCK_IS_NONE(search) ||
		MEMORY_BLOCK_EQUALS(search, m))
		return 0;

	void *uptr = search.m_ops->get_user_data(&search);

	return HEAP_PTR_TO_OFF(heap, uptr);
}

/*
 * palloc_boot -- initializes allocator section
 */
int
palloc_boot(struct palloc_heap *heap, void *heap_start, uint64_t heap_size,
		uint64_t run_id, void *base, struct pmem_ops *p_ops)
{
	return heap_boot(heap, heap_start, heap_size, run_id, base, p_ops);
}

/*
 * palloc_buckets_init -- initialize buckets
 */
int
palloc_buckets_init(struct palloc_heap *heap)
{
	return heap_buckets_init(heap);
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
 * palloc_vg_register_alloc -- (internal) registers allocation header
 * in Valgrind
 */
static int
palloc_vg_register_alloc(const struct memory_block *m, void *arg)
{
	struct palloc_heap *heap = arg;

	m->m_ops->reinit_header(m);

	void *uptr = m->m_ops->get_user_data(m);
	size_t usize = m->m_ops->get_user_size(m);
	VALGRIND_DO_MEMPOOL_ALLOC(heap->layout, uptr, usize);
	VALGRIND_DO_MAKE_MEM_DEFINED(uptr, usize);

	return 0;
}

/*
 * palloc_heap_vg_open -- notifies Valgrind about heap layout
 */
void
palloc_heap_vg_open(struct palloc_heap *heap, int objects)
{
	heap_vg_open(heap, palloc_vg_register_alloc, heap, objects);
}
#endif
