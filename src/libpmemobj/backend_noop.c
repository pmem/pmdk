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
 * backend_noop.c -- implementation of noop backend
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
#include "backend_noop.h"

static struct bucket_backend_operations noop_bucket_ops = {

};

static struct arena_backend_operations noop_arena_ops = {
	.set_alloc_ptr = noop_set_alloc_ptr
};

static struct pool_backend_operations noop_pool_ops = {

};

/*
 * noop_backend_open -- opens a backend with all no-op functions
 */
struct backend *
backend_noop_open(void *ptr, size_t size)
{
	struct backend_noop *backend = Malloc(sizeof (*backend));
	if (backend == NULL) {
		goto error_backend_malloc;
	}

	backend_init(&(backend->super), BACKEND_NOOP,
		&noop_bucket_ops, &noop_arena_ops, &noop_pool_ops);

	return (struct backend *)backend;

error_backend_malloc:
	return NULL;
}


/*
 * noop_backend_close -- closes a noop backend
 */
void
backend_noop_close(struct backend *backend)
{
	ASSERT(backend->type == BACKEND_NOOP);

	struct backend_noop *noop_backend =
		(struct backend_noop *)backend;

	Free(noop_backend);
}

/*
 * noop_set_alloc_ptr -- no-op implementation of set_alloc_ptr
 */
void
noop_set_alloc_ptr(struct arena *arena, uint64_t *ptr,
	uint64_t value)
{
	/* no-op */
}
