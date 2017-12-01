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
 * memblock.h -- internal definitions for memory block
 */

#ifndef LIBPMEMOBJ_MEMBLOCK_H
#define LIBPMEMOBJ_MEMBLOCK_H 1

#include <stddef.h>
#include <stdint.h>

#include "os_thread.h"
#include "heap_layout.h"
#include "memops.h"
#include "palloc.h"

#define MEMORY_BLOCK_NONE \
(struct memory_block)\
{0, 0, 0, 0, 0, NULL, NULL, MAX_HEADER_TYPES, MAX_MEMORY_BLOCK}

#define MEMORY_BLOCK_IS_NONE(_m)\
((_m).heap == NULL)

#define MEMORY_BLOCK_EQUALS(lhs, rhs)\
((lhs).zone_id == (rhs).zone_id && (lhs).chunk_id == (rhs).chunk_id &&\
(lhs).block_off == (rhs).block_off && (lhs).heap == (rhs).heap)

enum memory_block_type {
	/*
	 * Huge memory blocks are directly backed by memory chunks. A single
	 * huge block can consist of several chunks.
	 * The persistent representation of huge memory blocks can be thought
	 * of as a doubly linked list with variable length elements.
	 * That list is stored in the chunk headers array where one element
	 * directly corresponds to one chunk.
	 *
	 * U - used, F - free, R - footer, . - empty
	 * |U| represents a used chunk with a size index of 1, with type
	 * information (CHUNK_TYPE_USED) stored in the corresponding header
	 * array element - chunk_headers[chunk_id].
	 *
	 * |F...R| represents a free chunk with size index of 5. The empty
	 * chunk headers have undefined values and shouldn't be used. All
	 * chunks with size larger than 1 must have a footer in the last
	 * corresponding header array - chunk_headers[chunk_id - size_idx - 1].
	 *
	 * The above representation of chunks will be used to describe the
	 * way fail-safety is achieved during heap operations.
	 *
	 * Allocation of huge memory block with size index 5:
	 * Initial heap state: |U| <> |F..R| <> |U| <> |F......R|
	 *
	 * The only block that matches that size is at very end of the chunks
	 * list: |F......R|
	 *
	 * As the request was for memory block of size 5, and this ones size is
	 * 7 there's a need to first split the chunk in two.
	 * 1) The last chunk header of the new allocation is marked as footer
	 *	and the block after that one is marked as free: |F...RF.R|
	 *	This is allowed and has no impact on the heap because this
	 *	modification is into chunk header that is otherwise unused, in
	 *	other words the linked list didn't change.
	 *
	 * 2) The size index of the first header is changed from previous value
	 *	of 7 to 5: |F...R||F.R|
	 *	This is a single fail-safe atomic operation and this is the
	 *	first change that is noticeable by the heap operations.
	 *	A single linked list element is split into two new ones.
	 *
	 * 3) The allocation process either uses redo log or changes directly
	 *	the chunk header type from free to used: |U...R| <> |F.R|
	 *
	 * In a similar fashion the reverse operation, free, is performed:
	 * Initial heap state: |U| <> |F..R| <> |F| <> |U...R| <> |F.R|
	 *
	 * This is the heap after the previous example with the single chunk
	 * in between changed from used to free.
	 *
	 * 1) Determine the neighbours of the memory block which is being
	 *	freed.
	 *
	 * 2) Update the footer (if needed) information of the last chunk which
	 *	is the memory block being freed or it's neighbour to the right.
	 *	|F| <> |U...R| <> |F.R << this one|
	 *
	 * 3) Update the size index and type of the left-most chunk header.
	 *	And so this: |F << this one| <> |U...R| <> |F.R|
	 *	becomes this: |F.......R|
	 *	The entire chunk header can be updated in a single fail-safe
	 *	atomic operation because it's size is only 64 bytes.
	 */
	MEMORY_BLOCK_HUGE,
	/*
	 * Run memory blocks are chunks with CHUNK_TYPE_RUN and size index of 1.
	 * The entire chunk is subdivided into smaller blocks and has an
	 * additional metadata attached in the form of a bitmap - each bit
	 * corresponds to a single block.
	 * In this case there's no need to perform any coalescing or splitting
	 * on the persistent metadata.
	 * The bitmap is stored on a variable number of 64 bit values and
	 * because of the requirement of allocation fail-safe atomicity the
	 * maximum size index of a memory block from a run is 64 - since that's
	 * the limit of atomic write guarantee.
	 *
	 * The allocation/deallocation process is a single 8 byte write that
	 * sets/clears the corresponding bits. Depending on the user choice
	 * it can either be made atomically or using redo-log when grouped with
	 * other operations.
	 * It's also important to note that in a case of realloc it might so
	 * happen that a single 8 byte bitmap value has its bits both set and
	 * cleared - that's why the run memory block metadata changes operate
	 * on AND'ing or OR'ing a bitmask instead of directly setting the value.
	 */
	MEMORY_BLOCK_RUN,

