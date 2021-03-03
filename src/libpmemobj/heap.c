/*
 * Copyright 2015-2021, Intel Corporation
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
#include "container_ravl.h"
#include "container_seglists.h"
#include "alloc_class.h"
#include "os_thread.h"
#include "set.h"

#define MAX_RUN_LOCKS MAX_CHUNK
#define MAX_RUN_LOCKS_VG 1024 /* avoid perf issues /w drd */

/*
 * This is the value by which the heap might grow once we hit an OOM.
 */
#define HEAP_DEFAULT_GROW_SIZE (1 << 27) /* 128 megabytes */
#define MAX_DEFAULT_ARENAS (1 << 10) /* 1024 arenas */

struct arenas {
	VEC(, struct arena *) vec;
	size_t nactive;

	/*
	 * When nesting with other locks, this one must be acquired first,
	 * prior to locking any buckets or memory blocks.
	 */
	os_mutex_t lock;

	/* stores a pointer to one of the arenas */
	os_tls_key_t thread;
};

/*
 * Arenas store the collection of buckets for allocation classes.
 * Each thread is assigned an arena on its first allocator operation
 * if arena is set to auto.
 */
struct arena {
	/* one bucket per allocation class */
	struct bucket *buckets[MAX_ALLOCATION_CLASSES];

	/*
	 * Decides whether the arena can be
	 * automatically assigned to a thread.
	 */
	int automatic;
	size_t nthreads;
	struct arenas *arenas;
};

struct heap_rt {
	struct alloc_class_collection *alloc_classes;

	/* DON'T use these two variable directly! */
	struct bucket *default_bucket;

	struct arenas arenas;

	struct recycler *recyclers[MAX_ALLOCATION_CLASSES];

	os_mutex_t run_locks[MAX_RUN_LOCKS];
	unsigned nlocks;

	unsigned nzones;
	unsigned zones_exhausted;
};

/*
 * heap_arenas_init - (internal) initialize generic arenas info
 */
static int
heap_arenas_init(struct arenas *arenas)
{
	util_mutex_init(&arenas->lock);
	VEC_INIT(&arenas->vec);
	arenas->nactive = 0;

	if (VEC_RESERVE(&arenas->vec, MAX_DEFAULT_ARENAS) == -1)
		return -1;
	return 0;
}

/*
 * heap_arenas_fini - (internal) destroy generic arenas info
 */
static void
heap_arenas_fini(struct arenas *arenas)
{
	util_mutex_destroy(&arenas->lock);
	VEC_DELETE(&arenas->vec);
}

/*
 * heap_alloc_classes -- returns the allocation classes collection
 */
struct alloc_class_collection *
heap_alloc_classes(struct palloc_heap *heap)
{
	return heap->rt->alloc_classes;
}

/*
 * heap_arena_delete -- (internal) destroys arena instance
 */
static void
heap_arena_delete(struct arena *arena)
{
	for (int i = 0; i < MAX_ALLOCATION_CLASSES; ++i)
		if (arena->buckets[i] != NULL)
			bucket_delete(arena->buckets[i]);
	Free(arena);
}

/*
 * heap_arena_new -- (internal) initializes arena instance
 */
