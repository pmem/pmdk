/*
 * Copyright 2016-2017, Intel Corporation
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
 * alloc_class.h -- internal definitions for allocation classes
 */

#ifndef LIBPMEMOBJ_ALLOC_CLASS_H
#define LIBPMEMOBJ_ALLOC_CLASS_H 1

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include "memblock.h"

#define MAX_ALLOCATION_CLASSES (UINT8_MAX)
#define DEFAULT_ALLOC_CLASS_ID (0)
#define RUN_UNIT_MAX BITS_PER_VALUE

struct alloc_class_collection;

enum alloc_class_type {
	CLASS_UNKNOWN,
	CLASS_HUGE,
	CLASS_RUN,

	MAX_ALLOC_CLASS_TYPES
};

struct alloc_class_run_proto {
	/*
	 * Last value of a bitmap representing completely free
	 * run from this bucket.
	 */
	uint64_t bitmap_lastval;

	/*
	 * Number of 8 byte values this run bitmap is
	 * composed of.
	 */
	unsigned bitmap_nval;

	/*
	 * Number of allocations that can be performed from a
	 * single run.
	 */
	unsigned bitmap_nallocs;

	/*
	 * The size index of a single run instance.
	 */
	uint32_t size_idx;
};

struct alloc_class {
	uint8_t id;

	size_t unit_size;
	enum header_type header_type;
	enum alloc_class_type type;

	union {
		/* struct { ... } huge; */
		struct alloc_class_run_proto run;
	};
};

struct alloc_class_collection *alloc_class_collection_new(void);
void alloc_class_collection_delete(struct alloc_class_collection *ac);

void alloc_class_generate_run_proto(struct alloc_class_run_proto *dest,
	size_t unit_size, uint32_t size_idx);

struct alloc_class *alloc_class_by_run(
	struct alloc_class_collection *ac,
	size_t unit_size, enum header_type header_type, uint32_t size_idx);
struct alloc_class *alloc_class_by_alloc_size(
	struct alloc_class_collection *ac, size_t size);
struct alloc_class *alloc_class_by_id(
	struct alloc_class_collection *ac, uint8_t id);

int alloc_class_reserve(struct alloc_class_collection *ac, uint8_t id);
int alloc_class_find_first_free_slot(struct alloc_class_collection *ac,
	uint8_t *slot);

ssize_t
alloc_class_calc_size_idx(struct alloc_class *c, size_t size);

struct alloc_class *
alloc_class_register(struct alloc_class_collection *ac,
	struct alloc_class *c);

void alloc_class_delete(struct alloc_class_collection *ac,
	struct alloc_class *c);

#endif
