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
 * heap.c -- heap implementation
 */

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <float.h>

#include "queue.h"
#include "heap.h"
#include "out.h"
#include "util.h"
#include "sys_util.h"
#include "valgrind_internal.h"
#include "recycler.h"
#include "container_ctree.h"
#include "container_seglists.h"
#include "alloc_class.h"
#include "os_thread.h"

/* calculates the size of the entire run, including any additional chunks */
#define SIZEOF_RUN(runp, size_idx) (sizeof(*run) + ((size_idx - 1) * CHUNKSIZE))

#define MAX_RUN_LOCKS 1024

/*
 * Arenas store the collection of buckets for allocation classes. Each thread
 * is assigned an arena on its first allocator operation.
 */
struct arena {
	/* one bucket per allocation class */
	struct bucket *buckets[MAX_ALLOCATION_CLASSES];

	size_t nthreads;
};

struct heap_rt {
	struct alloc_class_collection *alloc_classes;

	/* DON'T use these two variable directly! */
	struct bucket *default_bucket;
	struct arena *arenas;

	/* protects assignment of arenas */
	os_mutex_t arenas_lock;

	/* stores a pointer to one of the arenas */
	os_tls_key_t thread_arena;

	struct recycler *recyclers[MAX_ALLOCATION_CLASSES];

	os_mutex_t run_locks[MAX_RUN_LOCKS];
	unsigned max_zone;
	unsigned zones_exhausted;
	unsigned narenas;
};

/*
 * heap_alloc_classes -- returns the allocation classes collection
 */
struct alloc_class_collection *
heap_alloc_classes(struct palloc_heap *heap)
{
	return heap->rt->alloc_classes;
}


/*
 * heap_arena_init -- (internal) initializes arena instance
 */
static void
heap_arena_init(struct arena *arena)
{
	arena->nthreads = 0;

	for (int i = 0; i < MAX_ALLOCATION_CLASSES; ++i)
		arena->buckets[i] = NULL;
}

/*
 * heap_arena_destroy -- (internal) destroys arena instance
 */
static void
heap_arena_destroy(struct arena *arena)
{
	for (int i = 0; i < MAX_ALLOCATION_CLASSES; ++i)
		if (arena->buckets[i] != NULL)
			bucket_delete(arena->buckets[i]);
}

/*
 * heap_get_best_class -- returns the alloc class that best fits the
 *	requested size
 */
struct alloc_class *
heap_get_best_class(struct palloc_heap *heap, size_t size)
{
	return alloc_class_by_alloc_size(heap->rt->alloc_classes, size);
}

/*
 * heap_thread_arena_destructor -- (internal) removes arena thread assignment
 */
static void
heap_thread_arena_destructor(void *arg)
{
	struct arena *a = arg;
	util_fetch_and_sub(&a->nthreads, 1);
}

/*
 * heap_thread_arena_assign -- (internal) assigns the least used arena
 *	to current thread
 *
 * To avoid complexities with regards to races in the search for the least
 * used arena, a lock is used, but the nthreads counter of the arena is still
 * bumped using atomic instruction because it can happen in parallel to a
 * destructor of a thread, which also touches that variable.
 */
static struct arena *
heap_thread_arena_assign(struct heap_rt *heap)
{
	os_mutex_lock(&heap->arenas_lock);

	struct arena *least_used = NULL;

	struct arena *a;
	for (unsigned i = 0; i < heap->narenas; ++i) {
		a = &heap->arenas[i];
		if (least_used == NULL || a->nthreads < least_used->nthreads)
			least_used = a;
	}

	LOG(4, "assigning %p arena to current thread", least_used);

	util_fetch_and_add(&least_used->nthreads, 1);

	os_mutex_unlock(&heap->arenas_lock);

	os_tls_set(heap->thread_arena, least_used);

	return least_used;
}

/*
 * heap_thread_arena -- (internal) returns the arena assigned to the current
 *	thread
 */
static struct arena *
heap_thread_arena(struct heap_rt *heap)
{
	struct arena *a;
	if ((a = os_tls_get(heap->thread_arena)) == NULL)
		a = heap_thread_arena_assign(heap);

	return a;
}

/*
 * heap_bucket_acquire_by_id -- fetches by id a bucket exclusive for the thread
 *	until heap_bucket_release is called
 */
struct bucket *
heap_bucket_acquire_by_id(struct palloc_heap *heap, uint8_t class_id)
{
	struct heap_rt *rt = heap->rt;
	struct bucket *b;

	if (class_id == DEFAULT_ALLOC_CLASS_ID) {
		b = rt->default_bucket;
	} else {
		struct arena *arena = heap_thread_arena(heap->rt);
		b = arena->buckets[class_id];
	}

	util_mutex_lock(&b->lock);

	return b;
}

/*
 * heap_bucket_acquire_by_id -- fetches by class a bucket exclusive for the
 *	thread until heap_bucket_release is called
 */
struct bucket *
heap_bucket_acquire(struct palloc_heap *heap, struct alloc_class *c)
{
	return heap_bucket_acquire_by_id(heap, c->id);
}

/*
 * heap_bucket_release -- puts the bucket back into the heap
 */
void
heap_bucket_release(struct palloc_heap *heap, struct bucket *b)
{
	util_mutex_unlock(&b->lock);
}

/*
 * heap_get_run_lock -- returns the lock associated with memory block
 */
os_mutex_t *
heap_get_run_lock(struct palloc_heap *heap, uint32_t chunk_id)
{
	return &heap->rt->run_locks[chunk_id % MAX_RUN_LOCKS];
}

/*
 * heap_max_zone -- (internal) calculates how many zones can the heap fit
 */
static unsigned
heap_max_zone(size_t size)
{
	unsigned max_zone = 0;
	size -= sizeof(struct heap_header);

	while (size >= ZONE_MIN_SIZE) {
		max_zone++;
		size -= size <= ZONE_MAX_SIZE ? size : ZONE_MAX_SIZE;
	}

	return max_zone;
}

/*
 * get_zone_size_idx -- (internal) calculates zone size index
 */
