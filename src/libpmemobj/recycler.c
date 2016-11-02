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

#include <sys/queue.h>

#include "recycler.h"
#include "out.h"
#include "util.h"
#include "sys_util.h"
#include "ctree.h"

struct recycler_element {
	uint32_t chunk_id;
	uint32_t zone_id;
};

struct recycler {
	struct ctree *runs;
	struct palloc_heap *heap;
};

struct recycler *
recycler_new(struct palloc_heap *heap)
{
	struct recycler *r = Malloc(sizeof(struct recycler));
	if (r == NULL)
		goto error_alloc_tree;

	r->runs = ctree_new();
	if (r->runs == NULL)
		goto error_alloc_recycler;

	r->heap = heap;

	return r;

error_alloc_tree:
	Free(r);
error_alloc_recycler:
	return NULL;
}

void
recycler_delete(struct recycler *r)
{
	ctree_delete(r->runs);
	Free(r);
}

static uint64_t
recycler_calc_score(struct recycler *r, const struct memory_block *m)
{
	struct zone *z = ZID_TO_ZONE(r->heap->layout, m->zone_id);
	struct chunk_run *run = (struct chunk_run *)&z->chunks[m->chunk_id];

	uint64_t score = 0;
	for (int i = 0; i < MAX_BITMAP_VALUES; ++i)
		if (run->bitmap[i] != UINT64_MAX)
			score += !__builtin_popcountll(run->bitmap[i]);

	return score;
}

/* to make sure that the 0, 0 is recongizable, the zone id is bumped by one */
#define RUN_VALUE_PACK(c, z)\
((uint64_t)(c) << 32 | ((z) + 1))

#define RUN_VALUE_GET_CHUNK_ID(v)\
((uint32_t)((v) >> 32))

#define RUN_VALUE_GET_ZONE_ID(v)\
((uint32_t)((v) - 1))

int
recycler_put(struct recycler *r, const struct memory_block *m)
{
	uint64_t score = recycler_calc_score(r, m);
	uint64_t value = RUN_VALUE_PACK(m->chunk_id, m->zone_id);

	return ctree_insert(r->runs, score, value);
}

int
recycler_get(struct recycler *r, struct memory_block *m)
{
	uint64_t score;
	uint64_t value;
	ctree_remove_max(r->runs, &score, &value);
	if (value == 0)
		return ENOMEM;

	m->chunk_id = RUN_VALUE_GET_CHUNK_ID(value);
	m->zone_id = RUN_VALUE_GET_ZONE_ID(value);

	struct zone *z = ZID_TO_ZONE(r->heap->layout, m->zone_id);
	struct chunk_header *hdr = &z->chunk_headers[m->chunk_id];
	m->size_idx = hdr->size_idx;

	return 0;
}
