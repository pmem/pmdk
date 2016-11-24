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
 * bucket.c -- bucket implementation
 *
 * Buckets manage volatile state of the heap. They are the abstraction layer
 * between the heap-managed chunks/runs and memory allocations.
 *
 * Each bucket instance can have a different underlying container that is
 * responsible for selecting blocks - which means that whether the allocator
 * serves memory blocks in best/first/next -fit manner is decided during bucket
 * creation.
 */

#include "bucket.h"
#include "ctree.h"
#include "heap.h"
#include "out.h"
#include "sys_util.h"
#include "valgrind_internal.h"

/*
 * bucket_calc_units -- (internal) calculates the size index of a memory block
 *	whose size in bytes is equal or exceeds the value 'size' provided
 *	by the caller.
 */
static uint32_t
bucket_calc_units(struct bucket *b, size_t size)
{
	ASSERTne(size, 0);
	return CALC_SIZE_IDX(b->unit_size, size);
}

/*
 * bucket_init -- (internal) initializes bucket instance
 */
static int
bucket_init(struct bucket *b, uint8_t id, struct block_container *c,
	size_t unit_size)
{
	if (c == NULL)
		return -1;

	b->id = id;
	b->calc_units = bucket_calc_units;

	b->container = c;
	b->c_ops = c->c_ops;

	util_mutex_init(&b->lock, NULL);

	b->unit_size = unit_size;

	return 0;
}

/*
 * bucket_huge_new -- creates a huge bucket
 *
 * Huge bucket contains chunks with either free or used types. The only reason
 * there's a separate huge data structure is the bitmap information that is
 * required for runs and is not relevant for huge chunks.
 */
struct bucket_huge *
bucket_huge_new(uint8_t id, struct block_container *c, size_t unit_size)
{
	struct bucket_huge *b = Malloc(sizeof(*b));
	if (b == NULL)
		return NULL;

	if (bucket_init(&b->super, id, c, unit_size) != 0) {
		Free(b);
		return NULL;
	}

	b->super.type = BUCKET_HUGE;

	return b;
}

/*
 * bucket_run_new -- creates a run bucket
 *
 * This type of bucket is responsible for holding memory blocks from runs, which
 * means that each object it contains has a representation in a bitmap.
 *
 * The run buckets also contain the detailed information about the bitmap
 * all of the memory blocks contained within this container must be
 * represented by. This is not to say that a single bucket contains objects
 * only from a single chunk/bitmap - a bucket contains objects from a single
 * TYPE of bitmap run.
 */
struct bucket_run *
bucket_run_new(uint8_t id, struct block_container *c,
	size_t unit_size, unsigned unit_max, unsigned unit_max_alloc)
{
	struct bucket_run *b = Malloc(sizeof(*b));
	if (b == NULL)
		return NULL;

	if (bucket_init(&b->super, id, c, unit_size) != 0) {
		Free(b);
		return NULL;
	}

	b->super.type = BUCKET_RUN;
	b->unit_max = unit_max;
	b->unit_max_alloc = unit_max_alloc;
	b->is_active = 0;

	/*
	 * Here the bitmap definition is calculated based on the size of the
	 * available memory and the size of a memory block - the result of
	 * dividing those two numbers is the number of possible allocations from
	 * that block, and in other words, the amount of bits in the bitmap.
	 */
	ASSERT(RUN_NALLOCS(unit_size) <= UINT32_MAX);
	b->bitmap_nallocs = (unsigned)(RUN_NALLOCS(unit_size));

	/*
	 * The two other numbers that define our bitmap is the size of the
	 * array that represents the bitmap and the last value of that array
	 * with the bits that exceed number of blocks marked as set (1).
	 */
	ASSERT(b->bitmap_nallocs <= RUN_BITMAP_SIZE);
	unsigned unused_bits = RUN_BITMAP_SIZE - b->bitmap_nallocs;

	unsigned unused_values = unused_bits / BITS_PER_VALUE;

	ASSERT(MAX_BITMAP_VALUES >= unused_values);
	b->bitmap_nval = MAX_BITMAP_VALUES - unused_values;

	ASSERT(unused_bits >= unused_values * BITS_PER_VALUE);
	unused_bits -= unused_values * BITS_PER_VALUE;

	b->bitmap_lastval = unused_bits ?
		(((1ULL << unused_bits) - 1ULL) <<
			(BITS_PER_VALUE - unused_bits)) : 0;

	return b;
}

/*
 * bucket_insert_block -- inserts a block into the bucket
 */
int
bucket_insert_block(struct bucket *b, struct palloc_heap *heap,
	struct memory_block m)
{
#ifdef USE_VG_MEMCHECK
	if (On_valgrind) {
		size_t size = MEMBLOCK_OPS(AUTO, &m)->get_real_size(&m, heap);
		void *data = MEMBLOCK_OPS(AUTO, &m)->get_real_data(&m, heap);
		VALGRIND_MAKE_MEM_NOACCESS(data, size);
	}
#endif
	return b->c_ops->insert(b->container, heap, m);
}

/*
 * bucket_delete -- cleanups and deallocates bucket instance
 */
void
bucket_delete(struct bucket *b)
{
	util_mutex_destroy(&b->lock);
	b->c_ops->destroy(b->container);
	Free(b);
}
