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
 * obj_pmalloc_bucket.c -- unit test for pmalloc buckets
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

void test_bucket_register_class() {
	struct bucket_class mock_class_0 = {
		.unit_size = 4
	};

	struct bucket_class mock_class_1 = {
		.unit_size = 8
	};

	struct pmalloc_pool mock_pool = {
		.bucket_classes = {{0}}
	};
	ASSERT(0 == bucket_register_class(&mock_pool, mock_class_0));
	ASSERT(1 == bucket_register_class(&mock_pool, mock_class_1));

	ASSERT(bucket_unregister_class(&mock_pool, 0));
	ASSERT(bucket_unregister_class(&mock_pool, 1));
}

#define	MOCK_BUCKET_OPS ((void *)0xABC)
#define	MOCK_BUCKET_UNIT_SIZE 1

void test_bucket_create_delete() {
	struct bucket_class mock_class_0 = {
		.unit_size = MOCK_BUCKET_UNIT_SIZE
	};
	struct backend mock_backend = {
		.b_ops = MOCK_BUCKET_OPS
	};

	struct pmalloc_pool mock_pool = {
		.backend = &mock_backend,
		.bucket_classes = {{0}}
	};
	int class_id = bucket_register_class(&mock_pool, mock_class_0);
	struct bucket *b = bucket_new(&mock_pool, class_id);
	ASSERT(b != NULL);
	ASSERT(b->pool == &mock_pool);
	ASSERT(b->b_ops == MOCK_BUCKET_OPS);
	ASSERT(b->class.unit_size == MOCK_BUCKET_UNIT_SIZE);
	bucket_delete(b);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pmalloc_bucket");

	test_bucket_register_class();
	test_bucket_create_delete();

	DONE(NULL);
}