static uint32_t
get_zone_size_idx(uint32_t zone_id, unsigned max_zone, size_t heap_size)
{
	ASSERT(max_zone > 0);
	if (zone_id < max_zone - 1)
		return MAX_CHUNK;

	ASSERT(heap_size >= zone_id * ZONE_MAX_SIZE);
	size_t zone_raw_size = heap_size - zone_id * ZONE_MAX_SIZE;

	ASSERT(zone_raw_size >= (sizeof(struct zone_header) +
			sizeof(struct chunk_header) * MAX_CHUNK));
	zone_raw_size -= sizeof(struct zone_header) +
		sizeof(struct chunk_header) * MAX_CHUNK;

	size_t zone_size_idx = zone_raw_size / CHUNKSIZE;
	ASSERT(zone_size_idx <= UINT32_MAX);

	return (uint32_t)zone_size_idx;
}

/*
 * heap_chunk_write_footer -- writes a chunk footer
 */
static void
heap_chunk_write_footer(struct chunk_header *hdr, uint32_t size_idx)
{
	if (size_idx == 1) /* that would overwrite the header */
		return;

	VALGRIND_DO_MAKE_MEM_UNDEFINED(hdr + size_idx - 1, sizeof(*hdr));

	struct chunk_header f = *hdr;
	f.type = CHUNK_TYPE_FOOTER;
	f.size_idx = size_idx;
	*(hdr + size_idx - 1) = f;
	/* no need to persist, footers are recreated in heap_populate_buckets */
	VALGRIND_SET_CLEAN(hdr + size_idx - 1, sizeof(f));
}

/*
 * heap_chunk_init -- (internal) writes chunk header
 */
static void
heap_chunk_init(struct palloc_heap *heap, struct chunk_header *hdr,
	uint16_t type, uint32_t size_idx)
{
	struct chunk_header nhdr = {
		.type = type,
		.flags = 0,
		.size_idx = size_idx
	};
	VALGRIND_DO_MAKE_MEM_UNDEFINED(hdr, sizeof(*hdr));

	*hdr = nhdr; /* write the entire header (8 bytes) at once */
	pmemops_persist(&heap->p_ops, hdr, sizeof(*hdr));

	heap_chunk_write_footer(hdr, size_idx);
}

/*
 * heap_zone_init -- (internal) writes zone's first chunk and header
 */
static void
heap_zone_init(struct palloc_heap *heap, uint32_t zone_id)
{
	struct zone *z = ZID_TO_ZONE(heap->layout, zone_id);
	uint32_t size_idx = get_zone_size_idx(zone_id, heap->rt->max_zone,
			heap->size);

	heap_chunk_init(heap, &z->chunk_headers[0], CHUNK_TYPE_FREE, size_idx);

	struct zone_header nhdr = {
		.size_idx = size_idx,
		.magic = ZONE_HEADER_MAGIC,
	};
	z->header = nhdr;  /* write the entire header (8 bytes) at once */
	pmemops_persist(&heap->p_ops, &z->header, sizeof(z->header));
}

/*
 * heap_run_init -- (internal) creates a run based on a chunk
 */
static void
heap_run_init(struct palloc_heap *heap, struct bucket *b,
	const struct memory_block *m)
{
	struct alloc_class *c = b->aclass;
	ASSERTeq(c->type, CLASS_RUN);

	struct zone *z = ZID_TO_ZONE(heap->layout, m->zone_id);

	struct chunk_run *run = (struct chunk_run *)&z->chunks[m->chunk_id];
	ASSERTne(m->size_idx, 0);
	size_t runsize = SIZEOF_RUN(run, m->size_idx);

	VALGRIND_DO_MAKE_MEM_UNDEFINED(run, runsize);

	/* add/remove chunk_run and chunk_header to valgrind transaction */
	VALGRIND_ADD_TO_TX(run, runsize);
	run->block_size = c->unit_size;
	pmemops_persist(&heap->p_ops, &run->block_size,
			sizeof(run->block_size));

	/* set all the bits */
	memset(run->bitmap, 0xFF, sizeof(run->bitmap));

	unsigned nval = c->run.bitmap_nval;
	ASSERT(nval > 0);
	/* clear only the bits available for allocations from this bucket */
	memset(run->bitmap, 0, sizeof(uint64_t) * (nval - 1));
	run->bitmap[nval - 1] = c->run.bitmap_lastval;

	run->incarnation_claim = heap->run_id;
	VALGRIND_SET_CLEAN(&run->incarnation_claim,
		sizeof(run->incarnation_claim));

	VALGRIND_REMOVE_FROM_TX(run, runsize);

	pmemops_persist(&heap->p_ops, run->bitmap, sizeof(run->bitmap));

	struct chunk_header run_data_hdr;
	run_data_hdr.type = CHUNK_TYPE_RUN_DATA;
	run_data_hdr.flags = 0;

	struct chunk_header *data_hdr;
	for (unsigned i = 1; i < m->size_idx; ++i) {
		data_hdr = &z->chunk_headers[m->chunk_id + i];
		VALGRIND_DO_MAKE_MEM_UNDEFINED(data_hdr, sizeof(*data_hdr));
		VALGRIND_ADD_TO_TX(data_hdr, sizeof(*data_hdr));
		run_data_hdr.size_idx = i;
		*data_hdr = run_data_hdr;
		VALGRIND_REMOVE_FROM_TX(data_hdr, sizeof(*data_hdr));
	}
	pmemops_persist(&heap->p_ops,
		&z->chunk_headers[m->chunk_id + 1],
		sizeof(struct chunk_header) * (m->size_idx - 1));

	struct chunk_header *hdr = &z->chunk_headers[m->chunk_id];
	ASSERT(hdr->type == CHUNK_TYPE_FREE);

	VALGRIND_ADD_TO_TX(hdr, sizeof(*hdr));
	struct chunk_header run_hdr;
	run_hdr.size_idx = hdr->size_idx;
	run_hdr.type = CHUNK_TYPE_RUN;
	run_hdr.flags = header_type_to_flag[c->header_type];
	*hdr = run_hdr;
	VALGRIND_REMOVE_FROM_TX(hdr, sizeof(*hdr));

	pmemops_persist(&heap->p_ops, hdr, sizeof(*hdr));
}

/*
 * heap_run_insert -- (internal) inserts and splits a block of memory into a run
 */
