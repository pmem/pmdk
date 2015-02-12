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
 * obj_pmalloc_pool.c -- unit test for pmalloc pools
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
#include "backend_noop.h"
#include "util.h"

struct pool_backend_operations mock_pool_ops;

struct backend mock_backend = {
	.type = BACKEND_NOOP,
	.p_ops = &mock_pool_ops
};

FUNC_WILL_RETURN(pthread_mutex_init, 0);
FUNC_WILL_RETURN(pthread_mutex_destroy, 0);
FUNC_WILL_RETURN(backend_noop_open, &mock_backend)
FUNC_WILL_RETURN(backend_noop_close, NULL);

void
pool_test_create_delete()
{
	struct pmalloc_pool *p = pool_new(NULL, 0, BACKEND_NOOP);
	ASSERT(p != NULL);
	ASSERT(p->backend == &mock_backend);
	ASSERT(p->p_ops == &mock_pool_ops);
	pool_delete(p);
}

struct bucket mock_bucket;
struct bucket_object mock_object;

FUNC_WILL_RETURN(get_bucket_class_id_by_size, 0);
FUNC_WILL_RETURN(bucket_new, &mock_bucket)

FUNC_WRAP_BEGIN(bucket_add_object, bool, struct bucket *bucket,
	struct bucket_object *obj)
FUNC_WRAP_ARG_EQ(bucket, &mock_bucket)
FUNC_WRAP_ARG_EQ(obj, &mock_object)
FUNC_WRAP_END(true)

void
pool_test_recycle_object()
{
	struct pmalloc_pool mock_pool = {
		.buckets = {NULL}
	};
	assert(pool_recycle_object(&mock_pool, &mock_object));
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pmalloc_pool");

	pool_test_create_delete();
	pool_test_recycle_object();

	DONE(NULL);
}
