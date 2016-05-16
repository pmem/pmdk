/*
 * Copyright 2015-2016, Intel Corporation
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

#include <errno.h>
#include <pthread.h>

#include "libpmem.h"
#include "libpmemobj.h"
#include "util.h"
#include "out.h"
#include "sys_util.h"
#include "redo.h"
#include "heap_layout.h"
#include "memops.h"
#include "memblock.h"
#include "bucket.h"
#include "ctree.h"
#include "lane.h"
#include "pmalloc.h"
#include "heap.h"
#include "list.h"
#include "obj.h"
#include "valgrind_internal.h"

/*
 * The elements in the tree are sorted by the key and it's vital that the
 * order is by size, hence the order of the pack arguments.
 */
#define CHUNK_KEY_PACK(z, c, b, s)\
((uint64_t)(s) << 48 | (uint64_t)(b) << 32 | (uint64_t)(c) << 16 | (z))

#define CHUNK_KEY_GET_ZONE_ID(k)\
((uint16_t)((k & 0xFFFF)))

#define CHUNK_KEY_GET_CHUNK_ID(k)\
((uint16_t)((k & 0xFFFF0000) >> 16))

#define CHUNK_KEY_GET_BLOCK_OFF(k)\
((uint16_t)((k & 0xFFFF00000000) >> 32))

#define CHUNK_KEY_GET_SIZE_IDX(k)\
((uint16_t)((k & 0xFFFF000000000000) >> 48))

struct block_container_ctree {
	struct block_container super;
	struct ctree *tree;
};

#ifdef USE_VG_MEMCHECK
/*
 * bucket_vg_mark_noaccess -- (internal) marks memory block as no access for vg
 */
static void
bucket_vg_mark_noaccess(PMEMobjpool *pop, struct block_container *bc,
	struct memory_block m)
{
	if (On_valgrind) {
		size_t rsize = m.size_idx * bc->unit_size;
		void *block_data = heap_get_block_data(pop, m);
		VALGRIND_DO_MAKE_MEM_NOACCESS(pop, block_data, rsize);
	}
}
#endif

/*
 * bucket_tree_insert_block -- (internal) inserts a new memory block
 *	into the container
 */
static int
bucket_tree_insert_block(struct block_container *bc, PMEMobjpool *pop,
	struct memory_block m)
{
	/*
	 * Even though the memory block representation of an object uses
	 * relatively large types in practise the entire memory block structure
	 * needs to fit in a single 64 bit value - the type of the key in the
	 * container tree.
	 * Given those limitations a reasonable idea might be to make the
	 * memory_block structure be the size of single uint64_t, which would
	 * work for now, but if someday someone decides there's a need for
	 * larger objects the current implementation would allow them to simply
	 * replace this container instead of making little changes all over
	 * the heap code.
	 */
	ASSERT(m.chunk_id < MAX_CHUNK);
	ASSERT(m.zone_id < UINT16_MAX);
	ASSERTne(m.size_idx, 0);

	struct block_container_ctree *c = (struct block_container_ctree *)bc;

#ifdef USE_VG_MEMCHECK
	bucket_vg_mark_noaccess(pop, bc, m);
#endif

	uint64_t key = CHUNK_KEY_PACK(m.zone_id, m.chunk_id, m.block_off,
				m.size_idx);

	return ctree_insert(c->tree, key, 0);
}

/*
 * bucket_tree_get_rm_block_bestfit -- (internal) removes and returns the
 *	best-fit memory block for size
 */
static int
bucket_tree_get_rm_block_bestfit(struct block_container *bc,
	struct memory_block *m)
{
	uint64_t key = CHUNK_KEY_PACK(m->zone_id, m->chunk_id, m->block_off,
			m->size_idx);

	struct block_container_ctree *c = (struct block_container_ctree *)bc;

	if ((key = ctree_remove(c->tree, key, 0)) == 0)
		return ENOMEM;

	m->chunk_id = CHUNK_KEY_GET_CHUNK_ID(key);
	m->zone_id = CHUNK_KEY_GET_ZONE_ID(key);
	m->block_off = CHUNK_KEY_GET_BLOCK_OFF(key);
	m->size_idx = CHUNK_KEY_GET_SIZE_IDX(key);

	return 0;
}

/*
 * bucket_tree_get_rm_block_exact -- (internal) removes exact match memory block
 */
static int
bucket_tree_get_rm_block_exact(struct block_container *bc,
	struct memory_block m)
{
	uint64_t key = CHUNK_KEY_PACK(m.zone_id, m.chunk_id, m.block_off,
			m.size_idx);

	struct block_container_ctree *c = (struct block_container_ctree *)bc;

	if ((key = ctree_remove(c->tree, key, 1)) == 0)
		return ENOMEM;

	return 0;
}

/*
 * bucket_tree_get_block_exact -- (internal) finds exact match memory block
 */