static void
heap_run_insert(struct palloc_heap *heap, struct bucket *b,
	const struct memory_block *m, uint32_t size_idx, uint16_t block_off)
{
	struct alloc_class *c = b->aclass;
	ASSERTeq(c->type, CLASS_RUN);

	ASSERT(size_idx <= BITS_PER_VALUE);
	ASSERT(block_off + size_idx <= c->run.bitmap_nallocs);

	uint32_t unit_max = RUN_UNIT_MAX;
	struct memory_block nm = *m;
	nm.size_idx = unit_max - (block_off % unit_max);
	nm.block_off = block_off;
	if (nm.size_idx > size_idx)
		nm.size_idx = size_idx;

	do {
		bucket_insert_block(b, &nm);
		ASSERT(nm.size_idx <= UINT16_MAX);
		ASSERT(nm.block_off + nm.size_idx <= UINT16_MAX);
		nm.block_off = (uint16_t)(nm.block_off + (uint16_t)nm.size_idx);
		size_idx -= nm.size_idx;
		nm.size_idx = size_idx > unit_max ? unit_max : size_idx;
	} while (size_idx != 0);
}

/*
 * heap_process_run_metadata -- (internal) parses the run bitmap
 */
static uint32_t
heap_process_run_metadata(struct palloc_heap *heap, struct bucket *b,
	const struct memory_block *m)
{
	struct alloc_class *c = b->aclass;
	ASSERTeq(c->type, CLASS_RUN);
	ASSERTeq(m->size_idx, c->run.size_idx);

	uint16_t block_off = 0;
	uint16_t block_size_idx = 0;
	uint32_t inserted_blocks = 0;

	struct zone *z = ZID_TO_ZONE(heap->layout, m->zone_id);
	struct chunk_run *run = (struct chunk_run *)&z->chunks[m->chunk_id];

	ASSERTeq(run->block_size, c->unit_size);

	for (unsigned i = 0; i < c->run.bitmap_nval; ++i) {
		ASSERT(i < MAX_BITMAP_VALUES);
		uint64_t v = run->bitmap[i];
		ASSERT(BITS_PER_VALUE * i <= UINT16_MAX);
		block_off = (uint16_t)(BITS_PER_VALUE * i);
		if (v == 0) {
			heap_run_insert(heap, b, m, BITS_PER_VALUE, block_off);
			inserted_blocks += BITS_PER_VALUE;
			continue;
		} else if (v == UINT64_MAX) {
			continue;
		}

		for (unsigned j = 0; j < BITS_PER_VALUE; ++j) {
			if (BIT_IS_CLR(v, j)) {
				block_size_idx++;
			} else if (block_size_idx != 0) {
				ASSERT(block_off >= block_size_idx);

				heap_run_insert(heap, b, m,
					block_size_idx,
					(uint16_t)(block_off - block_size_idx));
				inserted_blocks += block_size_idx;
				block_size_idx = 0;
			}

			if ((block_off++) == c->run.bitmap_nallocs) {
				i = MAX_BITMAP_VALUES;
				break;
			}
		}

		if (block_size_idx != 0) {
			ASSERT(block_off >= block_size_idx);

			heap_run_insert(heap, b, m,
					block_size_idx,
					(uint16_t)(block_off - block_size_idx));
			inserted_blocks += block_size_idx;
			block_size_idx = 0;
		}
	}

	return inserted_blocks;
}

/*
 * heap_create_run -- (internal) initializes a new run on an existing free chunk
 */
static void
heap_create_run(struct palloc_heap *heap, struct bucket *b,
	struct memory_block *m)
{
	heap_run_init(heap, b, m);
	memblock_rebuild_state(heap, m);
	heap_process_run_metadata(heap, b, m);
}

/*
 * heap_reuse_run -- (internal) reuses existing run
 */
static uint32_t
heap_reuse_run(struct palloc_heap *heap, struct bucket *b,
	const struct memory_block *m)
{
	ASSERTeq(m->type, MEMORY_BLOCK_RUN);

	return heap_process_run_metadata(heap, b, m);
}

/*
 * heap_reclaim_run -- checks the run for available memory if unclaimed.
 *
 * Returns 1 if reclaimed chunk, 0 otherwise.
 */
static int
heap_reclaim_run(struct palloc_heap *heap, struct bucket *defb,
	struct memory_block *m)
{
	if (m->m_ops->claim(m) != 0)
		return 0; /* this run already has an owner */

	struct chunk_run *run = (struct chunk_run *)
		&ZID_TO_ZONE(heap->layout, m->zone_id)->chunks[m->chunk_id];

	struct alloc_class_run_proto run_proto;
	alloc_class_generate_run_proto(&run_proto,
		run->block_size, m->size_idx);

	os_mutex_t *lock = m->m_ops->get_lock(m);
	util_mutex_lock(lock);

	unsigned i;
	unsigned nval = run_proto.bitmap_nval;
	for (i = 0; nval > 0 && i < nval - 1; ++i)
		if (run->bitmap[i] != 0)
			break;

	int empty = (i == (nval - 1)) &&
		(run->bitmap[i] == run_proto.bitmap_lastval);
	if (empty) {
		struct zone *z = ZID_TO_ZONE(heap->layout, m->zone_id);
		struct chunk_header *hdr = &z->chunk_headers[m->chunk_id];

		/*
		 * The redo log ptr can be NULL if we are sure that there's only
		 * one persistent value modification in the entire operation
		 * context.
		 */
		struct operation_context ctx;
		operation_init(&ctx, heap->base, NULL, NULL);
		ctx.p_ops = &heap->p_ops;

		struct memory_block nb = MEMORY_BLOCK_NONE;
		nb.chunk_id = m->chunk_id;
		nb.zone_id = m->zone_id;
		nb.block_off = 0;
		nb.size_idx = m->size_idx;

		heap_chunk_init(heap, hdr, CHUNK_TYPE_FREE, nb.size_idx);
		memblock_rebuild_state(heap, &nb);

		nb = heap_coalesce_huge(heap, defb, &nb);
		nb.m_ops->prep_hdr(&nb, MEMBLOCK_FREE, &ctx);

		operation_process(&ctx);

		bucket_insert_block(defb, &nb);

		*m = nb;
	} else {
		struct alloc_class *c = alloc_class_by_unit_size(
			heap->rt->alloc_classes,
			run->block_size);

		if (c == NULL ||
		    c->type != CLASS_RUN ||
		    c->run.size_idx != m->size_idx ||
		    c->header_type != m->header_type ||
		    recycler_put(heap->rt->recyclers[c->id], m) < 0)
			m->m_ops->claim_revoke(m);
	}

	util_mutex_unlock(lock);

	return empty;
}

/*
 * heap_init_free_chunk -- initializes free chunk transient state
 */
