/*
 * Copyright (c) 2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * persistent_backend.c -- implementation of persistent backend
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <libpmem.h>
#include "bucket.h"
#include "arena.h"
#include "backend.h"
#include "pool.h"
#include "util.h"
#include "out.h"
#include "persistent_backend.h"

static struct bucket_backend_operations persistent_bucket_ops = {

};

static struct arena_backend_operations persistent_arena_ops = {
	.set_alloc_ptr = persistent_set_alloc_ptr
};

static struct pool_backend_operations persistent_pool_ops = {

};

/*
 * persistent_backend_open -- opens a persistent backend
 */
struct backend *
persistent_backend_open(void *ptr, size_t size)
{
	struct persistent_backend *backend = Malloc(sizeof (*backend));
	if (backend == NULL) {
		goto error_backend_malloc;
	}

	backend_open(&(backend->super), BACKEND_PERSISTENT,
		&persistent_bucket_ops,
		&persistent_arena_ops,
		&persistent_pool_ops);

	backend->is_pmem = pmem_is_pmem(ptr, size);
	/*
	 * Casting is necessery to silence the compiler about msync returning
	 * an int instead of nothing - the return value is not used anyway.
	 */
	if (backend->is_pmem)
		backend->persist = (persist_func)pmem_persist;
	else
		backend->persist = (persist_func)pmem_msync;

	return (struct backend *)backend;

error_backend_malloc:
	return NULL;
}

/*
 * persistent_backend_close -- closes a persistent backend
 */
void
persistent_backend_close(struct backend *backend)
{
	ASSERT(backend->type == BACKEND_PERSISTENT);

	struct persistent_backend *persistent_backend =
		(struct persistent_backend *)backend;

	backend_close(&(persistent_backend->super));
	Free(persistent_backend);
}

/*
 * persistent_set_alloc_ptr -- persistent implementation of set_alloc_ptr
 */
void
persistent_set_alloc_ptr(struct arena *arena, uint64_t *ptr,
	uint64_t value)
{
	struct persistent_backend *backend =
		(struct persistent_backend *)arena->pool->backend;

	*ptr = value;
	backend->persist(ptr, sizeof (*ptr));
}
