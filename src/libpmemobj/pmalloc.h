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
 * pmalloc.h -- internal definitions for persistent malloc
 */

#ifndef LIBPMEMOBJ_PMALLOC_H
#define LIBPMEMOBJ_PMALLOC_H 1

#include <stddef.h>
#include <stdint.h>

#include "libpmemobj.h"
#include "memops.h"
#include "palloc.h"

/*
 * The maximum number of entries in redo log used by the allocator. The common
 * case is to use two, one for modification of the object destination memory
 * location and the second for applying the chunk metadata modifications.
 */
#define ALLOC_REDO_LOG_SIZE 10
struct lane_alloc_layout {
	struct redo_log redo[ALLOC_REDO_LOG_SIZE];
};

int pmalloc_operation(struct palloc_heap *heap,
	uint64_t off, uint64_t *dest_off, size_t size,
	palloc_constr constructor, void *arg,
	uint64_t extra_field, uint16_t object_flags, uint16_t class_id,
	struct operation_context *ctx);

int pmalloc(PMEMobjpool *pop, uint64_t *off, size_t size,
	uint64_t extra_field, uint16_t object_flags);
int pmalloc_construct(PMEMobjpool *pop, uint64_t *off, size_t size,
	palloc_constr constructor, void *arg,
	uint64_t extra_field, uint16_t object_flags, uint16_t class_id);

int prealloc(PMEMobjpool *pop, uint64_t *off, size_t size,
	uint64_t extra_field, uint16_t object_flags);

void pfree(PMEMobjpool *pop, uint64_t *off);

struct redo_log *pmalloc_redo_hold(PMEMobjpool *pop);
void pmalloc_redo_release(PMEMobjpool *pop);

void pmalloc_ctl_register(PMEMobjpool *pop);

#endif
