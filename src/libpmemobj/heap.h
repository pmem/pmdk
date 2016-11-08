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
 * heap.h -- internal definitions for heap
 */

#ifndef LIBPMEMOBJ_HEAP_H
#define LIBPMEMOBJ_HEAP_H 1

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include "bucket.h"
#include "heap_layout.h"
#include "memblock.h"
#include "memops.h"
#include "palloc.h"

#define MAX_BUCKETS (UINT8_MAX)
#define RUN_UNIT_MAX 64U
#define RUN_UNIT_MAX_ALLOC 8U

/*
 * Every allocation has to be a multiple of a cacheline because we need to
 * ensure proper alignment of every pmem structure.
 */
#define ALLOC_BLOCK_SIZE 64

/*
 * Converts size (in bytes) to number of allocation blocks.
 */
#define SIZE_TO_ALLOC_BLOCKS(_s) (1 + (((_s) - 1) / ALLOC_BLOCK_SIZE))

#define BIT_IS_CLR(a, i)	(!((a) & (1ULL << (i))))

int heap_boot(struct palloc_heap *heap, void *heap_start, uint64_t heap_size,
		void *base, struct pmem_ops *p_ops);
int heap_init(void *heap_start, uint64_t heap_size, struct pmem_ops *p_ops);
void heap_vg_open(void *heap_start, uint64_t heap_size);
void heap_cleanup(struct palloc_heap *heap);
int heap_check(void *heap_start, uint64_t heap_size);
int heap_check_remote(void *heap_start, uint64_t heap_size,
		struct remote_ops *ops);

struct bucket *heap_get_best_bucket(struct palloc_heap *heap, size_t size);
struct bucket *heap_get_chunk_bucket(struct palloc_heap *heap,
		uint32_t chunk_id, uint32_t zone_id);
struct bucket *heap_get_auxiliary_bucket(struct palloc_heap *heap,
		size_t size);
void heap_drain_to_auxiliary(struct palloc_heap *heap, struct bucket *auxb,
	uint32_t size_idx);
void *heap_get_block_data(struct palloc_heap *heap, struct memory_block m);

int heap_get_adjacent_free_block(struct palloc_heap *heap, struct bucket *b,
	struct memory_block *m, struct memory_block cnt, int prev);
void heap_chunk_write_footer(struct chunk_header *hdr, uint32_t size_idx);

int heap_get_bestfit_block(struct palloc_heap *heap, struct bucket *b,
	struct memory_block *m);
int heap_get_exact_block(struct palloc_heap *heap, struct bucket *b,
	struct memory_block *m, uint32_t new_size_idx);
void heap_degrade_run_if_empty(struct palloc_heap *heap, struct bucket *b,
		struct memory_block m);

pthread_mutex_t *heap_get_run_lock(struct palloc_heap *heap,
		uint32_t chunk_id);

struct memory_block heap_free_block(struct palloc_heap *heap, struct bucket *b,
	struct memory_block m, struct operation_context *ctx);

/* foreach callback, terminates iteration if return value is non-zero */
typedef int (*object_callback)(uint64_t off, void *arg);

void heap_foreach_object(struct palloc_heap *heap, object_callback cb,
	void *arg, struct memory_block start);

void *heap_end(struct palloc_heap *heap);

#endif
