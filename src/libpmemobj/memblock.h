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
 * memblock.h -- internal definitions for memory block
 */

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
};

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

enum memblock_hdr_op {
	HDR_OP_ALLOC,
	HDR_OP_FREE,

	MAX_MEMBLOCK_HDR_OP
};

size_t huge_block_size(struct memory_block *m, struct heap_layout *h);
size_t run_block_size(struct memory_block *m, struct heap_layout *h);

uint16_t huge_block_offset(struct memory_block *m, PMEMobjpool *pop, void *ptr);
uint16_t run_block_offset(struct memory_block *m, PMEMobjpool *pop, void *ptr);

void huge_prep_operation_hdr(struct memory_block *m, PMEMobjpool *pop,
	enum memblock_hdr_op op, struct operation_context *ctx);
void run_prep_operation_hdr(struct memory_block *m, PMEMobjpool *pop,
	enum memblock_hdr_op op, struct operation_context *ctx);

void huge_lock(struct memory_block *m, PMEMobjpool *pop);
void run_lock(struct memory_block *m, PMEMobjpool *pop);

void huge_unlock(struct memory_block *m, PMEMobjpool *pop);
void run_unlock(struct memory_block *m, PMEMobjpool *pop);

enum memory_block_type memblock_autodetect_type(struct memory_block *m,
	struct heap_layout *h);

struct memory_block_ops {
	size_t (*block_size)(struct memory_block *m, struct heap_layout *h);
	uint16_t (*block_offset)(struct memory_block *m, PMEMobjpool *pop,
		void *ptr);
	void (*prep_hdr)(struct memory_block *m, PMEMobjpool *pop,
		enum memblock_hdr_op, struct operation_context *ctx);
	void (*lock)(struct memory_block *m, PMEMobjpool *pop);
	void (*unlock)(struct memory_block *m, PMEMobjpool *pop);
};

extern const struct memory_block_ops mb_ops[MAX_MEMORY_BLOCK];

#define MEMBLOCK_OPS_AUTO(memblock, heap_layout)\
(&mb_ops[memblock_autodetect_type(memblock, heap_layout)])

#define MEMBLOCK_OPS_HUGE(memblock, heap_layout)\
(&mb_ops[MEMORY_BLOCK_HUGE])

#define MEMBLOCK_OPS_RUN(memblock, heap_layout)\
(&mb_ops[MEMORY_BLOCK_RUN])

#define MEMBLOCK_OPS_ MEMBLOCK_OPS_AUTO

#define MEMBLOCK_OPS(type, memblock)\
MEMBLOCK_OPS_##type(memblock, pop->hlayout)
