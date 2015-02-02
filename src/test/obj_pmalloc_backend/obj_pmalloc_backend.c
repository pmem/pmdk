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
 * obj_pmalloc_backend.c -- unit test for pmalloc backends
 */
#include <stdbool.h>
#include <assert.h>
#include "unittest.h"
#include "pmalloc.h"
#include "bucket.h"
#include "arena.h"
#include "backend.h"
#include "pool.h"
#include "persistent_backend.h"
#include "util.h"

#define	MOCK_BUCKET_OPS ((void *)0xABC)
#define	MOCK_ARENA_OPS ((void *)0xBCD)
#define	MOCK_POOL_OPS ((void *)0xCDE)

void
test_backend()
{
	struct backend mock_backend;
	backend_open(&mock_backend, BACKEND_PERSISTENT, MOCK_BUCKET_OPS,
		MOCK_ARENA_OPS, MOCK_POOL_OPS);

	ASSERT(mock_backend.type == BACKEND_PERSISTENT);
	ASSERT(mock_backend.b_ops == MOCK_BUCKET_OPS);
	ASSERT(mock_backend.a_ops == MOCK_ARENA_OPS);
	ASSERT(mock_backend.p_ops == MOCK_POOL_OPS);
}

#define	MOCK_PTR NULL
#define	MOCK_PTR_SIZE 0

void
test_backend_persistent()
{
	struct backend *mock_backend =
		persistent_backend_open(MOCK_PTR, MOCK_PTR_SIZE);

	ASSERT(mock_backend != NULL);
	ASSERT(mock_backend->type == BACKEND_PERSISTENT);
	ASSERT(mock_backend->a_ops->set_alloc_ptr ==
		persistent_set_alloc_ptr);

	persistent_backend_close(mock_backend);
}

#define	TEST_VAL_A 5
#define	TEST_VAL_B 10
uint64_t val = TEST_VAL_A;

bool mock_persist_called = false;

void
mock_persist(void *addr, size_t len)
{
	uint64_t *p_val = addr;
	ASSERT(p_val == &val);
	ASSERT(*p_val == TEST_VAL_B);
	mock_persist_called = true;
}

void
test_backend_persistent_set_ptr()
{
	struct persistent_backend mock_backend = {
		.persist = mock_persist
	};

	struct pmalloc_pool mock_pool = {
		.backend = (struct backend *)&mock_backend
	};

	struct arena mock_arena = {
		.pool = &mock_pool
	};

	persistent_set_alloc_ptr(&mock_arena, &val, TEST_VAL_B);
	ASSERT(val == TEST_VAL_B);
	ASSERT(mock_persist_called);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pmalloc_backend");

	test_backend();
	test_backend_persistent();
	test_backend_persistent_set_ptr();
	test_backend_persistent_set_ptr();

	DONE(NULL);
}