static void
heap_init_free_chunk(struct palloc_heap *heap,
	struct bucket *bucket,
	struct chunk_header *hdr,
	struct memory_block *m)
{
	struct operation_context ctx;
	operation_init(&ctx, heap->base, NULL, NULL);
	ctx.p_ops = &heap->p_ops;
	heap_chunk_write_footer(hdr, hdr->size_idx);
	/*
	 * Perform coalescing just in case there
	 * are any neighbouring free chunks.
	 */
	struct memory_block nm = heap_coalesce_huge(heap, bucket, m);
	if (nm.chunk_id != m->chunk_id) {
		m->m_ops->prep_hdr(&nm, MEMBLOCK_FREE, &ctx);
		operation_process(&ctx);
	}
	*m = nm;
	bucket_insert_block(bucket, m);
}

/*
 * heap_reclaim_zone_garbage -- (internal) creates volatile state of unused runs
 */
static int
heap_reclaim_zone_garbage(struct palloc_heap *heap, struct bucket *bucket,
	uint32_t zone_id, int init)
{
	struct zone *z = ZID_TO_ZONE(heap->layout, zone_id);

	int rchunks = 0;

	/*
	 * If this is the first time this zone is processed, recreate all
	 * footers BEFORE any other operation takes place. For example, the
	 * heap_init_free_chunk call expects the footers to be created.
	 */
	if (init) {
		for (uint32_t i = 0; i < z->header.size_idx; ) {
			struct chunk_header *hdr = &z->chunk_headers[i];
			switch (hdr->type) {
				case CHUNK_TYPE_FREE:
				case CHUNK_TYPE_USED:
					heap_chunk_write_footer(hdr,
						hdr->size_idx);
					break;
			}

			i += hdr->size_idx;
		}
	}

	for (uint32_t i = 0; i < z->header.size_idx; ) {
		struct chunk_header *hdr = &z->chunk_headers[i];
		ASSERT(hdr->size_idx != 0);

		struct memory_block m = MEMORY_BLOCK_NONE;
		m.zone_id = zone_id;
		m.chunk_id = i;
		m.size_idx = hdr->size_idx;

		memblock_rebuild_state(heap, &m);

		switch (hdr->type) {
			case CHUNK_TYPE_RUN:
				rchunks += heap_reclaim_run(heap, bucket, &m);
				break;
			case CHUNK_TYPE_FREE:
				if (init) {
					rchunks += (int)m.size_idx;
					heap_init_free_chunk(heap, bucket,
						hdr, &m);
				}
				break;
			case CHUNK_TYPE_USED:
				break;
			default:
				ASSERT(0);
		}

		i = m.chunk_id + m.size_idx; /* hdr might have changed */
	}

	return rchunks == 0 ? ENOMEM : 0;
}

/*
 * heap_populate_bucket -- (internal) creates volatile state of memory blocks
 */
static int
heap_populate_bucket(struct palloc_heap *heap, struct bucket *bucket)
{
	struct heap_rt *h = heap->rt;

	if (h->zones_exhausted == h->max_zone)
		return ENOMEM;

	uint32_t zone_id = h->zones_exhausted++;
	struct zone *z = ZID_TO_ZONE(heap->layout, zone_id);

	/* ignore zone and chunk headers */
	VALGRIND_ADD_TO_GLOBAL_TX_IGNORE(z, sizeof(z->header) +
		sizeof(z->chunk_headers));

	if (z->header.magic != ZONE_HEADER_MAGIC)
		heap_zone_init(heap, zone_id);

	return heap_reclaim_zone_garbage(heap, bucket, zone_id, 1 /* init */);
}

/*
 * heap_reclaim_garbage -- (internal) creates volatile state of unused runs
 */
static int
heap_reclaim_garbage(struct palloc_heap *heap, struct bucket *bucket)
{
	struct memory_block m = MEMORY_BLOCK_NONE;
	for (size_t i = 0; i < MAX_ALLOCATION_CLASSES; ++i) {
		while (recycler_get(heap->rt->recyclers[i], &m) == 0) {
			m.m_ops->claim_revoke(&m);
			m.size_idx = 0;
		}
	}

	int ret = ENOMEM;
	for (unsigned i = 0; i < heap->rt->zones_exhausted; ++i) {
		if (heap_reclaim_zone_garbage(heap,
			bucket, i, 0 /* not init */) == 0)
			ret = 0;
	}

	return ret;
}

/*
 * heap_ensure_huge_bucket_filled --
 *	(internal) refills the default bucket if needed
 */
static int
heap_ensure_huge_bucket_filled(struct palloc_heap *heap, struct bucket *bucket)
{
	return (heap_reclaim_garbage(heap, bucket) == 0 ||
		heap_populate_bucket(heap, bucket) == 0) ? 0 : ENOMEM;
}

/*
 * heap_reuse_from_recycler -- (internal) try reusing runs that are currently
 *	in the recycler
 */
static int
heap_reuse_from_recycler(struct palloc_heap *heap,
	struct bucket *b, uint32_t units)
{
	struct memory_block m = MEMORY_BLOCK_NONE;
	m.size_idx = units;

	if (recycler_get(heap->rt->recyclers[b->aclass->id], &m) == 0) {
		os_mutex_t *lock = m.m_ops->get_lock(&m);

		util_mutex_lock(lock);
		heap_reuse_run(heap, b, &m);
		util_mutex_unlock(lock);

		b->active_memory_block = m;
		b->is_active = 1;

		return 0;
	}

	return ENOMEM;
}

/*
 * heap_ensure_run_bucket_filled -- (internal) refills the bucket if needed
 */
static int
heap_ensure_run_bucket_filled(struct palloc_heap *heap, struct bucket *b,
	uint32_t units)
{
	ASSERTeq(b->aclass->type, CLASS_RUN);

	int ret = 0;

	struct bucket *defb = heap_bucket_acquire_by_id(heap,
			DEFAULT_ALLOC_CLASS_ID);

	/* get rid of the active block in the bucket */
	if (b->is_active) {
		b->c_ops->rm_all(b->container);
		b->is_active = 0;

		b->active_memory_block.m_ops
			->claim_revoke(&b->active_memory_block);

		/* either convert to a full chunk or place it in the recycler */
		heap_reclaim_run(heap, defb, &b->active_memory_block);
	}

	if (heap_reuse_from_recycler(heap, b, units) == 0)
		goto out;

	struct memory_block m = MEMORY_BLOCK_NONE;
	m.size_idx = b->aclass->run.size_idx;

	/* cannot reuse an existing run, create a new one */
	if (heap_get_bestfit_block(heap, defb, &m) == 0) {
		ASSERTeq(m.block_off, 0);

		heap_create_run(heap, b, &m);

		b->active_memory_block = m;
		b->is_active = 1;

		goto out;
	}

	/*
	 * Try the recycler again, the previous call to the bestfit_block for
	 * huge chunks might have reclaimed some unused runs.
	 */
	if (heap_reuse_from_recycler(heap, b, units) == 0)
		goto out;

	ret = ENOMEM;

out:
	heap_bucket_release(heap, defb);

	return ret;
}

