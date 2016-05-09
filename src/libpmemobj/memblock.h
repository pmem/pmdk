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
	 * Number of preceeding blocks in the chunk. In other words, the
	 * position of this memory block in run bitmap.
	 */
	uint16_t block_off;
};

enum memory_block_type {
	MEMORY_BLOCK_HUGE,
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

static const struct memory_block_ops mb_ops[MAX_MEMORY_BLOCK] = {
	[MEMORY_BLOCK_HUGE] = {
		.block_size = huge_block_size,
		.block_offset = huge_block_offset,
		.prep_hdr = huge_prep_operation_hdr,
		.lock = huge_lock,
		.unlock = huge_unlock,
	},
	[MEMORY_BLOCK_RUN] = {
		.block_size = run_block_size,
		.block_offset = run_block_offset,
		.prep_hdr = run_prep_operation_hdr,
		.lock = run_lock,
		.unlock = run_unlock,
	}
};

#define MEMBLOCK_OPS_AUTO(memblock, heap_layout)\
(&mb_ops[memblock_autodetect_type(memblock, heap_layout)])

#define MEMBLOCK_OPS_HUGE(memblock, heap_layout)\
(&mb_ops[MEMORY_BLOCK_HUGE])

#define MEMBLOCK_OPS_RUN(memblock, heap_layout)\
(&mb_ops[MEMORY_BLOCK_RUN])

#define MEMBLOCK_OPS_ MEMBLOCK_OPS_AUTO

#define MEMBLOCK_OPS(type, memblock)\
MEMBLOCK_OPS_##type(memblock, pop->hlayout)
