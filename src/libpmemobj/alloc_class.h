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
 * alloc_class.h -- internal definitions for allocation classes
 */

#ifndef LIBPMEMOBJ_ALLOC_CLASS_H
#define LIBPMEMOBJ_ALLOC_CLASS_H 1

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include "heap_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ALLOCATION_CLASSES (UINT8_MAX)
#define DEFAULT_ALLOC_CLASS_ID (0)
#define RUN_UNIT_MAX RUN_BITS_PER_VALUE

struct alloc_class_collection;

enum alloc_class_type {
	CLASS_UNKNOWN,
	CLASS_HUGE,
	CLASS_RUN,

	MAX_ALLOC_CLASS_TYPES
};

struct alloc_class {
	uint8_t id;
	uint16_t flags;

	size_t unit_size;

	enum header_type header_type;
	enum alloc_class_type type;

	/* run-specific data */
	struct {
		uint32_t size_idx; /* size index of a single run instance */
		size_t alignment; /* required alignment of objects */
		unsigned nallocs; /* number of allocs per run */
	} run;
};

struct alloc_class_collection *alloc_class_collection_new(void);
void alloc_class_collection_delete(struct alloc_class_collection *ac);

struct alloc_class *alloc_class_by_run(
	struct alloc_class_collection *ac,
	size_t unit_size, uint16_t flags, uint32_t size_idx);
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
alloc_class_new(int id, struct alloc_class_collection *ac,
	enum alloc_class_type type, enum header_type htype,
	size_t unit_size, size_t alignment,
	uint32_t size_idx);

void alloc_class_delete(struct alloc_class_collection *ac,
	struct alloc_class *c);

#ifdef __cplusplus
}
#endif

#endif
