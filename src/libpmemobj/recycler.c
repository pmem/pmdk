/*
 * Copyright 2017, Intel Corporation
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
 * recycler.c -- implementation of run recycler
 */

#include "recycler.h"
#include "out.h"
#include "util.h"
#include "sys_util.h"
#include "ctree.h"

/*
 * The zone variable is offseted by 1 to make sure the key is never 0.
 * XXX: The tree API should change so that a 0 key is valid, once that happens,
 * this workaround can be removed.
 */
#define RUN_KEY_PACK(z, c, free_space, max_block)\
((uint64_t)(max_block) << 48 |\
(uint64_t)(free_space) << 32 |\
(uint64_t)(c) << 16 | (z + 1))

#define RUN_KEY_GET_ZONE_ID(k)\
((uint16_t)(((int16_t)(k)) - 1))

#define RUN_KEY_GET_CHUNK_ID(k)\
((uint16_t)((k) >> 16))

struct recycler_element {
	uint32_t chunk_id;
	uint32_t zone_id;
};

struct recycler {
	struct ctree *runs;
	struct palloc_heap *heap;

	/*
	 * How many unaccounted units there *might* be inside of the memory
	 * blocks stored in the recycler.
	 * The value is not meant to be accurate, but rather a rough measure on
	 * how often should the memory block scores be recalculated.
	 */
	size_t unaccounted_units;
	size_t recalc_threshold;

	os_mutex_t lock;
};

/*
 * recycler_new -- creates new recycler instance
 */
struct recycler *
recycler_new(struct palloc_heap *heap, size_t recalc_threshold)
{
	struct recycler *r = Malloc(sizeof(struct recycler));
	if (r == NULL)
		goto error_alloc_recycler;

	r->runs = ctree_new();
	if (r->runs == NULL)
		goto error_alloc_tree;

	r->heap = heap;
	r->recalc_threshold = recalc_threshold;

	os_mutex_init(&r->lock);

	return r;

error_alloc_tree:
	Free(r);
error_alloc_recycler:
	return NULL;
}

/*
 * recycler_delete -- deletes recycler instance
 */
void
recycler_delete(struct recycler *r)
{
	ctree_delete(r->runs);
	Free(r);
}

/*
 * recycler_calc_score -- calculates how many free bytes does a run have and
 *	what's the largest request that the run can handle
 */
static uint64_t
recycler_calc_score(struct recycler *r, const struct memory_block *m)
{
	struct zone *z = ZID_TO_ZONE(r->heap->layout, m->zone_id);
	struct chunk_run *run = (struct chunk_run *)&z->chunks[m->chunk_id];

	uint16_t free_space = 0;
	uint16_t max_block = 0;

	for (int i = 0; i < MAX_BITMAP_VALUES; ++i) {
		uint64_t value = ~run->bitmap[i];
		if (value == 0)
			continue;

		uint16_t free_in_value = (uint16_t)__builtin_popcountll(value);
		free_space = (uint16_t)(free_space + free_in_value);

		/*
		 * If this value has less free blocks than already found max,
		 * there's no point in searching.
		 */
		if (free_in_value < max_block)
			continue;

		/* if the entire value is empty, no point in searching */
		if (free_in_value == BITS_PER_VALUE) {
			max_block = BITS_PER_VALUE;
			continue;
		}

		/*
		 * Find the biggest free block in the bitmap.
		 * This algorithm is not the most clever imaginable, but it's
		 * easy to implement and fast enough.
		 */
		uint16_t n = 0;
		while (value != 0) {
			value &= (value << 1ULL);
			n++;
		}

		if (n > max_block)
			max_block = n;
	}

	return RUN_KEY_PACK(m->zone_id, m->chunk_id, free_space, max_block);
}

/*
 * recycler_put -- inserts new run into the recycler
 */
int
recycler_put(struct recycler *r, const struct memory_block *m)
{
	int ret = 0;

	util_mutex_lock(&r->lock);

	uint64_t score = recycler_calc_score(r, m);

	ret = ctree_insert_unlocked(r->runs, score, 0);

	util_mutex_unlock(&r->lock);

	return ret;
}

/*
 * recycler_recalc_block -- the ctree delete callback. Each node represents a
 *	a run whose score will be recalculated and then inserted into the new
 *	tree.
 */
static void
recycler_recalc_block(uint64_t key, uint64_t value, void *ctx)
{
	struct memory_block m;
	m.chunk_id = RUN_KEY_GET_CHUNK_ID(key);
	m.zone_id = RUN_KEY_GET_ZONE_ID(key);
	struct recycler *r = ctx;
	uint64_t score = recycler_calc_score(r, &m);
	ctree_insert_unlocked(r->runs, score, 0);
}

/*
 * recycler_recalc_if_needed -- (internal) if the unaccounted units exceed the
 *	threshold, the entire tree is rebuilt from scratch and the run scores
 *	are recalculated.
 */
static int
recycler_recalc_if_needed(struct recycler *r)
{
	if (r->unaccounted_units < r->recalc_threshold)
		return 0;

	size_t units = r->unaccounted_units;

	struct ctree *old = r->runs;
	if ((r->runs = ctree_new()) == NULL) {
		r->runs = old;
		return -1;
	}

	ctree_delete_cb(old, recycler_recalc_block, r);

	util_fetch_and_sub(&r->unaccounted_units, units);

	return 0;
}

/*
 * recycler_get -- retrieves a chunk from the recycler
 */
int
recycler_get(struct recycler *r, struct memory_block *m)
{
	int ret = 0;

	util_mutex_lock(&r->lock);

	if (recycler_recalc_if_needed(r) != 0) {
		ERR("unable to recreate recycler tree");
		/* continue /w degraded effiency */
	}

	uint64_t key = RUN_KEY_PACK(0, 0, 0, m->size_idx);
	if ((key = ctree_remove_unlocked(r->runs, key, 0)) == 0) {
		ret = ENOMEM;
		goto out;
	}

	m->chunk_id = RUN_KEY_GET_CHUNK_ID(key);
	m->zone_id = RUN_KEY_GET_ZONE_ID(key);

	struct zone *z = ZID_TO_ZONE(r->heap->layout, m->zone_id);
	struct chunk_header *hdr = &z->chunk_headers[m->chunk_id];
	m->size_idx = hdr->size_idx;

	memblock_rebuild_state(r->heap, m);

out:
	util_mutex_unlock(&r->lock);

	return ret;
}

/*
 * recycler_inc_unaccounted -- increases the number of unaccounted units in the
 *	recycler
 */
void
recycler_inc_unaccounted(struct recycler *r, const struct memory_block *m)
{
	util_fetch_and_add(&r->unaccounted_units, m->size_idx);
}
