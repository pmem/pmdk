// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2019, Intel Corporation */

/*
 * recycler.h -- internal definitions of run recycler
 *
 * This is a container that stores runs that are currently not used by any of
 * the buckets.
 */

#ifndef LIBPMEMOBJ_RECYCLER_H
#define LIBPMEMOBJ_RECYCLER_H 1

#include "memblock.h"
#include "vec.h"

#ifdef __cplusplus
extern "C" {
#endif

struct recycler;
VEC(empty_runs, struct memory_block);

struct recycler_element {
	uint32_t max_free_block;
	uint32_t free_space;

	uint32_t chunk_id;
	uint32_t zone_id;
};

struct recycler *recycler_new(struct palloc_heap *layout,
	size_t nallocs, size_t *peak_arenas);
void recycler_delete(struct recycler *r);
struct recycler_element recycler_element_new(struct palloc_heap *heap,
	const struct memory_block *m);

int recycler_put(struct recycler *r, const struct memory_block *m,
	struct recycler_element element);

int recycler_get(struct recycler *r, struct memory_block *m);

struct empty_runs recycler_recalc(struct recycler *r, int force);

void recycler_inc_unaccounted(struct recycler *r,
	const struct memory_block *m);

#ifdef __cplusplus
}
#endif

#endif
