/*
 * Copyright 2015-2017, Intel Corporation
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
 * obj_bucket.c -- unit test for bucket
 */
#include "bucket.h"
#include "container_ctree.h"
#include "util.h"
#include "unittest.h"

#define TEST_CHUNK_ID	10
#define TEST_ZONE_ID	20
#define TEST_SIZE_IDX	30
#define TEST_BLOCK_OFF	40

struct container_test {
	struct block_container super;
};

const struct memory_block *inserted_memblock;

static int
container_test_insert(struct block_container *c,
	const struct memory_block *m)
{
	inserted_memblock = m;
	return 0;
}

static int
container_test_get_rm_bestfit(struct block_container *c,
	struct memory_block *m)
{
	if (inserted_memblock == NULL)
		return ENOMEM;

	*m = *inserted_memblock;
	inserted_memblock = NULL;
	return 0;
}

static int
container_test_get_rm_exact(struct block_container *c,
	const struct memory_block *m)
{
	if (inserted_memblock == NULL)
		return ENOMEM;

	if (inserted_memblock->chunk_id == m->chunk_id) {
		inserted_memblock = NULL;
		return 0;
	}

	return ENOMEM;
}

static void
container_test_destroy(struct block_container *c)
{
	FREE(c);
}

static struct block_container_ops container_test_ops = {
	.insert = container_test_insert,
	.get_rm_exact = container_test_get_rm_exact,
	.get_rm_bestfit = container_test_get_rm_bestfit,
	.get_exact = NULL,
	.is_empty = NULL,
	.rm_all = NULL,
	.destroy = container_test_destroy,
};

static struct block_container *
container_new_test()
{
	struct container_test *c = MALLOC(sizeof(struct container_test));
	c->super.c_ops = &container_test_ops;
	return &c->super;
}

static void
test_bucket_insert_get()
{
	struct bucket *b = bucket_new(container_new_test(), NULL);
	UT_ASSERT(b != NULL);

	struct memory_block m = {TEST_CHUNK_ID, TEST_ZONE_ID,
		TEST_SIZE_IDX, TEST_BLOCK_OFF};

	/* get from empty */

	UT_ASSERT(b->c_ops->get_rm_bestfit(b->container, &m) != 0);

	UT_ASSERT(bucket_insert_block(b, &m) == 0);

	UT_ASSERT(b->c_ops->get_rm_bestfit(b->container, &m) == 0);

	UT_ASSERT(m.chunk_id == TEST_CHUNK_ID);
	UT_ASSERT(m.zone_id == TEST_ZONE_ID);
	UT_ASSERT(m.size_idx == TEST_SIZE_IDX);
	UT_ASSERT(m.block_off == TEST_BLOCK_OFF);

	bucket_delete(b);
}

static void
test_bucket_remove()
{
	struct bucket *b = bucket_new(container_new_test(), NULL);
	UT_ASSERT(b != NULL);

	struct memory_block m = {TEST_CHUNK_ID, TEST_ZONE_ID,
		TEST_SIZE_IDX, TEST_BLOCK_OFF};

	UT_ASSERT(bucket_insert_block(b, &m) == 0);

	UT_ASSERT(b->c_ops->get_rm_exact(b->container, &m) == 0);

	bucket_delete(b);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_bucket");

	test_bucket_insert_get();
	test_bucket_remove();

	DONE(NULL);
}
