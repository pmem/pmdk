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
 * bucket.h -- internal definitions for bucket
 */

#ifndef LIBPMEMOBJ_BUCKET_H
#define LIBPMEMOBJ_BUCKET_H 1

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include "container.h"
#include "memblock.h"

#define RUN_NALLOCS(_bs)\
((RUNSIZE / ((_bs))))

#define CALC_SIZE_IDX(_unit_size, _size)\
((uint32_t)(((_size - 1) / _unit_size) + 1))

enum bucket_type {
	BUCKET_UNKNOWN,
	BUCKET_HUGE,
	BUCKET_RUN,

	MAX_BUCKET_TYPE
};

struct bucket {
	enum bucket_type type;

	/*
	 * Identifier of this bucket in the heap's bucket map.
	 */
	uint8_t id;

	/*
	 * Size of a single memory block in bytes.
	 */
	size_t unit_size;

	uint32_t (*calc_units)(struct bucket *b, size_t size);

	pthread_mutex_t lock;

	struct block_container *container;
	struct block_container_ops *c_ops;
};

struct bucket_huge {
	struct bucket super;
};

struct bucket_run {
	struct bucket super;

	/*
	 * Last value of a bitmap representing completely free run from this
	 * bucket.
	 */
	uint64_t bitmap_lastval;

	/*
	 * Number of 8 byte values this run bitmap is composed of.
	 */
	unsigned bitmap_nval;

	/*
	 * Number of allocations that can be performed from a single run.
	 */
	unsigned bitmap_nallocs;

	/*
	 * Maximum multiplication factor of unit_size for memory blocks.
	 */
	unsigned unit_max;

	/*
	 * Maximum multiplication factor of unit_size for allocations.
	 * If a memory block is larger than the allowed size it is split and the
	 * remainder is returned back to the bucket.
	 */
	unsigned unit_max_alloc;

	struct memory_block active_memory_block;
	int is_active;
};

struct bucket_huge *bucket_huge_new(uint8_t id, struct block_container *c,
	size_t unit_size);

struct bucket_run *bucket_run_new(uint8_t id, struct block_container *c,
	size_t unit_size, unsigned unit_max, unsigned unit_max_alloc);

int bucket_insert_block(struct bucket *b, struct palloc_heap *heap,
		struct memory_block m);

void bucket_delete(struct bucket *b);

#endif
