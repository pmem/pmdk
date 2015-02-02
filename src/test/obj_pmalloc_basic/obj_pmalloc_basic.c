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
 * obj_pmalloc_basic.c -- unit test for pmemobj pmalloc/pfree/prealloc
 * it mocks all the other allocator functions/structures to test
 * just the interface.
 */
#include <stdbool.h>
#include <assert.h>
#include "unittest.h"
#include "pmalloc.h"
#include "bucket.h"
#include "arena.h"
#include "backend.h"
#include "pool.h"
#include "noop_backend.h"
#include "util.h"

#define	TEST_ALLOC_SIZE 1024
#define	TEST_BUCKET_UNITS 10
#define	TEST_DATA_OFFSET 100

uint64_t test_ptr;

struct bucket mock_bucket;
struct arena mock_arena;

void
mock_set_alloc_ptr(struct arena *arena, uint64_t *ptr, uint64_t value)
{
	*ptr = value;
	ASSERT(value == TEST_DATA_OFFSET || value == NULL_OFFSET);
	ASSERT(ptr == &test_ptr);
}

struct arena_backend_operations mock_arena_ops = {
	mock_set_alloc_ptr
};

struct bucket_object mock_obj = {
	.real_size = TEST_ALLOC_SIZE,
	.data_offset = TEST_DATA_OFFSET
};

FUNC_WRAP_BEGIN(bucket_object_init, void, struct bucket_object *obj,
	struct pmalloc_pool *p, uint64_t ptr)
	obj->real_size = TEST_ALLOC_SIZE;
	obj->data_offset = TEST_DATA_OFFSET;
FUNC_WRAP_END_NO_RET

FUNC_WILL_RETURN(arena_guard_up, true)
FUNC_WILL_RETURN(arena_guard_down, true)
FUNC_WILL_RETURN(bucket_calc_units, TEST_BUCKET_UNITS)
FUNC_WILL_RETURN(bucket_remove_object, true)

FUNC_WILL_RETURN(pool_recycle_object, true)

FUNC_WRAP_BEGIN(arena_select_bucket, void *, struct arena *arena, size_t size)
FUNC_WRAP_ARG_EQ(arena, &mock_arena)
FUNC_WRAP_END(&mock_bucket)

FUNC_WRAP_BEGIN(bucket_find_object, void *, struct bucket *bucket,
	uint64_t units)
FUNC_WRAP_ARG_EQ(bucket, &mock_bucket)
FUNC_WRAP_ARG_EQ(units, TEST_BUCKET_UNITS)
FUNC_WRAP_END(&mock_obj)

FUNC_WRAP_BEGIN(pool_select_arena, void *, struct pmalloc_pool *p)
FUNC_WRAP_ARG_NE(p->lock, NULL)
FUNC_WRAP_ARG_EQ(p->backend->type, BACKEND_NOOP)
FUNC_WRAP_ARG_EQ(p->backend->a_ops->set_alloc_ptr, noop_set_alloc_ptr)
FUNC_WRAP_END(&mock_arena)

#define	TEST_POOL_SIZE 1024 * 1024 * 40 /* 40MB */

void
test_flow()
{
	mock_arena.a_ops = &mock_arena_ops;

	struct pmalloc_pool *p = pool_open_noop(MALLOC(TEST_POOL_SIZE),
		TEST_POOL_SIZE);

	pmalloc(p, &test_ptr, TEST_ALLOC_SIZE);
	ASSERT(test_ptr == TEST_DATA_OFFSET);

	prealloc(p, &test_ptr, TEST_ALLOC_SIZE*2);

	pfree(p, &test_ptr);
	ASSERT(test_ptr == NULL_OFFSET);

	pool_close(p);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pmalloc_basic");

	test_flow();

	DONE(NULL);
}
