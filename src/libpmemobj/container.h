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
 * container.h -- internal definitions for block containers
 */

#ifndef LIBPMEMOBJ_CONTAINER_H
#define LIBPMEMOBJ_CONTAINER_H 1

#include "memblock.h"

#ifdef __cplusplus
extern "C" {
#endif

struct block_container {
	const struct block_container_ops *c_ops;
	struct palloc_heap *heap;
};

struct block_container_ops {
	/* inserts a new memory block into the container */
	int (*insert)(struct block_container *c, const struct memory_block *m);

	/* removes exact match memory block */
	int (*get_rm_exact)(struct block_container *c,
		const struct memory_block *m);

	/* removes and returns the best-fit memory block for size */
	int (*get_rm_bestfit)(struct block_container *c,
		struct memory_block *m);

	/* checks whether the container is empty */
	int (*is_empty)(struct block_container *c);

	/* removes all elements from the container */
	void (*rm_all)(struct block_container *c);

	/* deletes the container */
	void (*destroy)(struct block_container *c);
};

#ifdef __cplusplus
}
#endif

#endif /* LIBPMEMOBJ_CONTAINER_H */
