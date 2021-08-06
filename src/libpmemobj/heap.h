/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2021, Intel Corporation */

/*
 * heap.h -- internal definitions for heap
 */

#ifndef LIBPMEMOBJ_HEAP_H
#define LIBPMEMOBJ_HEAP_H 1

#include <stddef.h>
#include <stdint.h>

#include "bucket.h"
#include "memblock.h"
#include "memops.h"
#include "palloc.h"
#include "os_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

extern enum pobj_arenas_assignment_type Default_arenas_assignment_type;
extern ssize_t Default_arenas_max;

#define HEAP_OFF_TO_PTR(heap, off) ((void *)((char *)((heap)->base) + (off)))
#define HEAP_PTR_TO_OFF(heap, ptr)\
	((uintptr_t)(ptr) - (uintptr_t)((heap)->base))

#define BIT_IS_CLR(a, i)	(!((a) & (1ULL << (i))))
#define HEAP_ARENA_PER_THREAD (0)

int heap_boot(struct palloc_heap *heap, void *heap_start, uint64_t heap_size,
		uint64_t *sizep,
		void *base, struct pmem_ops *p_ops,
		struct stats *stats, struct pool_set *set);
int heap_init(void *heap_start, uint64_t heap_size, uint64_t *sizep,
	struct pmem_ops *p_ops);
void heap_cleanup(struct palloc_heap *heap);
int heap_check(void *heap_start, uint64_t heap_size);
int heap_check_remote(void *heap_start, uint64_t heap_size,
		struct remote_ops *ops);
int heap_buckets_init(struct palloc_heap *heap);
int heap_create_alloc_class_buckets(struct palloc_heap *heap,
	struct alloc_class *c);

int heap_extend(struct palloc_heap *heap, struct bucket *defb, size_t size);

struct alloc_class *
heap_get_best_class(struct palloc_heap *heap, size_t size);

struct bucket *
heap_bucket_acquire(struct palloc_heap *heap, uint8_t class_id,
		uint16_t arena_id);

void
heap_bucket_release(struct palloc_heap *heap, struct bucket *b);

int heap_get_bestfit_block(struct palloc_heap *heap, struct bucket *b,
	struct memory_block *m);
struct memory_block
heap_coalesce_huge(struct palloc_heap *heap, struct bucket *b,
	const struct memory_block *m);
os_mutex_t *heap_get_run_lock(struct palloc_heap *heap,
		uint32_t chunk_id);

void
heap_force_recycle(struct palloc_heap *heap);

void
heap_discard_run(struct palloc_heap *heap, struct memory_block *m);

void
heap_memblock_on_free(struct palloc_heap *heap, const struct memory_block *m);

int
heap_free_chunk_reuse(struct palloc_heap *heap,
	struct bucket *bucket, struct memory_block *m);

void heap_foreach_object(struct palloc_heap *heap, object_callback cb,
	void *arg, struct memory_block start);

struct alloc_class_collection *heap_alloc_classes(struct palloc_heap *heap);

void *heap_end(struct palloc_heap *heap);

unsigned heap_get_narenas_total(struct palloc_heap *heap);

unsigned heap_get_narenas_max(struct palloc_heap *heap);

int heap_set_narenas_max(struct palloc_heap *heap, unsigned size);

unsigned heap_get_narenas_auto(struct palloc_heap *heap);

unsigned heap_get_thread_arena_id(struct palloc_heap *heap);

int heap_arena_create(struct palloc_heap *heap);

struct bucket **
heap_get_arena_buckets(struct palloc_heap *heap, unsigned arena_id);

int heap_get_arena_auto(struct palloc_heap *heap, unsigned arena_id);

int heap_set_arena_auto(struct palloc_heap *heap, unsigned arena_id,
		int automatic);

void heap_set_arena_thread(struct palloc_heap *heap, unsigned arena_id);

unsigned heap_get_procs(void);

void heap_vg_open(struct palloc_heap *heap, object_callback cb,
		void *arg, int objects);

static inline struct chunk_header *
heap_get_chunk_hdr(struct palloc_heap *heap, const struct memory_block *m)
{
	return GET_CHUNK_HDR(heap->layout, m->zone_id, m->chunk_id);
}

static inline struct chunk *
heap_get_chunk(struct palloc_heap *heap, const struct memory_block *m)
{
	return GET_CHUNK(heap->layout, m->zone_id, m->chunk_id);
}

static inline struct chunk_run *
heap_get_chunk_run(struct palloc_heap *heap, const struct memory_block *m)
{
	return GET_CHUNK_RUN(heap->layout, m->zone_id, m->chunk_id);
}

#ifdef __cplusplus
}
#endif

#endif
