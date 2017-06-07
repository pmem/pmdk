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
 * palloc.h -- internal definitions for persistent allocator
 */

#ifndef LIBPMEMOBJ_PALLOC_H
#define LIBPMEMOBJ_PALLOC_H 1

#include <stddef.h>
#include <stdint.h>

#include "memops.h"
#include "redo.h"

struct palloc_heap {
	struct pmem_ops p_ops;
	struct heap_layout *layout;
	struct heap_rt *rt;
	uint64_t size;
	uint64_t run_id;

	void *base;
};

struct memory_block;

typedef int (*palloc_constr)(void *base, void *ptr,
		size_t usable_size, void *arg);

int palloc_operation(struct palloc_heap *heap, uint64_t off, uint64_t *dest_off,
	size_t size, palloc_constr constructor, void *arg,
	uint64_t extra_field, uint16_t object_flags, uint16_t class_id,
	struct operation_context *ctx);

uint64_t palloc_first(struct palloc_heap *heap);
uint64_t palloc_next(struct palloc_heap *heap, uint64_t off);

size_t palloc_usable_size(struct palloc_heap *heap, uint64_t off);
uint64_t palloc_extra(struct palloc_heap *heap, uint64_t off);
uint16_t palloc_flags(struct palloc_heap *heap, uint64_t off);

int palloc_boot(struct palloc_heap *heap, void *heap_start, uint64_t run_id,
		uint64_t heap_size, void *base, struct pmem_ops *p_ops);
int palloc_buckets_init(struct palloc_heap *heap);

int palloc_init(void *heap_start, uint64_t heap_size, struct pmem_ops *p_ops);
void *palloc_heap_end(struct palloc_heap *h);
int palloc_heap_check(void *heap_start, uint64_t heap_size);
int palloc_heap_check_remote(void *heap_start, uint64_t heap_size,
		struct remote_ops *ops);
void palloc_heap_cleanup(struct palloc_heap *heap);

/* foreach callback, terminates iteration if return value is non-zero */
typedef int (*object_callback)(const struct memory_block *m, void *arg);

#ifdef USE_VG_MEMCHECK
void palloc_heap_vg_open(struct palloc_heap *heap, int objects);
#endif

#endif
