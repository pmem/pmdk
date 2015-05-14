/*
 * Copyright (c) 2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 */

#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include "libpmem.h"
#include "libpmemobj.h"
#include "util.h"
#include "out.h"
#include "redo.h"
#include "heap_layout.h"
#include "bucket.h"
#include "ctree.h"
#include "heap.h"

/*
 * The elements in the tree are sorted by the key and it's vital that the
 * order is by size, hence the order of the pack arguments.
 */
#define	CHUNK_KEY_PACK(c, z, b, s)\
((uint64_t)(c) << 16 | (uint64_t)(z) << 32 | (uint64_t)(b) << 48 | (s))

#define	CHUNK_KEY_GET_CHUNK_ID(k)\
((uint16_t)((k & 0xFFFF0000) >> 16))

#define	CHUNK_KEY_GET_ZONE_ID(k)\
((uint16_t)((k & 0xFFFF00000000) >> 32))

#define	CHUNK_KEY_GET_BLOCK_OFF(k)\
((uint16_t)((k & 0xFFFF000000000000) >> 48))

#define	CHUNK_KEY_GET_SIZE_IDX(k)\
((uint16_t)((k & 0xFFFF)))

struct bucket {
	size_t unit_size;
	int unit_max;
	struct ctree *tree;
	pthread_mutex_t lock;
};

/*
 * bucket_new -- allocates and initializes bucket instance
 */
struct bucket *
bucket_new(size_t unit_size, int unit_max)
{
	ASSERT(unit_size > 0);

	struct bucket *b = Malloc(sizeof (*b));
	if (b == NULL)
		goto error_bucket_malloc;

	b->tree = ctree_new();
	if (b->tree == NULL)
		goto error_tree_new;

	if ((errno = pthread_mutex_init(&b->lock, NULL)) != 0) {
		LOG(1, "!pthread_mutex_init");
		goto error_mutex_init;
	}

	b->unit_size = unit_size;
	b->unit_max = unit_max;

	return b;

error_mutex_init:
	ctree_delete(b->tree);
error_tree_new:
	Free(b);
error_bucket_malloc:
	return NULL;
}

/*
 * bucket_delete -- cleanup and deallocate bucket instance
 */
void
bucket_delete(struct bucket *b)
{
	if ((errno = pthread_mutex_destroy(&b->lock)) != 0)
		LOG(1, "!pthread_mutex_destroy");

	ctree_delete(b->tree);
	Free(b);
}

/*
 * bucket_unit_size -- returns unit size of a bucket
 */
size_t
bucket_unit_size(struct bucket *b)
{
	return b->unit_size;
}

/*
 * bucket_is_small -- returns whether the bucket handles small allocations
 */
int
bucket_is_small(struct bucket *b)
{
	return b->unit_size != CHUNKSIZE;
}

/*
 * bucket_calc_units -- calculates the number of units requested size requires
 */
uint32_t
bucket_calc_units(struct bucket *b, size_t size)
{
	ASSERT(size != 0);
	return ((size - 1) / b->unit_size) + 1;
}

/*
 * bucket_insert_block -- inserts a new memory block into the container
 */
int
bucket_insert_block(struct bucket *b, uint32_t chunk_id, uint32_t zone_id,
	uint32_t size_idx, uint16_t block_off)
{
	ASSERT(chunk_id < MAX_CHUNK);
	ASSERT(zone_id < MAX_CHUNK);
	ASSERT(size_idx < MAX_CHUNK);

	uint64_t key = CHUNK_KEY_PACK(chunk_id, zone_id, block_off, size_idx);

	return ctree_insert(b->tree, key);
}

/*
 * bucket_get_rm_block -- removes and returns the best-fit memory block for size
 */
int
bucket_get_rm_block_bestfit(struct bucket *b, uint32_t *chunk_id,
	uint32_t *zone_id, uint32_t *size_idx, uint16_t *block_off)
{
	uint64_t key = CHUNK_KEY_PACK(0, 0, 0, *size_idx);
	if ((key = ctree_remove(b->tree, key, 0)) == 0)
		return ENOMEM;

	*chunk_id = CHUNK_KEY_GET_CHUNK_ID(key);
	*zone_id = CHUNK_KEY_GET_ZONE_ID(key);
	*block_off = CHUNK_KEY_GET_BLOCK_OFF(key);
	*size_idx = CHUNK_KEY_GET_SIZE_IDX(key);

	return 0;
}

/*
 * bucket_get_rm_block_exact -- removes exact match memory block
 */
int bucket_get_rm_block_exact(struct bucket *b, uint32_t chunk_id,
	uint32_t zone_id, uint32_t size_idx, uint16_t block_off)
{
	uint64_t key = CHUNK_KEY_PACK(chunk_id, zone_id, block_off, size_idx);
	if ((key = ctree_remove(b->tree, key, 1)) == 0)
		return ENOMEM;

	return 0;
}

/*
 * bucket_lock -- acquire bucket lock
 */
int
bucket_lock(struct bucket *b)
{
	return pthread_mutex_lock(&b->lock);
}

/*
 * bucket_unlock -- release bucket lock
 */
void
bucket_unlock(struct bucket *b)
{
	if ((errno = pthread_mutex_unlock(&b->lock)) != 0)
		LOG(1, "!pthread_mutex_unlock");
}