/*
 * heap_resize_chunk -- (internal) splits the chunk into two smaller ones
 */
static void
heap_resize_chunk(struct palloc_heap *heap, struct bucket *bucket,
	uint32_t chunk_id, uint32_t zone_id, uint32_t new_size_idx)
{
	uint32_t new_chunk_id = chunk_id + new_size_idx;

	struct zone *z = ZID_TO_ZONE(heap->layout, zone_id);
	struct chunk_header *old_hdr = &z->chunk_headers[chunk_id];
	struct chunk_header *new_hdr = &z->chunk_headers[new_chunk_id];

	uint32_t rem_size_idx = old_hdr->size_idx - new_size_idx;
	heap_chunk_init(heap, new_hdr, CHUNK_TYPE_FREE, rem_size_idx);
	heap_chunk_init(heap, old_hdr, CHUNK_TYPE_FREE, new_size_idx);

	struct memory_block m = {new_chunk_id, zone_id, rem_size_idx, 0,
		0, 0, NULL, NULL};
	memblock_rebuild_state(heap, &m);
	bucket_insert_block(bucket, &m);
}

/*
 * heap_recycle_block -- (internal) recycles unused part of the memory block
 */
static void
heap_recycle_block(struct palloc_heap *heap, struct bucket *b,
		struct memory_block *m, uint32_t units)
{
	if (b->aclass->type == CLASS_RUN) {
		ASSERT(units <= UINT16_MAX);
		ASSERT(m->block_off + units <= UINT16_MAX);
		struct memory_block r = {m->chunk_id, m->zone_id,
			m->size_idx - units, (uint16_t)(m->block_off + units),
			0, 0, NULL, NULL};
		memblock_rebuild_state(heap, &r);
		bucket_insert_block(b, &r);
	} else {
		heap_resize_chunk(heap, b, m->chunk_id, m->zone_id, units);
	}

	m->size_idx = units;
}

/*
 * heap_get_bestfit_block --
 *	extracts a memory block of equal size index
 */
int
heap_get_bestfit_block(struct palloc_heap *heap, struct bucket *b,
	struct memory_block *m)
{
	uint32_t units = m->size_idx;

	while (b->c_ops->get_rm_bestfit(b->container, m) != 0) {
		if (b->aclass->type == CLASS_HUGE) {
			if (heap_ensure_huge_bucket_filled(heap, b) != 0)
				return ENOMEM;
		} else {
			if (heap_ensure_run_bucket_filled(heap, b, units) != 0)
				return ENOMEM;
		}
	}

	ASSERT(m->size_idx >= units);

	if (units != m->size_idx)
		heap_recycle_block(heap, b, m, units);

	return 0;
}

/*
 * heap_get_adjacent_free_block -- locates adjacent free memory block in heap
 */
static int
heap_get_adjacent_free_block(struct palloc_heap *heap,
	const struct memory_block *in, struct memory_block *out, int prev)
{
	struct zone *z = ZID_TO_ZONE(heap->layout, in->zone_id);
	struct chunk_header *hdr = &z->chunk_headers[in->chunk_id];
	out->zone_id = in->zone_id;

	if (prev) {
		if (in->chunk_id == 0)
			return ENOENT;

		struct chunk_header *prev_hdr =
			&z->chunk_headers[in->chunk_id - 1];
		out->chunk_id = in->chunk_id - prev_hdr->size_idx;

		if (z->chunk_headers[out->chunk_id].type != CHUNK_TYPE_FREE)
			return ENOENT;

		out->size_idx = z->chunk_headers[out->chunk_id].size_idx;
	} else { /* next */
		if (in->chunk_id + hdr->size_idx == z->header.size_idx)
			return ENOENT;

		out->chunk_id = in->chunk_id + hdr->size_idx;

		if (z->chunk_headers[out->chunk_id].type != CHUNK_TYPE_FREE)
			return ENOENT;

		out->size_idx = z->chunk_headers[out->chunk_id].size_idx;
	}
	memblock_rebuild_state(heap, out);

	return 0;
}

/*
 * heap_coalesce -- (internal) merges adjacent memory blocks
 */
static struct memory_block
heap_coalesce(struct palloc_heap *heap,
	const struct memory_block *blocks[], int n)
{
	struct memory_block ret;
	const struct memory_block *b = NULL;
	ret.size_idx = 0;
	for (int i = 0; i < n; ++i) {
		if (blocks[i] == NULL)
			continue;
		b = b ? b : blocks[i];
		ret.size_idx += blocks[i] ? blocks[i]->size_idx : 0;
	}

	ASSERTne(b, NULL);

	ret.chunk_id = b->chunk_id;
	ret.zone_id = b->zone_id;
	ret.block_off = b->block_off;
	memblock_rebuild_state(heap, &ret);

	return ret;
}

/*
 * heap_coalesce_huge -- finds neighbours of a huge block, removes them from the
 *	volatile state and returns the resulting block
 */
struct memory_block
heap_coalesce_huge(struct palloc_heap *heap, struct bucket *b,
	const struct memory_block *m)
{
	const struct memory_block *blocks[3] = {NULL, m, NULL};

	struct memory_block prev = MEMORY_BLOCK_NONE;
	if (heap_get_adjacent_free_block(heap, m, &prev, 1) == 0 &&
		b->c_ops->get_rm_exact(b->container, &prev) == 0) {
		blocks[0] = &prev;
	}

	struct memory_block next = MEMORY_BLOCK_NONE;
	if (heap_get_adjacent_free_block(heap, m, &next, 0) == 0 &&
		b->c_ops->get_rm_exact(b->container, &next) == 0) {
		blocks[2] = &next;
	}

	return heap_coalesce(heap, blocks, 3);
}

