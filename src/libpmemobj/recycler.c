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
#include "vec.h"
#include "out.h"
#include "util.h"
#include "sys_util.h"
#include "ctree.h"
#include "valgrind_internal.h"

/*
 * The zone variable is offset by 1 to make sure the key is never 0.
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

#define RUN_KEY_GET_FREE_SPACE(k)\
((uint16_t)((k) >> 32))

#define THRESHOLD_MUL 2

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
	size_t nallocs;
	size_t recalc_threshold;
	int recalc_inprogress;

	VEC(, uint64_t) recalc;

	os_mutex_t lock;
};

/*
 * recycler_new -- creates new recycler instance
 */
struct recycler *
recycler_new(struct palloc_heap *heap, size_t nallocs)
{
	struct recycler *r = Malloc(sizeof(struct recycler));
	if (r == NULL)
		goto error_alloc_recycler;

	r->runs = ctree_new();
	if (r->runs == NULL)
		goto error_alloc_tree;

	r->heap = heap;
	r->nallocs = nallocs;
	r->recalc_threshold = nallocs * THRESHOLD_MUL;
	r->unaccounted_units = 0;
	r->recalc_inprogress = 0;
	VEC_INIT(&r->recalc);

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
	VEC_DELETE(&r->recalc);
	os_mutex_destroy(&r->lock);
	ctree_delete(r->runs);
	Free(r);
}

/*
 * recycler_calc_score -- calculates how many free bytes does a run have and
 *	what's the largest request that the run can handle
 */
uint64_t
recycler_calc_score(struct palloc_heap *heap, const struct memory_block *m,
	uint64_t *out_free_space)
{
	/*
	 * Counting of the clear bits can race with a concurrent deallocation
	 * that operates on the same run. This race is benign and has absolutely
	 * no effect on the correctness of this algorithm. Ideally, we would
	 * avoid grabbing the lock, but helgrind gets very confused if we
	 * try to disable reporting for this function.
	 */
	os_mutex_t *lock = m->m_ops->get_lock(m);
	os_mutex_lock(lock);

	struct zone *z = ZID_TO_ZONE(heap->layout, m->zone_id);
	struct chunk_run *run = (struct chunk_run *)&z->chunks[m->chunk_id];


	uint16_t free_space = 0;
	uint16_t max_block = 0;

	for (int i = 0; i < MAX_BITMAP_VALUES; ++i) {
		uint64_t value = ~run->bitmap[i];
		if (value == 0)
			continue;

		uint16_t free_in_value = util_popcount64(value);
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

	if (out_free_space != NULL)
		*out_free_space = free_space;

	os_mutex_unlock(lock);

	return RUN_KEY_PACK(m->zone_id, m->chunk_id, free_space, max_block);
}

/*
 * recycler_put -- inserts new run into the recycler
 */
int
recycler_put(struct recycler *r, const struct memory_block *m, uint64_t score)
{
	int ret = 0;

	util_mutex_lock(&r->lock);

	ret = ctree_insert_unlocked(r->runs, score, 0);

	util_mutex_unlock(&r->lock);

	return ret;
}

/*
 * recycler_get -- retrieves a chunk from the recycler
 */
int
recycler_get(struct recycler *r, struct memory_block *m)
{
	int ret = 0;

	util_mutex_lock(&r->lock);

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
 * recycler_recalc -- recalculates the scores of runs in the recycler to match
 *	the updated persistent state
 */
struct empty_runs
recycler_recalc(struct recycler *r, int force)
{
	struct empty_runs runs;
	VEC_INIT(&runs);

	uint64_t units = r->unaccounted_units;

	if (r->recalc_inprogress || (!force && units < (r->recalc_threshold)))
		return runs;

	if (!util_bool_compare_and_swap32(&r->recalc_inprogress, 0, 1))
		return runs;

	util_mutex_lock(&r->lock);

	/* If the search is forced, recalculate everything */
	uint64_t search_limit = force ? UINT64_MAX : units;

	uint64_t found_units = 0;
	uint64_t free_space = 0;
	struct memory_block nm = MEMORY_BLOCK_NONE;
	uint64_t key;
	do {
		if ((key = ctree_remove_unlocked(r->runs, 0, 0)) == 0)
			break;

		nm.chunk_id = RUN_KEY_GET_CHUNK_ID(key);
		nm.zone_id = RUN_KEY_GET_ZONE_ID(key);
		uint64_t key_free_space = RUN_KEY_GET_FREE_SPACE(key);
		memblock_rebuild_state(r->heap, &nm);

		uint64_t score = recycler_calc_score(r->heap, &nm, &free_space);

		ASSERT(free_space >= key_free_space);
		uint64_t free_space_diff = free_space - key_free_space;
		found_units += free_space_diff;

		if (free_space == r->nallocs) {
			memblock_rebuild_state(r->heap, &nm);
			VEC_PUSH_BACK(&runs, nm);
		} else {
			VEC_PUSH_BACK(&r->recalc, score);
		}
	} while (found_units < search_limit);

	VEC_FOREACH(key, &r->recalc) {
		ctree_insert_unlocked(r->runs, key, 0);
	}

	VEC_CLEAR(&r->recalc);

	util_mutex_unlock(&r->lock);

	util_fetch_and_sub64(&r->unaccounted_units, units);
	int ret = util_bool_compare_and_swap32(&r->recalc_inprogress, 1, 0);
	ASSERTeq(ret, 1);

	return runs;
}

/*
 * recycler_inc_unaccounted -- increases the number of unaccounted units in the
 *	recycler
 */
void
recycler_inc_unaccounted(struct recycler *r, const struct memory_block *m)
{
	util_fetch_and_add64(&r->unaccounted_units, m->size_idx);
}
