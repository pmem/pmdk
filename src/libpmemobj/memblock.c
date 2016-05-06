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
 * memblock.c -- implementation of memory block
 *
 * Memory block is a representation of persistent object that resides in the
 * heap. A valid memory block must be either a huge (free or used) chunk or a
 * block inside a run.
 *
 * Huge blocks are 1:1 correlated with the chunk headers in the zone whereas
 * run blocks are represented by bits in corresponding chunk bitmap.
 *
 * This file contains implementations of abstract operations on memory blocks.
 * Instead of storing the mbops structure inside each memory block the correct
 * method implementation is chosen at runtime.
 */

#include <stdlib.h>
#include <inttypes.h>
#include "libpmemobj.h"
#include "util.h"
#include "lane.h"
#include "redo.h"
#include "memops.h"
#include "pmalloc.h"
#include "list.h"
#include "obj.h"
#include "out.h"
#include "heap_layout.h"
#include "memblock.h"
#include "heap.h"

/*
 * memblock_autodetect_type -- looks for the corresponding chunk header and
 *	depending on the chunks type returns the right memory block type.
 */
enum memory_block_type
memblock_autodetect_type(struct memory_block *m, struct heap_layout *h)
{
	switch (ZID_TO_ZONE(h, m->zone_id)->chunk_headers[m->chunk_id].type) {
		case CHUNK_TYPE_RUN:
			return MEMORY_BLOCK_RUN;
		case CHUNK_TYPE_FREE:
		case CHUNK_TYPE_USED:
			return MEMORY_BLOCK_HUGE;
		default:
			/* unreachable */
			FATAL("possible zone chunks metadata corruption");
	}
}

/*
 * huge_block_size -- returns the compile-time constant which defines the
 *	huge memory block size.
 */
size_t
huge_block_size(struct memory_block *m, struct heap_layout *h)
{
	return CHUNKSIZE;
}

/*
 * run_block_size -- looks for the right chunk and returns the block size
 *	information that is attached to the run block metadata.
 */
size_t
run_block_size(struct memory_block *m, struct heap_layout *h)
{
	struct zone *z = ZID_TO_ZONE(h, m->zone_id);
	struct chunk_run *run = (struct chunk_run *)&z->chunks[m->chunk_id];

	return run->block_size;
}