/*
 * heap_end -- returns first address after heap
 */
void *
heap_end(struct palloc_heap *h)
{
	ASSERT(h->rt->max_zone > 0);

	struct zone *last_zone = ZID_TO_ZONE(h->layout, h->rt->max_zone - 1);

	return &last_zone->chunks[last_zone->header.size_idx];
}

/*
 * heap_get_narenas -- (internal) returns the number of arenas to create
 */
static unsigned
heap_get_narenas(void)
{
	long cpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (cpus < 1)
		cpus = 1;

	unsigned arenas = (unsigned)cpus;

	LOG(4, "creating %u arenas", arenas);

	return arenas;
}

/*
 * heap_create_alloc_class_buckets -- allocates all cache bucket
 * instances of the specified type
 */
int
heap_create_alloc_class_buckets(struct palloc_heap *heap, struct alloc_class *c)
{
	struct heap_rt *h = heap->rt;
	int i;
	for (i = 0; i < (int)h->narenas; ++i) {
		h->arenas[i].buckets[c->id] = bucket_new(
			container_new_seglists(heap), c);
		if (h->arenas[i].buckets[c->id] == NULL)
			goto error_cache_bucket_new;
	}

	return 0;

error_cache_bucket_new:
	for (i -= 1; i >= 0; --i) {
		bucket_delete(h->arenas[i].buckets[c->id]);
	}

	return -1;
}

/*
 * heap_buckets_init -- (internal) initializes bucket instances
 */
int
heap_buckets_init(struct palloc_heap *heap)
{
	struct heap_rt *h = heap->rt;

	for (uint8_t i = 0; i < MAX_ALLOCATION_CLASSES; ++i) {
		struct alloc_class *c = alloc_class_by_id(h->alloc_classes, i);
		if (c != NULL) {
			if (heap_create_alloc_class_buckets(heap, c) != 0)
				goto error_bucket_create;
		}
	}

	h->default_bucket = bucket_new(container_new_ctree(heap),
		alloc_class_by_id(h->alloc_classes, DEFAULT_ALLOC_CLASS_ID));

	if (h->default_bucket == NULL)
		goto error_bucket_create;

	return 0;

error_bucket_create:
	for (unsigned i = 0; i < h->narenas; ++i)
		heap_arena_destroy(&h->arenas[i]);

	return -1;
}

/*
 * heap_boot -- opens the heap region of the pmemobj pool
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
heap_boot(struct palloc_heap *heap, void *heap_start, uint64_t heap_size,
		uint64_t run_id, void *base, struct pmem_ops *p_ops)
{
	struct heap_rt *h = Malloc(sizeof(*h));
	int err;
	if (h == NULL) {
		err = ENOMEM;
		goto error_heap_malloc;
	}

	h->alloc_classes = alloc_class_collection_new();
	if (h->alloc_classes == NULL) {
		err = ENOMEM;
		goto error_alloc_classes_new;
	}

	h->narenas = heap_get_narenas();
	h->arenas = Malloc(sizeof(struct arena) * h->narenas);
	if (h->arenas == NULL) {
		err = ENOMEM;
		goto error_arenas_malloc;
	}

	h->max_zone = heap_max_zone(heap_size);
	h->zones_exhausted = 0;

	for (int i = 0; i < MAX_RUN_LOCKS; ++i)
		util_mutex_init(&h->run_locks[i]);

	util_mutex_init(&h->arenas_lock);

	os_tls_key_create(&h->thread_arena, heap_thread_arena_destructor);

	heap->run_id = run_id;
	heap->p_ops = *p_ops;
	heap->layout = heap_start;
	heap->rt = h;
	heap->size = heap_size;
	heap->base = base;
	VALGRIND_DO_CREATE_MEMPOOL(heap->layout, 0, 0);

	for (unsigned i = 0; i < h->narenas; ++i)
		heap_arena_init(&h->arenas[i]);

	size_t rec_i;
	for (rec_i = 0; rec_i < MAX_ALLOCATION_CLASSES; ++rec_i) {
		if ((h->recyclers[rec_i] = recycler_new(heap)) == NULL) {
			err = ENOMEM;
			goto error_recycler_new;
		}
	}

	return 0;

error_recycler_new:
	Free(h->arenas);
	for (size_t i = 0; i < rec_i; ++i)
		recycler_delete(h->recyclers[i]);
error_arenas_malloc:
	alloc_class_collection_delete(h->alloc_classes);
error_alloc_classes_new:
	Free(h);
	heap->rt = NULL;
error_heap_malloc:
	return err;
}

/*
 * heap_write_header -- (internal) creates a clean header
 */
static void
heap_write_header(struct heap_header *hdr, size_t size)
{
	struct heap_header newhdr = {
		.signature = HEAP_SIGNATURE,
		.major = HEAP_MAJOR,
		.minor = HEAP_MINOR,
		.size = size,
		.chunksize = CHUNKSIZE,
		.chunks_per_zone = MAX_CHUNK,
		.reserved = {0},
		.checksum = 0
	};

	util_checksum(&newhdr, sizeof(newhdr), &newhdr.checksum, 1);
	*hdr = newhdr;
}

/*
 * heap_init -- initializes the heap
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
heap_init(void *heap_start, uint64_t heap_size, struct pmem_ops *p_ops)
{
	if (heap_size < HEAP_MIN_SIZE)
		return EINVAL;

	VALGRIND_DO_MAKE_MEM_UNDEFINED(heap_start, heap_size);

	struct heap_layout *layout = heap_start;
	heap_write_header(&layout->header, heap_size);
	pmemops_persist(p_ops, &layout->header, sizeof(struct heap_header));

	unsigned zones = heap_max_zone(heap_size);
	for (unsigned i = 0; i < zones; ++i) {
		pmemops_memset_persist(p_ops,
				&ZID_TO_ZONE(layout, i)->header,
				0, sizeof(struct zone_header));
		pmemops_memset_persist(p_ops,
				&ZID_TO_ZONE(layout, i)->chunk_headers,
				0, sizeof(struct chunk_header));

		/* only explicitly allocated chunks should be accessible */
		VALGRIND_DO_MAKE_MEM_NOACCESS(
			&ZID_TO_ZONE(layout, i)->chunk_headers,
			sizeof(struct chunk_header));
	}

	return 0;
}

/*
 * heap_cleanup -- cleanups the volatile heap state
 */
