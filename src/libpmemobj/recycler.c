// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2021, Intel Corporation */

/*
 * recycler.c -- implementation of run recycler
 */

#include "heap.h"
#include "recycler.h"
#include "vec.h"
#include "out.h"
#include "util.h"
#include "sys_util.h"
#include "ravl.h"
#include "valgrind_internal.h"

#define THRESHOLD_MUL 4

/*
 * recycler_element_cmp -- compares two recycler elements
 */
static int
recycler_element_cmp(const void *lhs, const void *rhs)
{
	const struct recycler_element *l = lhs;
	const struct recycler_element *r = rhs;

	int64_t diff = (int64_t)l->max_free_block - (int64_t)r->max_free_block;
	if (diff != 0)
		return diff > 0 ? 1 : -1;

	diff = (int64_t)l->free_space - (int64_t)r->free_space;
	if (diff != 0)
		return diff > 0 ? 1 : -1;

	diff = (int64_t)l->zone_id - (int64_t)r->zone_id;
	if (diff != 0)
		return diff > 0 ? 1 : -1;

	diff = (int64_t)l->chunk_id - (int64_t)r->chunk_id;
	if (diff != 0)
		return diff > 0 ? 1 : -1;

	return 0;
}

struct recycler {
	struct ravl *runs;
	struct palloc_heap *heap;

	/*
	 * How many unaccounted units there *might* be inside of the memory
	 * blocks stored in the recycler.
	 * The value is not meant to be accurate, but rather a rough measure on
	 * how often should the memory block scores be recalculated.
	 *
	 * Per-chunk unaccounted units are shared for all zones, which might
	 * lead to some unnecessary recalculations.
	 */
	size_t unaccounted_units[MAX_CHUNK];
	size_t unaccounted_total;
	size_t nallocs;
	size_t *peak_arenas;

	VEC(, struct recycler_element) recalc;

	os_mutex_t lock;
};

/*
 * recycler_new -- creates new recycler instance
 */
struct recycler *
recycler_new(struct palloc_heap *heap, size_t nallocs, size_t *peak_arenas)
{
	struct recycler *r = Malloc(sizeof(struct recycler));
	if (r == NULL)
		goto error_alloc_recycler;

	r->runs = ravl_new_sized(recycler_element_cmp,
		sizeof(struct recycler_element));
	if (r->runs == NULL)
		goto error_alloc_tree;

	r->heap = heap;
	r->nallocs = nallocs;
	r->peak_arenas = peak_arenas;
	r->unaccounted_total = 0;
	memset(&r->unaccounted_units, 0, sizeof(r->unaccounted_units));

	VEC_INIT(&r->recalc);

	util_mutex_init(&r->lock);

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

	util_mutex_destroy(&r->lock);
	ravl_delete(r->runs);
	Free(r);
}

/*
 * recycler_element_new -- calculates how many free bytes does a run have and
 *	what's the largest request that the run can handle, returns that as
 *	recycler element struct
 */
struct recycler_element
recycler_element_new(struct palloc_heap *heap, const struct memory_block *m)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(heap);

	/*
	 * Counting of the clear bits can race with a concurrent deallocation
	 * that operates on the same run. This race is benign and has absolutely
	 * no effect on the correctness of this algorithm. Ideally, we would
	 * avoid grabbing the lock, but helgrind gets very confused if we
	 * try to disable reporting for this function.
	 */
	os_mutex_t *lock = m->m_ops->get_lock(m);
	util_mutex_lock(lock);

	struct recycler_element e = {
		.free_space = 0,
		.max_free_block = 0,
		.chunk_id = m->chunk_id,
		.zone_id = m->zone_id,
	};
	m->m_ops->calc_free(m, &e.free_space, &e.max_free_block);

	util_mutex_unlock(lock);

	return e;
}

/*
 * recycler_put -- inserts new run into the recycler
 */
