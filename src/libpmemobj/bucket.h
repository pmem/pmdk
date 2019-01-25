/*
 * Copyright 2015-2019, Intel Corporation
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
 * bucket.h -- internal definitions for bucket
 */

#ifndef LIBPMEMOBJ_BUCKET_H
#define LIBPMEMOBJ_BUCKET_H 1

#include <stddef.h>
#include <stdint.h>

#include "container.h"
#include "memblock.h"
#include "os_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CALC_SIZE_IDX(_unit_size, _size)\
((_size) == 0 ? 0 : (uint32_t)((((_size) - 1) / (_unit_size)) + 1))

struct bucket {
	os_mutex_t lock;

	struct alloc_class *aclass;

	struct block_container *container;
	const struct block_container_ops *c_ops;

	struct memory_block_reserved *active_memory_block;
	int is_active;
};

struct bucket *bucket_new(struct block_container *c,
	struct alloc_class *aclass);

int *bucket_current_resvp(struct bucket *b);

int bucket_insert_block(struct bucket *b, const struct memory_block *m);

void bucket_delete(struct bucket *b);

#ifdef __cplusplus
}
#endif

#endif
