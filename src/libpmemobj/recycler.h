/*
 * Copyright 2016-2019, Intel Corporation
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