static int
bucket_tree_get_block_exact(struct block_container *bc, struct memory_block m)
{
	uint64_t key = CHUNK_KEY_PACK(m.zone_id, m.chunk_id, m.block_off,
			m.size_idx);

	struct block_container_ctree *c = (struct block_container_ctree *)bc;

	return ctree_find(c->tree, key) == key ? 0 : ENOMEM;
}

/*
 * bucket_tree_is_empty -- (internal) checks whether the bucket is empty
 */
static int
bucket_tree_is_empty(struct block_container *bc)
{
	struct block_container_ctree *c = (struct block_container_ctree *)bc;
	return ctree_is_empty(c->tree);
}

/*
 * Tree-based block container used to provide best-fit functionality to the
 * bucket. The time complexity for this particular container is O(k) where k is
 * the length of the key.
 *
 * The get methods also guarantee that the block with lowest possible address
 * that best matches the requirements is provided.
 */
static struct block_container_ops container_ctree_ops = {
	.insert = bucket_tree_insert_block,
	.get_rm_exact = bucket_tree_get_rm_block_exact,
	.get_rm_bestfit = bucket_tree_get_rm_block_bestfit,
	.get_exact = bucket_tree_get_block_exact,
	.is_empty = bucket_tree_is_empty
};

/*
 * bucket_tree_create -- (internal) creates a new tree-based container
 */
static struct block_container *
bucket_tree_create(size_t unit_size)
{
	struct block_container_ctree *bc = Malloc(sizeof(*bc));
	if (bc == NULL)
		goto error_container_malloc;

	bc->super.type = CONTAINER_CTREE;
	bc->super.unit_size = unit_size;

	bc->tree = ctree_new();
	if (bc->tree == NULL)
		goto error_ctree_new;

	return &bc->super;

error_ctree_new:
	Free(bc);

error_container_malloc:
	return NULL;
}

/*
 * bucket_tree_delete -- (internal) deletes a tree container
 */
static void
bucket_tree_delete(struct block_container *bc)
{
	struct block_container_ctree *c = (struct block_container_ctree *)bc;
	ctree_delete(c->tree);
	Free(bc);
}

static struct {
	struct block_container_ops *ops;
	struct block_container *(*create)(size_t unit_size);
	void (*delete)(struct block_container *c);
} block_containers[MAX_CONTAINER_TYPE] = {
	{NULL, NULL, NULL},
	{&container_ctree_ops, bucket_tree_create, bucket_tree_delete},
};

/*
 * bucket_run_create -- (internal) creates a run bucket
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
static struct bucket *
bucket_run_create(size_t unit_size, unsigned unit_max)
{
	struct bucket_run *b = Malloc(sizeof(*b));
	if (b == NULL)
		return NULL;

	b->super.type = BUCKET_RUN;
	b->unit_max = unit_max;

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

	return &b->super;
}

/*
 * bucket_huge_create -- (internal) creates a huge bucket
 *
 * Huge bucket contains chunks with either free or used types. The only reason
 * there's a separate huge data structure is the bitmap information that is
 * required for runs and is not relevant for huge chunks.
 */
static struct bucket *
bucket_huge_create(size_t unit_size, unsigned unit_max)
{
	struct bucket_huge *b = Malloc(sizeof(*b));
	if (b == NULL)
		return NULL;

	b->super.type = BUCKET_HUGE;

	return &b->super;
}

/*
 * bucket_common_delete -- (internal) deletes a bucket
 */
static void
bucket_common_delete(struct bucket *b)
{
	Free(b);
}

static struct {
	struct bucket *(*create)(size_t unit_size, unsigned unit_max);
	void (*delete)(struct bucket *b);
} bucket_types[MAX_BUCKET_TYPE] = {
	{NULL, NULL},
	{bucket_huge_create, bucket_common_delete},
	{bucket_run_create, bucket_common_delete}
};

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
 * bucket_new -- allocates and initializes bucket instance
 */
struct bucket *
bucket_new(uint8_t id, enum bucket_type type, enum block_container_type ctype,
	size_t unit_size, unsigned unit_max)
{
	ASSERT(unit_size > 0);

	struct bucket *b = bucket_types[type].create(unit_size, unit_max);
	if (b == NULL)
		goto error_bucket_malloc;

	b->id = id;
	b->calc_units = bucket_calc_units;

	b->container = block_containers[ctype].create(unit_size);
	if (b->container == NULL)
		goto error_container_create;

	b->container->unit_size = unit_size;

	util_mutex_init(&b->lock, NULL);

	b->c_ops = block_containers[ctype].ops;
	b->unit_size = unit_size;

	return b;

error_container_create:
	bucket_types[type].delete(b);
error_bucket_malloc:
	return NULL;
}

/*
 * bucket_delete -- cleanups and deallocates bucket instance
 */
void
bucket_delete(struct bucket *b)
{
	util_mutex_destroy(&b->lock);

	block_containers[b->container->type].delete(b->container);
	bucket_types[b->type].delete(b);
}