void
heap_cleanup(struct palloc_heap *heap)
{
	struct heap_rt *rt = heap->rt;

	alloc_class_collection_delete(rt->alloc_classes);

	bucket_delete(rt->default_bucket);

	for (unsigned i = 0; i < rt->narenas; ++i)
		heap_arena_destroy(&rt->arenas[i]);

	for (int i = 0; i < MAX_RUN_LOCKS; ++i)
		util_mutex_destroy(&rt->run_locks[i]);

	util_mutex_destroy(&rt->arenas_lock);

	os_tls_key_delete(rt->thread_arena);

	Free(rt->arenas);

	for (int i = 0; i < MAX_ALLOCATION_CLASSES; ++i) {
		recycler_delete(rt->recyclers[i]);
	}

	VALGRIND_DO_DESTROY_MEMPOOL(heap->layout);

	Free(rt);
	heap->rt = NULL;
}

/*
 * heap_verify_header -- (internal) verifies if the heap header is consistent
 */
static int
heap_verify_header(struct heap_header *hdr)
{
	if (util_checksum(hdr, sizeof(*hdr), &hdr->checksum, 0) != 1) {
		ERR("heap: invalid header's checksum");
		return -1;
	}

	if (memcmp(hdr->signature, HEAP_SIGNATURE, HEAP_SIGNATURE_LEN) != 0) {
		ERR("heap: invalid signature");
		return -1;
	}

	return 0;
}

/*
 * heap_verify_zone_header --
 *	(internal) verifies if the zone header is consistent
 */
static int
heap_verify_zone_header(struct zone_header *hdr)
{
	if (hdr->size_idx == 0) {
		ERR("heap: invalid zone size");
		return -1;
	}

	return 0;
}

/*
 * heap_verify_chunk_header --
 *	(internal) verifies if the chunk header is consistent
 */
static int
heap_verify_chunk_header(struct chunk_header *hdr)
{
	if (hdr->type == CHUNK_TYPE_UNKNOWN) {
		ERR("heap: invalid chunk type");
		return -1;
	}

	if (hdr->type >= MAX_CHUNK_TYPE) {
		ERR("heap: unknown chunk type");
		return -1;
	}

	if (hdr->flags & ~CHUNK_FLAGS_ALL_VALID) {
		ERR("heap: invalid chunk flags");
		return -1;
	}

	return 0;
}

/*
 * heap_verify_zone -- (internal) verifies if the zone is consistent
 */
static int
heap_verify_zone(struct zone *zone)
{
	if (zone->header.magic == 0)
		return 0; /* not initialized, and that is OK */

	if (zone->header.magic != ZONE_HEADER_MAGIC) {
		ERR("heap: invalid zone magic");
		return -1;
	}

	if (heap_verify_zone_header(&zone->header))
		return -1;

	uint32_t i;
	for (i = 0; i < zone->header.size_idx; ) {
		if (heap_verify_chunk_header(&zone->chunk_headers[i]))
			return -1;

		i += zone->chunk_headers[i].size_idx;
	}

	if (i != zone->header.size_idx) {
		ERR("heap: chunk sizes mismatch");
		return -1;
	}

	return 0;
}

/*
 * heap_check -- verifies if the heap is consistent and can be opened properly
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
heap_check(void *heap_start, uint64_t heap_size)
{
	if (heap_size < HEAP_MIN_SIZE) {
		ERR("heap: invalid heap size");
		return -1;
	}

	struct heap_layout *layout = heap_start;

	if (heap_size != layout->header.size) {
		ERR("heap: heap size missmatch");
		return -1;
	}

	if (heap_verify_header(&layout->header))
		return -1;

	for (unsigned i = 0; i < heap_max_zone(layout->header.size); ++i) {
		if (heap_verify_zone(ZID_TO_ZONE(layout, i)))
			return -1;
	}

	return 0;
}

/*
 * heap_check_remote -- verifies if the heap of a remote pool is consistent
 *                      and can be opened properly
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
heap_check_remote(void *heap_start, uint64_t heap_size, struct remote_ops *ops)
{
	if (heap_size < HEAP_MIN_SIZE) {
		ERR("heap: invalid heap size");
		return -1;
	}

	struct heap_layout *layout = heap_start;

	struct heap_header header;
	if (ops->read(ops->ctx, ops->base, &header, &layout->header,
						sizeof(struct heap_header))) {
		ERR("heap: obj_read_remote error");
		return -1;
	}

	if (heap_size != header.size) {
		ERR("heap: heap size mismatch");
		return -1;
	}

	if (heap_verify_header(&header))
		return -1;

	struct zone *zone_buff = (struct zone *)Malloc(sizeof(struct zone));
	if (zone_buff == NULL) {
		ERR("heap: zone_buff malloc error");
		return -1;
	}
	for (unsigned i = 0; i < heap_max_zone(header.size); ++i) {
		if (ops->read(ops->ctx, ops->base, zone_buff,
				ZID_TO_ZONE(layout, i), sizeof(struct zone))) {
			ERR("heap: obj_read_remote error");
			goto out;
		}

		if (heap_verify_zone(zone_buff)) {
			goto out;
		}
	}
	Free(zone_buff);
	return 0;

out:
	Free(zone_buff);
	return -1;
}

/*
 * heap_run_foreach_object -- (internal) iterates through objects in a run
 */
int
heap_run_foreach_object(struct palloc_heap *heap, object_callback cb,
		void *arg, struct memory_block *m)
{
	uint16_t i = m->block_off / BITS_PER_VALUE;
	uint16_t block_start = m->block_off % BITS_PER_VALUE;
	uint16_t block_off;

	struct chunk_run *run = (struct chunk_run *)
		&ZID_TO_ZONE(heap->layout, m->zone_id)->chunks[m->chunk_id];

	struct alloc_class_run_proto run_proto;
	alloc_class_generate_run_proto(&run_proto,
		run->block_size, m->size_idx);

	for (; i < run_proto.bitmap_nval; ++i) {
		uint64_t v = run->bitmap[i];
		block_off = (uint16_t)(BITS_PER_VALUE * i);

		for (uint16_t j = block_start; j < BITS_PER_VALUE; ) {
			if (block_off + j >= (uint16_t)run_proto.bitmap_nallocs)
				break;

			if (!BIT_IS_CLR(v, j)) {
				m->block_off = (uint16_t)(block_off + j);

				/*
				 * The size index of this memory block cannot be
				 * retrieved at this time because the header
				 * might not be initialized in valgrind yet.
				 */
				m->size_idx = 0;

				if (cb(m, arg)
						!= 0)
					return 1;

				m->size_idx = CALC_SIZE_IDX(run->block_size,
					m->m_ops->get_real_size(m));
				j = (uint16_t)(j + m->size_idx);
			} else {
				++j;
			}
		}
		block_start = 0;
	}

	return 0;
}