int
recycler_put(struct recycler *r, struct recycler_element element)
{
	int ret = 0;

	util_mutex_lock(&r->lock);

	ret = ravl_emplace_copy(r->runs, &element);

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

	struct recycler_element e = { .max_free_block = m->size_idx, 0, 0, 0};
	struct ravl_node *n = ravl_find(r->runs, &e,
		RAVL_PREDICATE_GREATER_EQUAL);
	if (n == NULL) {
		ret = ENOMEM;
		goto out;
	}

	struct recycler_element *ne = ravl_data(n);
	m->chunk_id = ne->chunk_id;
	m->zone_id = ne->zone_id;

	ravl_remove(r->runs, n);

	struct chunk_header *hdr = heap_get_chunk_hdr(r->heap, m);
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

	uint64_t units = r->unaccounted_total;

	size_t peak_arenas;
	util_atomic_load64(r->peak_arenas, &peak_arenas);

	uint64_t recalc_threshold =
		THRESHOLD_MUL * peak_arenas * r->nallocs;

	if (!force && units < recalc_threshold)
		return runs;

	if (util_mutex_trylock(&r->lock) != 0)
		return runs;

	/* If the search is forced, recalculate everything */
	uint64_t search_limit = force ? UINT64_MAX : units;

	uint64_t found_units = 0;
	struct memory_block nm = MEMORY_BLOCK_NONE;
	struct ravl_node *n;
	struct recycler_element next = {0, 0, 0, 0};
	enum ravl_predicate p = RAVL_PREDICATE_GREATER_EQUAL;
	do {
		if ((n = ravl_find(r->runs, &next, p)) == NULL)
			break;

		p = RAVL_PREDICATE_GREATER;

		struct recycler_element *ne = ravl_data(n);
		next = *ne;

		uint64_t chunk_units = r->unaccounted_units[ne->chunk_id];
		if (!force && chunk_units == 0)
			continue;

		uint32_t existing_free_space = ne->free_space;

		nm.chunk_id = ne->chunk_id;
		nm.zone_id = ne->zone_id;
		memblock_rebuild_state(r->heap, &nm);

		struct recycler_element e = recycler_element_new(r->heap, &nm);

		ASSERT(e.free_space >= existing_free_space);
		uint64_t free_space_diff = e.free_space - existing_free_space;
		found_units += free_space_diff;

		if (free_space_diff == 0)
			continue;

		/*
		 * Decrease the per chunk_id counter by the number of nallocs
		 * found, increased by the blocks potentially freed in the
		 * active memory block. Cap the sub value to prevent overflow.
		 */
		util_fetch_and_sub64(&r->unaccounted_units[nm.chunk_id],
			MIN(chunk_units, free_space_diff + r->nallocs));

		ravl_remove(r->runs, n);

		if (e.free_space == r->nallocs) {
			memblock_rebuild_state(r->heap, &nm);
			if (VEC_PUSH_BACK(&runs, nm) != 0)
				ASSERT(0); /* XXX: fix after refactoring */
		} else {
			if (VEC_PUSH_BACK(&r->recalc, e) != 0)
				ASSERT(0); /* XXX: fix after refactoring */
		}
	} while (found_units < search_limit);

	struct recycler_element *e;
	VEC_FOREACH_BY_PTR(e, &r->recalc) {
		ravl_emplace_copy(r->runs, e);
	}

	VEC_CLEAR(&r->recalc);

	util_mutex_unlock(&r->lock);

	util_fetch_and_sub64(&r->unaccounted_total, units);

	return runs;
}

/*
 * recycler_inc_unaccounted -- increases the number of unaccounted units in the
 *	recycler
 */
void
recycler_inc_unaccounted(struct recycler *r, const struct memory_block *m)
{
	util_fetch_and_add64(&r->unaccounted_total, m->size_idx);
	util_fetch_and_add64(&r->unaccounted_units[m->chunk_id],
		m->size_idx);
}
