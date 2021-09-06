/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

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

#ifdef __cplusplus
extern "C" {
#endif

#define MEMORY_BLOCK_NONE \
(struct memory_block)\
{0, 0, 0, 0, NULL, NULL, MAX_HEADER_TYPES, MAX_MEMORY_BLOCK, NULL}

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
	 * 1) Determine the neighbors of the memory block which is being
	 *	freed.
	 *
	 * 2) Update the footer (if needed) information of the last chunk which
	 *	is the memory block being freed or it's neighbor to the right.
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

/* runtime bitmap information for a run */
struct run_bitmap {
	unsigned nvalues; /* number of 8 byte values - size of values array */
	unsigned nbits; /* number of valid bits */

	size_t size; /* total size of the bitmap in bytes */

	uint64_t *values; /* pointer to the bitmap's values array */
};

/* runtime information necessary to create a run */
struct run_descriptor {
	uint16_t flags; /* chunk flags for the run */
	size_t unit_size; /* the size of a single unit in a run */
	uint32_t size_idx; /* size index of a single run instance */
	size_t alignment; /* required alignment of objects */
	unsigned nallocs; /* number of allocs per run */
	struct run_bitmap bitmap;
};

struct memory_block_ops {
	/* returns memory block size */
	size_t (*block_size)(const struct memory_block *m);

	/* prepares header modification operation */
	void (*prep_hdr)(const struct memory_block *m,
		enum memblock_state dest_state, struct operation_context *ctx);

	/* returns lock associated with memory block */
	os_mutex_t *(*get_lock)(const struct memory_block *m);

	/* returns whether a block is allocated or not */
	enum memblock_state (*get_state)(const struct memory_block *m);

	/* returns pointer to the data of a block */
	void *(*get_user_data)(const struct memory_block *m);

	/*
	 * Returns the size of a memory block without overhead.
	 * This is the size of a data block that can be used.
	 */
	size_t (*get_user_size)(const struct memory_block *m);

	/* returns pointer to the beginning of data of a run block */
	void *(*get_real_data)(const struct memory_block *m);

	/* returns the size of a memory block, including headers */
	size_t (*get_real_size)(const struct memory_block *m);

	/* writes a header of an allocation */
	void (*write_header)(const struct memory_block *m,
		uint64_t extra_field, uint16_t flags);
	void (*invalidate)(const struct memory_block *m);

	/*
	 * Checks the header type of a chunk matches the expected type and
	 * modifies it if necessary. This is fail-safe atomic.
	 */
	void (*ensure_header_type)(const struct memory_block *m,
		enum header_type t);

	/*
	 * Reinitializes a block after a heap restart.
	 * This is called for EVERY allocation, but *only* under Valgrind.
	 */
	void (*reinit_header)(const struct memory_block *m);

	/* returns the extra field of an allocation */
	uint64_t (*get_extra)(const struct memory_block *m);

	/* returns the flags of an allocation */
	uint16_t (*get_flags)(const struct memory_block *m);

	/* initializes memblock in valgrind */
	void (*vg_init)(const struct memory_block *m, int objects,
		object_callback cb, void *arg);

	/* iterates over every free block */
	int (*iterate_free)(const struct memory_block *m,
		object_callback cb, void *arg);

	/* iterates over every used block */
	int (*iterate_used)(const struct memory_block *m,
		object_callback cb, void *arg);

	/* calculates number of free units, valid only for runs */
	void (*calc_free)(const struct memory_block *m,
		uint32_t *free_space, uint32_t *max_free_block);

	/* this is called exactly once for every existing chunk */
	void (*reinit_chunk)(const struct memory_block *m);

	/*
	 * Initializes bitmap data for a run.
	 * Do *not* use this function unless absolutely necessary, it breaks
	 * the abstraction layer by exposing implementation details.
	 */
	void (*get_bitmap)(const struct memory_block *m, struct run_bitmap *b);

	/* calculates the ratio between occupied and unoccupied space */
	unsigned (*fill_pct)(const struct memory_block *m);
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
	uint32_t block_off;

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
	struct run_bitmap *cached_bitmap;
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

	struct bucket *bucket;
	/*
	 * Number of reservations made from this run, the pointer to this value
	 * is stored in a user facing pobj_action structure. Decremented once
	 * the reservation is published or canceled.
	 */
	int nresv;
};

struct memory_block memblock_from_offset(struct palloc_heap *heap,
	uint64_t off);
struct memory_block memblock_from_offset_opt(struct palloc_heap *heap,
	uint64_t off, int size);
void memblock_rebuild_state(struct palloc_heap *heap, struct memory_block *m);

struct memory_block memblock_huge_init(struct palloc_heap *heap,
	uint32_t chunk_id, uint32_t zone_id, uint32_t size_idx);

struct memory_block memblock_run_init(struct palloc_heap *heap,
	uint32_t chunk_id, uint32_t zone_id, struct run_descriptor *rdsc);

void memblock_run_bitmap(uint32_t *size_idx, uint16_t flags,
	uint64_t unit_size, uint64_t alignment, void *content,
	struct run_bitmap *b);

#ifdef __cplusplus
}
#endif

#endif