static struct arena *
heap_arena_new(struct palloc_heap *heap, int automatic)
{
	struct heap_rt *rt = heap->rt;

	struct arena *arena = Zalloc(sizeof(struct arena));
	if (arena == NULL) {
		ERR("!heap: arena malloc error");
		return NULL;
	}
	arena->nthreads = 0;
	arena->automatic = automatic;
	arena->arenas = &heap->rt->arenas;

	COMPILE_ERROR_ON(MAX_ALLOCATION_CLASSES > UINT8_MAX);
	for (uint8_t i = 0; i < MAX_ALLOCATION_CLASSES; ++i) {
		struct alloc_class *ac =
			alloc_class_by_id(rt->alloc_classes, i);
		if (ac != NULL) {
			arena->buckets[i] =
				bucket_new(container_new_seglists(heap), ac);
			if (arena->buckets[i] == NULL)
				goto error_bucket_create;
		} else {
			arena->buckets[i] = NULL;
		}
	}

	return arena;

error_bucket_create:
	heap_arena_delete(arena);
	return NULL;
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
 * heap_arena_thread_detach -- detaches arena from the current thread
 *
 * Must be called with arenas lock taken.
 */
static void
heap_arena_thread_detach(struct arena *a)
{
	/*
	 * Even though this is under a lock, nactive variable can also be read
	 * concurrently from the recycler (without the arenas lock).
	 * That's why we are using an atomic operation.
	 */
	if ((--a->nthreads) == 0)
		util_fetch_and_sub64(&a->arenas->nactive, 1);
}

/*
 * heap_arena_thread_attach -- assign arena to the current thread
 *
 * Must be called with arenas lock taken.
 */
static void
heap_arena_thread_attach(struct palloc_heap *heap, struct arena *a)
{
	struct heap_rt *h = heap->rt;

	struct arena *thread_arena = os_tls_get(h->arenas.thread);
	if (thread_arena)
		heap_arena_thread_detach(thread_arena);

	ASSERTne(a, NULL);

	/*
	 * Even though this is under a lock, nactive variable can also be read
	 * concurrently from the recycler (without the arenas lock).
	 * That's why we are using an atomic operation.
	 */
	if ((a->nthreads++) == 0)
		util_fetch_and_add64(&a->arenas->nactive, 1);

	os_tls_set(h->arenas.thread, a);
}

/*
 * heap_thread_arena_destructor -- (internal) removes arena thread assignment
 */
static void
heap_thread_arena_destructor(void *arg)
{
	struct arena *a = arg;
	os_mutex_lock(&a->arenas->lock);
	heap_arena_thread_detach(a);
	os_mutex_unlock(&a->arenas->lock);
}

/*
 * heap_get_arena_by_id -- returns arena by id
 *
 * Must be called with arenas lock taken.
 */
static struct arena *
heap_get_arena_by_id(struct palloc_heap *heap, unsigned arena_id)
{
	return VEC_ARR(&heap->rt->arenas.vec)[arena_id - 1];
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
heap_thread_arena_assign(struct palloc_heap *heap)
{
	util_mutex_lock(&heap->rt->arenas.lock);

	struct arena *least_used = NULL;

	ASSERTne(VEC_SIZE(&heap->rt->arenas.vec), 0);

	struct arena *a;
	VEC_FOREACH(a, &heap->rt->arenas.vec) {
		if (!a->automatic)
			continue;
		if (least_used == NULL ||
			a->nthreads < least_used->nthreads)
			least_used = a;
	}

	LOG(4, "assigning %p arena to current thread", least_used);

	/* at least one automatic arena must exist */
	ASSERTne(least_used, NULL);
	heap_arena_thread_attach(heap, least_used);

	util_mutex_unlock(&heap->rt->arenas.lock);

	return least_used;
}

/*
 * heap_thread_arena -- (internal) returns the arena assigned to the current
 *	thread
 */
static struct arena *
heap_thread_arena(struct palloc_heap *heap)
{
	struct arena *a;
	if ((a = os_tls_get(heap->rt->arenas.thread)) == NULL)
		a = heap_thread_arena_assign(heap);

	return a;
}

/*
 * heap_get_thread_arena_id -- returns the arena id assigned to the current
 *	thread
 */
unsigned
heap_get_thread_arena_id(struct palloc_heap *heap)
{
	unsigned arena_id = 1;
	struct arena *arenap = heap_thread_arena(heap);
	struct arena *arenav;
	struct heap_rt *rt = heap->rt;

	util_mutex_lock(&rt->arenas.lock);
	VEC_FOREACH(arenav, &heap->rt->arenas.vec) {
		if (arenav == arenap) {
			util_mutex_unlock(&rt->arenas.lock);
			return arena_id;
		}
		arena_id++;
	}

	util_mutex_unlock(&rt->arenas.lock);
	ASSERT(0);
	return arena_id;
}

/*
 * heap_bucket_acquire -- fetches by arena or by id a bucket exclusive
 * for the thread until heap_bucket_release is called
 */
struct bucket *
heap_bucket_acquire(struct palloc_heap *heap, uint8_t class_id,
		uint16_t arena_id)
{
	struct heap_rt *rt = heap->rt;
	struct bucket *b;

	if (class_id == DEFAULT_ALLOC_CLASS_ID) {
		b = rt->default_bucket;
		goto out;
	}

	if (arena_id == HEAP_ARENA_PER_THREAD) {
		struct arena *arena = heap_thread_arena(heap);
		ASSERTne(arena->buckets, NULL);
		b = arena->buckets[class_id];
	} else {
		b = (VEC_ARR(&heap->rt->arenas.vec)
			[arena_id - 1])->buckets[class_id];
	}

out:
	util_mutex_lock(&b->lock);

	return b;
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
	return &heap->rt->run_locks[chunk_id % heap->rt->nlocks];
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
 * zone_calc_size_idx -- (internal) calculates zone size index
 */
static uint32_t
zone_calc_size_idx(uint32_t zone_id, unsigned max_zone, size_t heap_size)
{
	ASSERT(max_zone > 0);
	if (zone_id < max_zone - 1)
		return MAX_CHUNK;

	ASSERT(heap_size >= zone_id * ZONE_MAX_SIZE);
	size_t zone_raw_size = heap_size - zone_id * ZONE_MAX_SIZE;

	ASSERT(zone_raw_size >= (sizeof(struct zone_header) +
			sizeof(struct chunk_header) * MAX_CHUNK) +
			sizeof(struct heap_header));
	zone_raw_size -= sizeof(struct zone_header) +
		sizeof(struct chunk_header) * MAX_CHUNK +
		sizeof(struct heap_header);

	size_t zone_size_idx = zone_raw_size / CHUNKSIZE;
	ASSERT(zone_size_idx <= UINT32_MAX);

	return (uint32_t)zone_size_idx;
}

/*
 * heap_zone_init -- (internal) writes zone's first chunk and header
 */
static void
heap_zone_init(struct palloc_heap *heap, uint32_t zone_id,
	uint32_t first_chunk_id)
{
	struct zone *z = ZID_TO_ZONE(heap->layout, zone_id);
	uint32_t size_idx = zone_calc_size_idx(zone_id, heap->rt->nzones,
			*heap->sizep);

	ASSERT(size_idx > first_chunk_id);
	memblock_huge_init(heap, first_chunk_id, zone_id,
		size_idx - first_chunk_id);

	struct zone_header nhdr = {
		.size_idx = size_idx,
		.magic = ZONE_HEADER_MAGIC,
	};
	z->header = nhdr; /* write the entire header (8 bytes) at once */
	pmemops_persist(&heap->p_ops, &z->header, sizeof(z->header));
}

/*
 * heap_memblock_insert_block -- (internal) bucket insert wrapper for callbacks
 */
static int
heap_memblock_insert_block(const struct memory_block *m, void *b)
{
	return bucket_insert_block(b, m);
}

/*
 * heap_run_create -- (internal) initializes a new run on an existing free chunk
 */
static int
heap_run_create(struct palloc_heap *heap, struct bucket *b,
	struct memory_block *m)
{
	*m = memblock_run_init(heap,
		m->chunk_id, m->zone_id, m->size_idx,
		b->aclass->flags, b->aclass->unit_size,
		b->aclass->run.alignment);

	if (m->m_ops->iterate_free(m, heap_memblock_insert_block, b) != 0) {
		b->c_ops->rm_all(b->container);
		return -1;
	}

	STATS_INC(heap->stats, transient, heap_run_active,
		m->size_idx * CHUNKSIZE);

	return 0;
}

/*
 * heap_run_reuse -- (internal) reuses existing run
 */
static int
heap_run_reuse(struct palloc_heap *heap, struct bucket *b,
	const struct memory_block *m)
{
	int ret = 0;

	ASSERTeq(m->type, MEMORY_BLOCK_RUN);
	os_mutex_t *lock = m->m_ops->get_lock(m);

	util_mutex_lock(lock);

	ret = m->m_ops->iterate_free(m, heap_memblock_insert_block, b);

	util_mutex_unlock(lock);

	if (ret == 0) {
		b->active_memory_block->m = *m;
		b->active_memory_block->bucket = b;
		b->is_active = 1;
		util_fetch_and_add64(&b->active_memory_block->nresv, 1);
	} else {
		b->c_ops->rm_all(b->container);
	}

	return ret;
}

/*
 * heap_free_chunk_reuse -- reuses existing free chunk
 */
int
heap_free_chunk_reuse(struct palloc_heap *heap,
	struct bucket *bucket,
	struct memory_block *m)
{
	/*
	 * Perform coalescing just in case there
	 * are any neighboring free chunks.
	 */
	struct memory_block nm = heap_coalesce_huge(heap, bucket, m);
	if (nm.size_idx != m->size_idx) {
		m->m_ops->prep_hdr(&nm, MEMBLOCK_FREE, NULL);
	}

	*m = nm;

	return bucket_insert_block(bucket, m);
}

/*
 * heap_run_into_free_chunk -- (internal) creates a new free chunk in place of
 *	a run.
 */
static void
heap_run_into_free_chunk(struct palloc_heap *heap,
	struct bucket *bucket,
	struct memory_block *m)
{
	struct chunk_header *hdr = heap_get_chunk_hdr(heap, m);

	m->block_off = 0;
	m->size_idx = hdr->size_idx;

	STATS_SUB(heap->stats, transient, heap_run_active,
		m->size_idx * CHUNKSIZE);

	/*
	 * The only thing this could race with is heap_memblock_on_free()
	 * because that function is called after processing the operation,
	 * which means that a different thread might immediately call this
	 * function if the free() made the run empty.
	 * We could forgo this lock if it weren't for helgrind which needs it
	 * to establish happens-before relation for the chunk metadata.
	 */
	os_mutex_t *lock = m->m_ops->get_lock(m);
	util_mutex_lock(lock);

	*m = memblock_huge_init(heap, m->chunk_id, m->zone_id, m->size_idx);

	heap_free_chunk_reuse(heap, bucket, m);

	util_mutex_unlock(lock);
}

/*
 * heap_reclaim_run -- checks the run for available memory if unclaimed.
 *
 * Returns 1 if reclaimed chunk, 0 otherwise.
 */
static int
heap_reclaim_run(struct palloc_heap *heap, struct memory_block *m, int startup)
{
	struct chunk_run *run = heap_get_chunk_run(heap, m);
	struct chunk_header *hdr = heap_get_chunk_hdr(heap, m);

	struct alloc_class *c = alloc_class_by_run(
		heap->rt->alloc_classes,
		run->hdr.block_size, hdr->flags, m->size_idx);

	struct recycler_element e = recycler_element_new(heap, m);
	if (c == NULL) {
		uint32_t size_idx = m->size_idx;
		struct run_bitmap b;
		m->m_ops->get_bitmap(m, &b);

		ASSERTeq(size_idx, m->size_idx);

		return e.free_space == b.nbits;
	}

	if (e.free_space == c->run.nallocs)
		return 1;

	if (startup) {
		STATS_INC(heap->stats, transient, heap_run_active,
			m->size_idx * CHUNKSIZE);
		STATS_INC(heap->stats, transient, heap_run_allocated,
			(c->run.nallocs - e.free_space) * run->hdr.block_size);
	}

	if (recycler_put(heap->rt->recyclers[c->id], m, e) < 0)
		ERR("lost runtime tracking info of %u run due to OOM", c->id);

	return 0;
}

/*
 * heap_reclaim_zone_garbage -- (internal) creates volatile state of unused runs
 */
static void
heap_reclaim_zone_garbage(struct palloc_heap *heap, struct bucket *bucket,
	uint32_t zone_id)
{
	struct zone *z = ZID_TO_ZONE(heap->layout, zone_id);

	for (uint32_t i = 0; i < z->header.size_idx; ) {
		struct chunk_header *hdr = &z->chunk_headers[i];
		ASSERT(hdr->size_idx != 0);

		struct memory_block m = MEMORY_BLOCK_NONE;
		m.zone_id = zone_id;
		m.chunk_id = i;
		m.size_idx = hdr->size_idx;

		memblock_rebuild_state(heap, &m);
		m.m_ops->reinit_chunk(&m);

		switch (hdr->type) {
			case CHUNK_TYPE_RUN:
				if (heap_reclaim_run(heap, &m, 1) != 0)
					heap_run_into_free_chunk(heap, bucket,
						&m);
				break;
			case CHUNK_TYPE_FREE:
				heap_free_chunk_reuse(heap, bucket, &m);
				break;
			case CHUNK_TYPE_USED:
				break;
			default:
				ASSERT(0);
		}

		i = m.chunk_id + m.size_idx; /* hdr might have changed */
	}
}

/*
 * heap_populate_bucket -- (internal) creates volatile state of memory blocks
 */
static int
heap_populate_bucket(struct palloc_heap *heap, struct bucket *bucket)
{
	struct heap_rt *h = heap->rt;

	/* at this point we are sure that there's no more memory in the heap */
	if (h->zones_exhausted == h->nzones)
		return ENOMEM;

	uint32_t zone_id = h->zones_exhausted++;
	struct zone *z = ZID_TO_ZONE(heap->layout, zone_id);

	/* ignore zone and chunk headers */
	VALGRIND_ADD_TO_GLOBAL_TX_IGNORE(z, sizeof(z->header) +
		sizeof(z->chunk_headers));

	if (z->header.magic != ZONE_HEADER_MAGIC)
		heap_zone_init(heap, zone_id, 0);

	heap_reclaim_zone_garbage(heap, bucket, zone_id);

	/*
	 * It doesn't matter that this function might not have found any
	 * free blocks because there is still potential that subsequent calls
	 * will find something in later zones.
	 */
	return 0;
}

/*
 * heap_recycle_unused -- recalculate scores in the recycler and turn any
 *	empty runs into free chunks
 *
 * If force is not set, this function might effectively be a noop if not enough
 * of space was freed.
 */
static int
heap_recycle_unused(struct palloc_heap *heap, struct recycler *recycler,
	struct bucket *defb, int force)
{
	struct empty_runs r = recycler_recalc(recycler, force);
	if (VEC_SIZE(&r) == 0)
		return ENOMEM;

	struct bucket *nb = defb == NULL ? heap_bucket_acquire(heap,
		DEFAULT_ALLOC_CLASS_ID, HEAP_ARENA_PER_THREAD) : NULL;

	ASSERT(defb != NULL || nb != NULL);

	struct memory_block *nm;
	VEC_FOREACH_BY_PTR(nm, &r) {
		heap_run_into_free_chunk(heap, defb ? defb : nb, nm);
	}

	if (nb != NULL)
		heap_bucket_release(heap, nb);

	VEC_DELETE(&r);

	return 0;
}

/*
 * heap_reclaim_garbage -- (internal) creates volatile state of unused runs
 */
static int
heap_reclaim_garbage(struct palloc_heap *heap, struct bucket *bucket)
{
	int ret = ENOMEM;
	struct recycler *r;
	for (size_t i = 0; i < MAX_ALLOCATION_CLASSES; ++i) {
		if ((r = heap->rt->recyclers[i]) == NULL)
			continue;

		if (heap_recycle_unused(heap, r, bucket, 1) == 0)
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
	if (heap_reclaim_garbage(heap, bucket) == 0)
		return 0;

	if (heap_populate_bucket(heap, bucket) == 0)
		return 0;

	int extend;
	if ((extend = heap_extend(heap, bucket, heap->growsize)) < 0)
		return ENOMEM;

	if (extend == 1)
		return 0;

	/*
	 * Extending the pool does not automatically add the chunks into the
	 * runtime state of the bucket - we need to traverse the new zone if
	 * it was created.
	 */
	if (heap_populate_bucket(heap, bucket) == 0)
		return 0;

	return ENOMEM;
}

/*
 * heap_bucket_deref_active -- detaches active blocks from the bucket
 */
static int
heap_bucket_deref_active(struct palloc_heap *heap, struct bucket *b)
{
	/* get rid of the active block in the bucket */
	struct memory_block_reserved **active = &b->active_memory_block;

	if (b->is_active) {
		b->c_ops->rm_all(b->container);
		if (util_fetch_and_sub64(&(*active)->nresv, 1) == 1) {
			VALGRIND_ANNOTATE_HAPPENS_AFTER(&(*active)->nresv);
			heap_discard_run(heap, &(*active)->m);
		} else {
			VALGRIND_ANNOTATE_HAPPENS_BEFORE(&(*active)->nresv);
			*active = NULL;
		}
		b->is_active = 0;
	}

	if (*active == NULL) {
		*active = Zalloc(sizeof(struct memory_block_reserved));
		if (*active == NULL)
			return -1;
	}

	return 0;
}

/*
 * heap_force_recycle -- detaches all memory from arenas, and forces global
 *	recycling of all memory blocks
 */
void
heap_force_recycle(struct palloc_heap *heap)
{
	util_mutex_lock(&heap->rt->arenas.lock);
	struct arena *arenap;
	VEC_FOREACH(arenap, &heap->rt->arenas.vec) {
		for (int i = 0; i < MAX_ALLOCATION_CLASSES; ++i) {
			struct bucket *b = arenap->buckets[i];
			if (b == NULL)
				continue;
			util_mutex_lock(&b->lock);
			/*
			 * There's no need to check if this fails, as that
			 * will not prevent progress in this function.
			 */
			heap_bucket_deref_active(heap, b);
			util_mutex_unlock(&b->lock);
		}
	}
	util_mutex_unlock(&heap->rt->arenas.lock);
	heap_reclaim_garbage(heap, NULL);
}

/*
 * heap_reuse_from_recycler -- (internal) try reusing runs that are currently
 *	in the recycler
 */
static int
heap_reuse_from_recycler(struct palloc_heap *heap,
	struct bucket *b, uint32_t units, int force)
{
	struct memory_block m = MEMORY_BLOCK_NONE;
	m.size_idx = units;

	struct recycler *r = heap->rt->recyclers[b->aclass->id];
	if (!force && recycler_get(r, &m) == 0)
		return heap_run_reuse(heap, b, &m);

	heap_recycle_unused(heap, r, NULL, force);

	if (recycler_get(r, &m) == 0)
		return heap_run_reuse(heap, b, &m);

	return ENOMEM;
}

/*
 * heap_discard_run -- puts the memory block back into the global heap.
 */
void
heap_discard_run(struct palloc_heap *heap, struct memory_block *m)
{
	if (heap_reclaim_run(heap, m, 0)) {
		struct bucket *defb =
			heap_bucket_acquire(heap,
			DEFAULT_ALLOC_CLASS_ID, 0);

		heap_run_into_free_chunk(heap, defb, m);

		heap_bucket_release(heap, defb);
	}
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

	if (heap_bucket_deref_active(heap, b) != 0)
		return ENOMEM;

	if (heap_reuse_from_recycler(heap, b, units, 0) == 0)
		goto out;

	/* search in the next zone before attempting to create a new run */
	struct bucket *defb = heap_bucket_acquire(heap,
		DEFAULT_ALLOC_CLASS_ID,
		HEAP_ARENA_PER_THREAD);
	heap_populate_bucket(heap, defb);
	heap_bucket_release(heap, defb);

	if (heap_reuse_from_recycler(heap, b, units, 0) == 0)
		goto out;

	struct memory_block m = MEMORY_BLOCK_NONE;
	m.size_idx = b->aclass->run.size_idx;

	defb = heap_bucket_acquire(heap,
		DEFAULT_ALLOC_CLASS_ID,
		HEAP_ARENA_PER_THREAD);
	/* cannot reuse an existing run, create a new one */
	if (heap_get_bestfit_block(heap, defb, &m) == 0) {
		ASSERTeq(m.block_off, 0);
		if (heap_run_create(heap, b, &m) != 0) {
			heap_bucket_release(heap, defb);
			return ENOMEM;
		}

		b->active_memory_block->m = m;
		b->is_active = 1;
		b->active_memory_block->bucket = b;
		util_fetch_and_add64(&b->active_memory_block->nresv, 1);

		heap_bucket_release(heap, defb);

		goto out;
	}
	heap_bucket_release(heap, defb);

	if (heap_reuse_from_recycler(heap, b, units, 0) == 0)
		goto out;

	ret = ENOMEM;
out:

	return ret;
}

/*
 * heap_memblock_on_free -- bookkeeping actions executed at every free of a
 *	block
 */
void
heap_memblock_on_free(struct palloc_heap *heap, const struct memory_block *m)
{
	if (m->type != MEMORY_BLOCK_RUN)
		return;

	struct chunk_header *hdr = heap_get_chunk_hdr(heap, m);
	struct chunk_run *run = heap_get_chunk_run(heap, m);

	ASSERTeq(hdr->type, CHUNK_TYPE_RUN);

	struct alloc_class *c = alloc_class_by_run(
		heap->rt->alloc_classes,
		run->hdr.block_size, hdr->flags, hdr->size_idx);

	if (c == NULL)
		return;

	recycler_inc_unaccounted(heap->rt->recyclers[c->id], m);
}

/*
 * heap_split_block -- (internal) splits unused part of the memory block
 */
static void
heap_split_block(struct palloc_heap *heap, struct bucket *b,
		struct memory_block *m, uint32_t units)
{
	ASSERT(units <= UINT16_MAX);
	ASSERT(units > 0);

	if (b->aclass->type == CLASS_RUN) {
		ASSERT((uint64_t)m->block_off + (uint64_t)units <= UINT32_MAX);
		struct memory_block r = {m->chunk_id, m->zone_id,
			m->size_idx - units, (uint32_t)(m->block_off + units),
			NULL, NULL, 0, 0};
		memblock_rebuild_state(heap, &r);
		if (bucket_insert_block(b, &r) != 0)
			LOG(2,
				"failed to allocate memory block runtime tracking info");
	} else {
		uint32_t new_chunk_id = m->chunk_id + units;
		uint32_t new_size_idx = m->size_idx - units;

		struct memory_block n = memblock_huge_init(heap,
			new_chunk_id, m->zone_id, new_size_idx);

		*m = memblock_huge_init(heap, m->chunk_id, m->zone_id, units);

		if (bucket_insert_block(b, &n) != 0)
			LOG(2,
				"failed to allocate memory block runtime tracking info");
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
		heap_split_block(heap, b, m, units);

	m->m_ops->ensure_header_type(m, b->aclass->header_type);
	m->header_type = b->aclass->header_type;

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
	struct memory_block ret = MEMORY_BLOCK_NONE;

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
	ASSERT(h->rt->nzones > 0);

	struct zone *last_zone = ZID_TO_ZONE(h->layout, h->rt->nzones - 1);

	return &last_zone->chunks[last_zone->header.size_idx];
}

/*
 * heap_arena_create -- create a new arena, push it to the vector
 * and return new arena id or -1 on failure
 */
int
heap_arena_create(struct palloc_heap *heap)
{
	struct heap_rt *h = heap->rt;

	struct arena *arena = heap_arena_new(heap, 0);
	if (arena == NULL)
		return -1;

	util_mutex_lock(&h->arenas.lock);

	if (VEC_PUSH_BACK(&h->arenas.vec, arena))
		goto err_push_back;

	int ret = (int)VEC_SIZE(&h->arenas.vec);
	util_mutex_unlock(&h->arenas.lock);

	return ret;

err_push_back:
	util_mutex_unlock(&h->arenas.lock);
	heap_arena_delete(arena);
	return -1;
}

/*
 * heap_get_narenas_total -- returns the number of all arenas in the heap
 */
unsigned
heap_get_narenas_total(struct palloc_heap *heap)
{
	struct heap_rt *h = heap->rt;

	util_mutex_lock(&h->arenas.lock);
	unsigned total = (unsigned)VEC_SIZE(&h->arenas.vec);
	util_mutex_unlock(&h->arenas.lock);

	return total;
}

/*
 * heap_get_narenas_max -- returns the max number of arenas
 */
unsigned
heap_get_narenas_max(struct palloc_heap *heap)
{
	struct heap_rt *h = heap->rt;

	util_mutex_lock(&h->arenas.lock);
	unsigned max = (unsigned)VEC_CAPACITY(&h->arenas.vec);
	util_mutex_unlock(&h->arenas.lock);

	return max;
}

/*
 * heap_set_narenas_max -- change the max number of arenas
 */
int
heap_set_narenas_max(struct palloc_heap *heap, unsigned size)
{
	struct heap_rt *h = heap->rt;
	int ret = -1;

	util_mutex_lock(&h->arenas.lock);
	unsigned capacity = (unsigned)VEC_CAPACITY(&h->arenas.vec);
	if (size < capacity) {
		LOG(2, "cannot decrease max number of arenas");
		goto out;
	} else if (size == capacity) {
		ret = 0;
		goto out;
	}

	ret = VEC_RESERVE(&h->arenas.vec, size);

out:
	util_mutex_unlock(&h->arenas.lock);
	return ret;
}

/*
 * heap_get_narenas_auto -- returns the number of all automatic arenas
 */
unsigned
heap_get_narenas_auto(struct palloc_heap *heap)
{
	struct heap_rt *h = heap->rt;
	struct arena *arena;
	unsigned narenas = 0;

	util_mutex_lock(&h->arenas.lock);

	VEC_FOREACH(arena, &h->arenas.vec) {
		if (arena->automatic)
			narenas++;
	}

	util_mutex_unlock(&h->arenas.lock);

	return narenas;
}

/*
 * heap_get_arena_buckets -- returns a pointer to buckets from the arena
 */
struct bucket **
heap_get_arena_buckets(struct palloc_heap *heap, unsigned arena_id)
{
	util_mutex_lock(&heap->rt->arenas.lock);
	struct arena *a = heap_get_arena_by_id(heap, arena_id);
	util_mutex_unlock(&heap->rt->arenas.lock);

	return a->buckets;
}

/*
 * heap_get_arena_auto -- returns arena automatic value
 */
int
heap_get_arena_auto(struct palloc_heap *heap, unsigned arena_id)
{
	util_mutex_lock(&heap->rt->arenas.lock);
	struct arena *a = heap_get_arena_by_id(heap, arena_id);
	util_mutex_unlock(&heap->rt->arenas.lock);

	return a->automatic;
}

/*
 * heap_set_arena_auto -- sets arena automatic value
 */
int
heap_set_arena_auto(struct palloc_heap *heap, unsigned arena_id,
		int automatic)
{
	unsigned nautomatic = 0;
	struct arena *a;
	struct heap_rt *h = heap->rt;
	int ret = 0;

	util_mutex_lock(&h->arenas.lock);
	VEC_FOREACH(a, &h->arenas.vec)
		if (a->automatic)
			nautomatic++;

	a = VEC_ARR(&heap->rt->arenas.vec)[arena_id - 1];

	if (!automatic && nautomatic <= 1 && a->automatic) {
		ERR("at least one automatic arena must exist");
		ret = -1;
		goto out;
	}
	a->automatic = automatic;

out:
	util_mutex_unlock(&h->arenas.lock);
	return ret;

}

/*
 * heap_set_arena_thread -- assign arena with given id to the current thread
 */
void
heap_set_arena_thread(struct palloc_heap *heap, unsigned arena_id)
{
	os_mutex_lock(&heap->rt->arenas.lock);
	heap_arena_thread_attach(heap, heap_get_arena_by_id(heap, arena_id));
	os_mutex_unlock(&heap->rt->arenas.lock);
}

/*
 * heap_get_procs -- (internal) returns the number of arenas to create
 */
static unsigned
heap_get_procs(void)
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

	if (c->type == CLASS_RUN) {
		h->recyclers[c->id] = recycler_new(heap, c->run.nallocs,
			&heap->rt->arenas.nactive);
		if (h->recyclers[c->id] == NULL)
			goto error_recycler_new;
	}

	size_t i;
	struct arena *arena;
	VEC_FOREACH_BY_POS(i, &h->arenas.vec) {
		arena = VEC_ARR(&h->arenas.vec)[i];
		if (arena->buckets[c->id] == NULL)
			arena->buckets[c->id] = bucket_new(
				container_new_seglists(heap), c);
		if (arena->buckets[c->id] == NULL)
			goto error_cache_bucket_new;
	}

	return 0;

error_cache_bucket_new:
	recycler_delete(h->recyclers[c->id]);

	for (; i != 0; --i)
		bucket_delete(VEC_ARR(&h->arenas.vec)[i - 1]->buckets[c->id]);

error_recycler_new:
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

	h->default_bucket = bucket_new(container_new_ravl(heap),
		alloc_class_by_id(h->alloc_classes, DEFAULT_ALLOC_CLASS_ID));

	if (h->default_bucket == NULL)
		goto error_bucket_create;

	return 0;

error_bucket_create: {
		struct arena *arena;
		VEC_FOREACH(arena, &h->arenas.vec)
			heap_arena_delete(arena);
	}
	return -1;
}

/*
 * heap_extend -- extend the heap by the given size
 *
 * Returns 0 if the current zone has been extended, 1 if a new zone had to be
 *	created, -1 if unsuccessful.
 *
 * If this function has to create a new zone, it will NOT populate buckets with
 * the new chunks.
 */
int
heap_extend(struct palloc_heap *heap, struct bucket *b, size_t size)
{
	void *nptr = util_pool_extend(heap->set, &size, PMEMOBJ_MIN_PART);
	if (nptr == NULL)
		return -1;

	*heap->sizep += size;
	pmemops_persist(&heap->p_ops, heap->sizep, sizeof(*heap->sizep));

	/*
	 * If interrupted after changing the size, the heap will just grow
	 * automatically on the next heap_boot.
	 */

	uint32_t nzones = heap_max_zone(*heap->sizep);
	uint32_t zone_id = nzones - 1;
	struct zone *z = ZID_TO_ZONE(heap->layout, zone_id);
	uint32_t chunk_id = heap->rt->nzones == nzones ? z->header.size_idx : 0;
	heap_zone_init(heap, zone_id, chunk_id);

	if (heap->rt->nzones != nzones) {
		heap->rt->nzones = nzones;
		return 0;
	}

	struct chunk_header *hdr = &z->chunk_headers[chunk_id];

	struct memory_block m = MEMORY_BLOCK_NONE;
	m.chunk_id = chunk_id;
	m.zone_id = zone_id;
	m.block_off = 0;
	m.size_idx = hdr->size_idx;
	memblock_rebuild_state(heap, &m);

	heap_free_chunk_reuse(heap, b, &m);

	return 1;
}

/*
 * heap_zone_update_if_needed -- updates the zone metadata if the pool has been
 *	extended.
 */
static void
heap_zone_update_if_needed(struct palloc_heap *heap)
{
	struct zone *z;

	for (uint32_t i = 0; i < heap->rt->nzones; ++i) {
		z = ZID_TO_ZONE(heap->layout, i);
		if (z->header.magic != ZONE_HEADER_MAGIC)
			continue;

		size_t size_idx = zone_calc_size_idx(i, heap->rt->nzones,
			*heap->sizep);

		if (size_idx == z->header.size_idx)
			continue;

		heap_zone_init(heap, i, z->header.size_idx);
	}
}

/*
 * heap_boot -- opens the heap region of the pmemobj pool
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
heap_boot(struct palloc_heap *heap, void *heap_start, uint64_t heap_size,
		uint64_t *sizep, void *base, struct pmem_ops *p_ops,
		struct stats *stats, struct pool_set *set)
{
	/*
	 * The size can be 0 if interrupted during heap_init or this is the
	 * first time booting the heap with the persistent size field.
	 */
	if (*sizep == 0) {
		*sizep = heap_size;
		pmemops_persist(p_ops, sizep, sizeof(*sizep));
	}

	if (heap_size < *sizep) {
		ERR("mapped region smaller than the heap size");
		return EINVAL;
	}

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

	unsigned narenas_default = heap_get_procs();

	if (heap_arenas_init(&h->arenas) != 0) {
		err = errno;
		goto error_arenas_malloc;
	}

	h->nzones = heap_max_zone(heap_size);

	h->zones_exhausted = 0;

	h->nlocks = On_valgrind ? MAX_RUN_LOCKS_VG : MAX_RUN_LOCKS;
	for (unsigned i = 0; i < h->nlocks; ++i)
		util_mutex_init(&h->run_locks[i]);

	if ((err = os_tls_key_create(&h->arenas.thread,
			heap_thread_arena_destructor)) != 0) {
		goto error_key_create;
	}

	heap->p_ops = *p_ops;
	heap->layout = heap_start;
	heap->rt = h;
	heap->sizep = sizep;
	heap->base = base;
	heap->stats = stats;
	heap->set = set;
	heap->growsize = HEAP_DEFAULT_GROW_SIZE;
	heap->alloc_pattern = PALLOC_CTL_DEBUG_NO_PATTERN;
	VALGRIND_DO_CREATE_MEMPOOL(heap->layout, 0, 0);

	for (unsigned i = 0; i < narenas_default; ++i) {
		if (VEC_PUSH_BACK(&h->arenas.vec, heap_arena_new(heap, 1))) {
			err = errno;
			goto error_vec_reserve;
		}
	}

	for (unsigned i = 0; i < MAX_ALLOCATION_CLASSES; ++i)
		h->recyclers[i] = NULL;

	heap_zone_update_if_needed(heap);

	return 0;

error_vec_reserve:
	os_tls_key_delete(h->arenas.thread);
error_key_create:
	heap_arenas_fini(&h->arenas);
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
heap_write_header(struct heap_header *hdr)
{
	struct heap_header newhdr = {
		.signature = HEAP_SIGNATURE,
		.major = HEAP_MAJOR,
		.minor = HEAP_MINOR,
		.unused = 0,
		.chunksize = CHUNKSIZE,
		.chunks_per_zone = MAX_CHUNK,
		.reserved = {0},
		.checksum = 0
	};

	util_checksum(&newhdr, sizeof(newhdr), &newhdr.checksum, 1, 0);
	*hdr = newhdr;
}

/*
 * heap_init -- initializes the heap
 *
 * If successful function returns zero. Otherwise an error number is returned.
 */
int
heap_init(void *heap_start, uint64_t heap_size, uint64_t *sizep,
	struct pmem_ops *p_ops)
{
	if (heap_size < HEAP_MIN_SIZE)
		return EINVAL;

	VALGRIND_DO_MAKE_MEM_UNDEFINED(heap_start, heap_size);

	struct heap_layout *layout = heap_start;
	heap_write_header(&layout->header);
	pmemops_persist(p_ops, &layout->header, sizeof(struct heap_header));

	unsigned zones = heap_max_zone(heap_size);
	for (unsigned i = 0; i < zones; ++i) {
		struct zone *zone = ZID_TO_ZONE(layout, i);
		pmemops_memset(p_ops, &zone->header, 0,
				sizeof(struct zone_header), 0);
		pmemops_memset(p_ops, &zone->chunk_headers, 0,
				sizeof(struct chunk_header), 0);

		/* only explicitly allocated chunks should be accessible */
		VALGRIND_DO_MAKE_MEM_NOACCESS(&zone->chunk_headers,
			sizeof(struct chunk_header));
	}

	*sizep = heap_size;
	pmemops_persist(p_ops, sizep, sizeof(*sizep));

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

	os_tls_key_delete(rt->arenas.thread);
	bucket_delete(rt->default_bucket);

	struct arena *arena;
	VEC_FOREACH(arena, &rt->arenas.vec)
		heap_arena_delete(arena);

	for (unsigned i = 0; i < rt->nlocks; ++i)
		util_mutex_destroy(&rt->run_locks[i]);

	heap_arenas_fini(&rt->arenas);

	for (int i = 0; i < MAX_ALLOCATION_CLASSES; ++i) {
		if (heap->rt->recyclers[i] == NULL)
			continue;

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
	if (util_checksum(hdr, sizeof(*hdr), &hdr->checksum, 0, 0) != 1) {
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
	if (hdr->magic != ZONE_HEADER_MAGIC) /* not initialized */
		return 0;

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

	if (heap_verify_header(&layout->header))
		return -1;

	for (unsigned i = 0; i < heap_max_zone(heap_size); ++i) {
		if (heap_verify_zone(ZID_TO_ZONE(layout, i)))
			return -1;
	}

	return 0;
}

/*
 * heap_check_remote -- verifies if the heap of a remote pool is consistent
 *	and can be opened properly
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

	if (heap_verify_header(&header))
		return -1;

	struct zone *zone_buff = (struct zone *)Malloc(sizeof(struct zone));
	if (zone_buff == NULL) {
		ERR("heap: zone_buff malloc error");
		return -1;
	}
	for (unsigned i = 0; i < heap_max_zone(heap_size); ++i) {
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
		struct chunk_header *hdr = heap_get_chunk_hdr(heap, m);
		memblock_rebuild_state(heap, m);
		m->size_idx = hdr->size_idx;

		if (m->m_ops->iterate_used(m, cb, arg) != 0)
			return 1;

		m->chunk_id += m->size_idx;
		m->block_off = 0;
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
	for (; m.zone_id < heap->rt->nzones; ++m.zone_id) {
		if (heap_zone_foreach_object(heap, cb, arg, &m) != 0)
			break;

		m.chunk_id = 0;
	}
}

#if VG_MEMCHECK_ENABLED

/*
 * heap_vg_open -- notifies Valgrind about heap layout
 */
void
heap_vg_open(struct palloc_heap *heap, object_callback cb,
	void *arg, int objects)
{
	ASSERTne(cb, NULL);
	VALGRIND_DO_MAKE_MEM_UNDEFINED(heap->layout, *heap->sizep);

	struct heap_layout *layout = heap->layout;

	VALGRIND_DO_MAKE_MEM_DEFINED(&layout->header, sizeof(layout->header));

	unsigned zones = heap_max_zone(*heap->sizep);

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

			/* define the header before rebuilding state */
			VALGRIND_DO_MAKE_MEM_DEFINED(hdr, sizeof(*hdr));

			m.chunk_id = c;
			m.size_idx = hdr->size_idx;

			memblock_rebuild_state(heap, &m);

			m.m_ops->vg_init(&m, objects, cb, arg);
			m.block_off = 0;

			ASSERT(hdr->size_idx > 0);

			c += hdr->size_idx;
		}

		/* mark all unused chunk headers after last as not accessible */
		VALGRIND_DO_MAKE_MEM_NOACCESS(&z->chunk_headers[chunks],
			(MAX_CHUNK - chunks) * sizeof(struct chunk_header));
	}
}
#endif
