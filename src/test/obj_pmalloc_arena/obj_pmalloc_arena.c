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
 * obj_pmalloc_arena.c -- unit test for pmalloc arenas
 */
#include <stdbool.h>
#include <assert.h>
#include <pthread.h>
#include "unittest.h"
#include "pmalloc.h"
#include "bucket.h"
#include "arena.h"
#include "backend.h"
#include "pool.h"
#include "util.h"

FUNC_REAL_DECL(arena_new, struct arena *, struct pmalloc_pool *p, int arena_id)

#define	MOCK_ARENA_OPS ((struct arena_backend_operations *)0xABC)
#define	MOCK_ARENA_ID 1

FUNC_WILL_RETURN(pthread_mutex_init, 0)
FUNC_WILL_RETURN(pthread_mutex_destroy, 0)

void
arena_test_create_delete()
{
	struct backend mock_backend = {
		.a_ops = MOCK_ARENA_OPS
	};

	struct pmalloc_pool mock_pool = {
		.backend = &mock_backend
	};

	struct arena *a = FUNC_REAL(arena_new)(&mock_pool, MOCK_ARENA_ID);
	ASSERT(a != NULL);
	ASSERT(a->id == MOCK_ARENA_ID);
	ASSERT(a->pool == &mock_pool);
	ASSERT(a->a_ops == MOCK_ARENA_OPS);
	for (int i = 0; i < MAX_BUCKETS; ++i) {
		ASSERT(a->buckets[i] == NULL);
	}

	arena_delete(a);
}

#define	MOCK_ARENA_LOCK ((pthread_mutex_t *)0xBCD)

FUNC_WILL_RETURN(pthread_mutex_lock, 0)
FUNC_WILL_RETURN(pthread_mutex_unlock, 0)

void
arena_test_guards()
{
	struct arena mock_arena = {
		.lock = MOCK_ARENA_LOCK
	};
	arena_guard_up(&mock_arena, NULL, GUARD_TYPE_MALLOC);
	arena_guard_down(&mock_arena, NULL, GUARD_TYPE_MALLOC);
}

struct arena mock_arena_0 = {
	.associated_threads = 0
};

struct arena mock_arena_1 = {
	.associated_threads = 0
};

FUNC_WILL_RETURN(arena_new, &mock_arena_1);

void
arena_test_select()
{
	struct pmalloc_pool mock_pool = {
		.arenas = {&mock_arena_0, NULL}
	};
	struct arena *a = pool_select_arena(&mock_pool);
	ASSERT(a == &mock_arena_1);
	a = pool_select_arena(&mock_pool);
	ASSERT(a == &mock_arena_1);
	ASSERT(mock_pool.arenas[0] == &mock_arena_0);
	ASSERT(mock_pool.arenas[1] == &mock_arena_1);
	for (int i = 2; i < MAX_ARENAS; ++i) {
		ASSERT(mock_pool.arenas[i] == NULL);
	}
}

#define	MOCK_BUCKET_PTR ((void *)0xABC)
#define	ALLOC_TEST_SIZE 1024

FUNC_WILL_RETURN(get_bucket_class_id_by_size, 0)
FUNC_WILL_RETURN(bucket_new, MOCK_BUCKET_PTR)

void
arena_test_select_bucket()
{
	struct arena mock_arena = {
		.buckets = { NULL }
	};
	struct bucket *b = arena_select_bucket(&mock_arena, ALLOC_TEST_SIZE);
	ASSERT(b == MOCK_BUCKET_PTR);
	ASSERT(mock_arena.buckets[0] == MOCK_BUCKET_PTR);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pmalloc_arena");
	arena_test_create_delete();
	arena_test_guards();
	arena_test_select();
	arena_test_select_bucket();

	DONE(NULL);
}
