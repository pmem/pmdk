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

#define MEMORY_BLOCK_IS_NONE(_m)\
((_m).size_idx == 0)

#define MEMORY_BLOCK_EQUALS(lhs, rhs)\
((lhs).zone_id == (rhs).zone_id && (lhs).chunk_id == (rhs).chunk_id &&\
(lhs).block_off == (rhs).block_off && (lhs).size_idx == (rhs).size_idx)

#define MEMORY_BLOCK_NONE \
(struct memory_block){0, 0, 0, 0}

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
	palloc_constr constructor, void *arg,
	uint64_t extra_field, uint16_t flags,
	struct alloc_class *c, size_t usize,
	uint64_t *offset_value)
{
	void *uptr = MEMBLOCK_OPS(AUTO, &m)
		->get_user_data_with_hdr_type(&m, c->header_type, heap);

	VALGRIND_DO_MAKE_MEM_UNDEFINED(uptr, usize);

	VALGRIND_DO_MEMPOOL_ALLOC(heap->layout, uptr, usize);

	int ret;
	if (constructor != NULL &&
		(ret = constructor(heap->base, uptr, usize, arg)) != 0) {

		/*
		 * If canceled, revert the block back to the free state in vg
		 * machinery. Because the free operation is only performed on
		 * the user data, the allocation header is made inaccessible
		 * in a separate call.
		 */
		VALGRIND_DO_MEMPOOL_FREE(heap->layout, uptr);

		return ret;
	}

	MEMBLOCK_OPS(AUTO, &m)->write_header(&m, heap, extra_field, flags, c->header_type);

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
	struct memory_block existing_block = {0, 0, 0, 0};
	struct memory_block new_block = {0, 0, 0, 0};
	enum memory_block_type existing_block_type = MAX_MEMORY_BLOCK;

	struct bucket *default_bucket = heap_get_default_bucket(heap);
	int ret = 0;

	/*
	 * These two lock are responsible for protecting the metadata for the
	 * persistent representation of a chunk. Depending on the operation and
	 * the type of a chunk, they might be NULL.
	 */
	pthread_mutex_t *existing_block_lock = NULL;
	pthread_mutex_t *new_block_lock = NULL;

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
	 * current thread context and to which allocation class the requested*
	 * size falls into.
	 *
	 * Once the bucket is selected, just enough memory is reserved for the
	 * requested size. The underlying block allocation algorithm
	 * (best-fit, next-fit, ...) varies depending on the bucket container.
	 */
	if (size != 0) {
		struct alloc_class *c = heap_get_best_class(heap, size);
		struct bucket *b = heap_get_bucket_by_class(heap, c);

		util_mutex_lock(&b->lock);

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

		errno = heap_get_bestfit_block(heap, b, &new_block);
		if (errno != 0) {
			util_mutex_unlock(&b->lock);
			ret = -1;
			goto out;
		}

		if (alloc_prep_block(heap, new_block, constructor, arg,
			extra_field, flags,
			c, size, &offset_value) != 0) {
			/*
			 * Constructor returned non-zero value which means
			 * the memory block reservation has to be rolled back.
			 */
			enum memory_block_type t = memblock_autodetect_type(
				&new_block, heap->layout);
			if (t == MEMORY_BLOCK_HUGE) {
				new_block = heap_coalesce_huge(heap, new_block);
				bucket_insert_block(b, heap, new_block);
			}

			util_mutex_unlock(&b->lock);
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
		new_block_lock = MEMBLOCK_OPS(AUTO, &new_block)
			->get_lock(&new_block, heap);

		if (new_block_lock != NULL)
			util_mutex_lock(new_block_lock);

		/*
		 * This lock can only be dropped after the run lock is acquired.
		 * The reason for this is that the bucket can revoke the claim
		 * on the run during the heap_get_bestfit_block method which
		 * means the run will become available to others.
		 */
		util_mutex_unlock(&b->lock);

#ifdef DEBUG
		if (MEMBLOCK_OPS(AUTO, &new_block)
			->get_state(&new_block, heap) != MEMBLOCK_FREE) {
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
		MEMBLOCK_OPS(AUTO, &new_block)
			->prep_hdr(&new_block, heap,
				MEMBLOCK_ALLOCATED, c->header_type, ctx);
	}

	/*
	 * The offset of an existing block can be nonzero which means this
	 * operation is either free or a realloc - either way the offset of the
	 * object needs to be translated into structure that all of the heap
	 * methods operate in.
	 */
	if (off != 0) {
		existing_block = memblock_from_offset(heap, off);

		size_t user_size = MEMBLOCK_OPS(AUTO, &existing_block)
			->get_user_size(&existing_block, heap);

		/* reallocation to exactly the same size, which is a no-op */
		if (user_size == size)
			goto out;

		/*
		 * This lock must be held until the operation is processed
		 * successfully, because other threads might operate on the
		 * same bitmap value.
		 */
		existing_block_lock = MEMBLOCK_OPS(AUTO, &existing_block)
			->get_lock(&existing_block, heap);

		/* the locks might be identical in the case of realloc */
		if (existing_block_lock == new_block_lock)
			existing_block_lock = NULL;

		if (existing_block_lock != NULL)
			util_mutex_lock(existing_block_lock);

		existing_block_type = memblock_autodetect_type(&existing_block,
			heap->layout);

#ifdef DEBUG
		if (MEMBLOCK_OPS(AUTO,
			&existing_block)->get_state(&existing_block, heap) !=
				MEMBLOCK_ALLOCATED) {
			ERR("Double free or heap corruption");
			ASSERT(0);
		}
#endif /* DEBUG */

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

		VALGRIND_DO_MEMPOOL_FREE(heap->layout,
			(char *)MEMBLOCK_OPS(AUTO, &existing_block)->
				get_user_data(&existing_block, heap));

		if (existing_block_type == MEMORY_BLOCK_HUGE) {
			util_mutex_lock(&default_bucket->lock);
			existing_block = heap_coalesce_huge(heap,
				existing_block);
			util_mutex_unlock(&default_bucket->lock);
		}

		/*
		 * This method will insert new entries into the operation
		 * context which will, after processing, update the chunk
		 * metadata to 'free'.
		 */
		MEMBLOCK_OPS(AUTO, &existing_block)
			->prep_hdr(&existing_block, heap,
				MEMBLOCK_FREE,
				0 /* hdr type doesn't matter for free */,
				ctx);
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
	if (!MEMORY_BLOCK_IS_NONE(existing_block)) {
		if (existing_block_type == MEMORY_BLOCK_HUGE) {
			util_mutex_lock(&default_bucket->lock);
			bucket_insert_block(default_bucket,
				heap, existing_block);
			util_mutex_unlock(&default_bucket->lock);
		}
	}

out:
	if (existing_block_lock != NULL)
		util_mutex_unlock(existing_block_lock);

	if (new_block_lock != NULL)
		util_mutex_unlock(new_block_lock);

	return ret;
}

/*
 * palloc_usable_size -- returns the number of bytes in the memory block
 */
size_t
palloc_usable_size(struct palloc_heap *heap, uint64_t off)
{
	struct memory_block m = memblock_from_offset(heap, off);

	return MEMBLOCK_OPS(AUTO, &m)->get_user_size(&m, heap);
}

/*
 * palloc_extra --
 */
uint64_t
palloc_extra(struct palloc_heap *heap, uint64_t off)
{
	struct memory_block m = memblock_from_offset(heap, off);

	return MEMBLOCK_OPS(AUTO, &m)->get_extra(&m, heap);
}

/*
 * palloc_flags --
 */
uint16_t
palloc_flags(struct palloc_heap *heap, uint64_t off)
{
	struct memory_block m = memblock_from_offset(heap, off);

	return MEMBLOCK_OPS(AUTO, &m)->get_flags(&m, heap);
}

/*
 * pmalloc_search_cb -- (internal) foreach callback. If the argument is equal
 *	to the current object offset then sets the argument to UINT64_MAX.
 *	If the argument is UINT64_MAX it breaks the iteration and sets the
 *	argument to the current object offset.
 */
static int
pmalloc_search_cb(const struct memory_block *m, void *arg)
{
	struct memory_block *out = arg;

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

	void *uptr = MEMBLOCK_OPS(AUTO, &search)->get_user_data(&search, heap);

	return HEAP_PTR_TO_OFF(heap, uptr);
}

/*
 * palloc_next -- returns the next object relative to 'off'.
 */
uint64_t
palloc_next(struct palloc_heap *heap, uint64_t off)
{
	struct memory_block m = memblock_from_offset(heap, off);
	struct memory_block search = MEMORY_BLOCK_NONE;

	heap_foreach_object(heap, pmalloc_search_cb, &search, m);

	if (MEMORY_BLOCK_IS_NONE(search) ||
		MEMORY_BLOCK_EQUALS(search, m))
		return 0;

	void *uptr = MEMBLOCK_OPS(AUTO, &search)->get_user_data(&search, heap);

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

	MEMBLOCK_OPS(AUTO, m)->reinit_header(m, heap);

	void *uptr = MEMBLOCK_OPS(AUTO, m)->get_user_data(m, heap);
	size_t usize = MEMBLOCK_OPS(AUTO, m)->get_user_size(m, heap);
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