	MAX_MEMORY_BLOCK
};

enum memblock_state {
	MEMBLOCK_STATE_UNKNOWN,
	MEMBLOCK_ALLOCATED,
	MEMBLOCK_FREE,

	MAX_MEMBLOCK_STATE,
};

enum header_type {
	HEADER_LEGACY,
	HEADER_COMPACT,
	HEADER_NONE,

	MAX_HEADER_TYPES
};

extern const size_t header_type_to_size[MAX_HEADER_TYPES];
extern const enum chunk_flags header_type_to_flag[MAX_HEADER_TYPES];

struct memory_block_ops {
	size_t (*block_size)(const struct memory_block *m);
	void (*prep_hdr)(const struct memory_block *m,
		enum memblock_state dest_state, struct operation_context *ctx);
	os_mutex_t *(*get_lock)(const struct memory_block *m);
	enum memblock_state (*get_state)(const struct memory_block *m);
	void *(*get_user_data)(const struct memory_block *m);
	size_t (*get_user_size)(const struct memory_block *m);
	void *(*get_real_data)(const struct memory_block *m);
	size_t (*get_real_size)(const struct memory_block *m);
	void (*write_header)(const struct memory_block *m,
		uint64_t extra_field, uint16_t flags);
	void (*flush_header)(const struct memory_block *m);
	void (*invalidate_header)(const struct memory_block *m);
	void (*ensure_header_type)(const struct memory_block *m,
		enum header_type t);

	/* this is called for EVERY allocation, but *only* on valgrind */
	void (*reinit_header)(const struct memory_block *m);

	uint64_t (*get_extra)(const struct memory_block *m);
	uint16_t (*get_flags)(const struct memory_block *m);
};

struct memory_block {
	uint32_t chunk_id; /* index of the memory block in its zone */
	uint32_t zone_id; /* index of this block zone in the heap */

	/*
	 * Size index of the memory block represented in either multiple of
	 * CHUNKSIZE in the case of a huge chunk or in multiple of a run
	 * block size.
	 */
	uint32_t size_idx;

	/*
	 * Used only for run chunks, must be zeroed for huge.
	 * Number of preceding blocks in the chunk. In other words, the
	 * position of this memory block in run bitmap.
	 */
	uint16_t block_off;
	uint16_t padding;

	/*
	 * The variables below are associated with the memory block and are
	 * stored here for convenience. Those fields are filled by either the
	 * memblock_from_offset or memblock_rebuild_state, and they should not
	 * be modified manually.
	 */
	const struct memory_block_ops *m_ops;
	struct palloc_heap *heap;
	enum header_type header_type;
	enum memory_block_type type;
};

/*
 * This is a representation of a run memory block that is active in a bucket or
 * is on a pending list in the recycler.
 * This structure should never be passed around by value because the address of
 * the nresv variable can be in reservations made through palloc_reserve(). Only
 * if the number of reservations equals 0 the structure can be moved/freed.
 */
struct memory_block_reserved {
	struct memory_block m;

	/*
	 * Number of reservations made from this run, the pointer to this value
	 * is stored in a user facing pobj_action structure. Decremented once
	 * the reservation is published or canceled.
	 */
	int nresv;
};

enum memblock_state memblock_validate_offset(struct palloc_heap *heap,
	uint64_t off);
struct memory_block memblock_from_offset(struct palloc_heap *heap,
	uint64_t off);
void memblock_rebuild_state(struct palloc_heap *heap, struct memory_block *m);

#endif