/*
 * heap_chunk_foreach_object -- (internal) iterates through objects in a chunk
 */
static int
heap_chunk_foreach_object(struct palloc_heap *heap, object_callback cb,
	void *arg, struct memory_block *m)
{
	struct zone *zone = ZID_TO_ZONE(heap->layout, m->zone_id);
	struct chunk_header *hdr = &zone->chunk_headers[m->chunk_id];
	memblock_rebuild_state(heap, m);
	m->size_idx = hdr->size_idx;

	switch (hdr->type) {
		case CHUNK_TYPE_FREE:
			return 0;
		case CHUNK_TYPE_USED:
			return cb(m, arg);
		case CHUNK_TYPE_RUN:
			return heap_run_foreach_object(heap, cb, arg, m);
		default:
			ASSERT(0);
	}

	return 0;
}

/*
 * heap_zone_foreach_object -- (internal) iterates through objects in a zone
 */
static int
heap_zone_foreach_object(struct palloc_heap *heap, object_callback cb,
	void *arg, struct memory_block *m)
{
	struct zone *zone = ZID_TO_ZONE(heap->layout, m->zone_id);
	if (zone->header.magic == 0)
		return 0;

	for (; m->chunk_id < zone->header.size_idx; ) {
		if (heap_chunk_foreach_object(heap, cb, arg, m) != 0)
			return 1;

		m->chunk_id += zone->chunk_headers[m->chunk_id].size_idx;

		/* reset the starting position of memblock */
		m->block_off = 0;
		m->size_idx = 0;
	}

	return 0;
}

/*
 * heap_foreach_object -- (internal) iterates through objects in the heap
 */
void
heap_foreach_object(struct palloc_heap *heap, object_callback cb, void *arg,
	struct memory_block m)
{
	struct heap_layout *layout = heap->layout;

	for (; m.zone_id < heap_max_zone(layout->header.size); ++m.zone_id) {
		if (heap_zone_foreach_object(heap, cb, arg, &m) != 0)
			break;

		m.chunk_id = 0;
	}
}

#ifdef USE_VG_MEMCHECK

/*
 * heap_vg_open_chunk -- (internal) notifies Valgrind about chunk layout
 */
static void
heap_vg_open_chunk(struct palloc_heap *heap,
	object_callback cb, void *arg, int objects,
	struct memory_block *m)
{
	struct zone *z = ZID_TO_ZONE(heap->layout, m->zone_id);
	void *chunk = &z->chunks[m->chunk_id];
	memblock_rebuild_state(heap, m);

	if (m->type == MEMORY_BLOCK_RUN) {
		struct chunk_run *run = chunk;

		ASSERTne(m->size_idx, 0);
		VALGRIND_DO_MAKE_MEM_NOACCESS(run,
			SIZEOF_RUN(run, m->size_idx));

		/* set the run metadata as defined */
		VALGRIND_DO_MAKE_MEM_DEFINED(run,
			sizeof(*run) - sizeof(run->data));

		if (objects) {
			int ret = heap_run_foreach_object(heap, cb, arg, m);
			ASSERTeq(ret, 0);
		}
	} else {
		size_t size = m->m_ops->get_real_size(m);
		VALGRIND_DO_MAKE_MEM_NOACCESS(chunk, size);

		if (objects && m->m_ops->get_state(m) == MEMBLOCK_ALLOCATED) {
			int ret = cb(m, arg);
			ASSERTeq(ret, 0);
		}
	}
}

/*
 * heap_vg_open -- notifies Valgrind about heap layout
 */
void
heap_vg_open(struct palloc_heap *heap, object_callback cb,
	void *arg, int objects)
{
	ASSERTne(cb, NULL);
	VALGRIND_DO_MAKE_MEM_UNDEFINED(heap->layout, heap->size);

	struct heap_layout *layout = heap->layout;

	VALGRIND_DO_MAKE_MEM_DEFINED(&layout->header, sizeof(layout->header));

	unsigned zones = heap_max_zone(heap->size);

	struct memory_block m = MEMORY_BLOCK_NONE;
	for (unsigned i = 0; i < zones; ++i) {
		struct zone *z = ZID_TO_ZONE(layout, i);
		uint32_t chunks;
		m.zone_id = i;
		m.chunk_id = 0;

		VALGRIND_DO_MAKE_MEM_DEFINED(&z->header, sizeof(z->header));

		if (z->header.magic != ZONE_HEADER_MAGIC)
			continue;

		chunks = z->header.size_idx;

		for (uint32_t c = 0; c < chunks; ) {
			struct chunk_header *hdr = &z->chunk_headers[c];
			m.chunk_id = c;

			VALGRIND_DO_MAKE_MEM_DEFINED(hdr, sizeof(*hdr));

			m.size_idx = hdr->size_idx;
			heap_vg_open_chunk(heap, cb, arg, objects, &m);
			m.block_off = 0;

			ASSERT(hdr->size_idx > 0);

			if (hdr->type == CHUNK_TYPE_RUN) {
				/*
				 * Mark run data headers as defined.
				 */
				for (unsigned j = 1; j < hdr->size_idx; ++j) {
					struct chunk_header *data_hdr =
						&z->chunk_headers[c + j];
					VALGRIND_DO_MAKE_MEM_DEFINED(data_hdr,
						sizeof(struct chunk_header));
					ASSERTeq(data_hdr->type,
						CHUNK_TYPE_RUN_DATA);
				}
			} else {
				/*
				 * Mark unused chunk headers as not accessible.
				 */
				VALGRIND_DO_MAKE_MEM_NOACCESS(
					&z->chunk_headers[c + 1],
					(hdr->size_idx - 1) *
					sizeof(struct chunk_header));
			}

			c += hdr->size_idx;
		}

		/* mark all unused chunk headers after last as not accessible */
		VALGRIND_DO_MAKE_MEM_NOACCESS(&z->chunk_headers[chunks],
			(MAX_CHUNK - chunks) * sizeof(struct chunk_header));
	}
}
#endif
