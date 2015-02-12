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
 * backend.c -- implementation of backend
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "bucket.h"
#include "arena.h"
#include "backend.h"
#include "backend_persistent.h"
#include "backend_noop.h"
#include "pool.h"

struct backend *(*backend_open_by_type[MAX_BACKEND])
	(void *ptr, size_t size) = {
	backend_noop_open,
	backend_persistent_open
};

void (*backend_close_by_type[MAX_BACKEND])
	(struct backend *b) = {
	backend_noop_close,
	backend_persistent_close
};

/*
 * backend_open -- opens a backend of desired type
 */
struct backend *
backend_open(enum backend_type type, void *ptr, size_t size)
{
	return backend_open_by_type[type](ptr, size);
}

/*
 * backend_close -- closes a backend based on its type
 */
void
backend_close(struct backend *backend)
{
	backend_close_by_type[backend->type](backend);
}

/*
 * backend_init -- initializes a backend
 */
void
backend_init(struct backend *backend, enum backend_type type,
	struct bucket_backend_operations *b_ops,
	struct arena_backend_operations *a_ops,
	struct pool_backend_operations *p_ops)
{
	backend->type = type;

	backend->b_ops = b_ops;
	backend->a_ops = a_ops;
	backend->p_ops = p_ops;
}
