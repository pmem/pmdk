/*
 * Copyright 2015-2016, Intel Corporation
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
#include "unittest.h"
#include "redo.h"
#include "memops.h"
#include "heap_layout.h"
#include "memblock.h"
#include "heap.h"
#include "bucket.h"

#define TEST_UNIT_SIZE 128
#define TEST_MAX_UNIT 1

#define TEST_SIZE 5
#define TEST_SIZE_UNITS 1

#define MOCK_CRIT	((void *)0xABC)

#define TEST_CHUNK_ID	10
#define TEST_ZONE_ID	20
#define TEST_SIZE_IDX	30
#define TEST_BLOCK_OFF	40

FUNC_MOCK(malloc, void *, size_t size)
	FUNC_MOCK_RUN_RET_DEFAULT_REAL(malloc, size)
	FUNC_MOCK_RUN(4) { /* +4 because of allocs for init, b malloc */
		return NULL;
	}
FUNC_MOCK_END

FUNC_MOCK(ctree_new, struct ctree *, void)
	FUNC_MOCK_RUN_RET_DEFAULT(MOCK_CRIT)
	FUNC_MOCK_RUN(1) { /* +1 because of ctree new in init */
		return NULL;
	}
FUNC_MOCK_END

FUNC_MOCK_RET_ALWAYS(ctree_delete, void *, NULL, struct ctree *t);

static uint64_t inserted_key;

FUNC_MOCK(ctree_insert, int, struct ctree *c, uint64_t key)
	FUNC_MOCK_RUN_DEFAULT {
		inserted_key = key;
		return 0;
	}
FUNC_MOCK_END

FUNC_MOCK(ctree_remove, uint64_t, struct ctree *c, uint64_t key, int eq)
	FUNC_MOCK_RUN_DEFAULT {
		return inserted_key;
	}
	FUNC_MOCK_RUN(0) {
		return 0;
	}
FUNC_MOCK_END

static void
test_new_delete_bucket()
{
	struct bucket *b = NULL;

	/* b malloc fail */
	b = bucket_new(1, BUCKET_HUGE, CONTAINER_CTREE, 1, 1);
	UT_ASSERT(b == NULL);

	/* b->ctree fail */
	b = bucket_new(2, BUCKET_HUGE, CONTAINER_CTREE, 1, 1);
	UT_ASSERT(b == NULL);

	/* all ok */
	b = bucket_new(4, BUCKET_HUGE, CONTAINER_CTREE, 1, 1);
	UT_ASSERT(b != NULL);

	bucket_delete(b);
}

static void
test_bucket_bitmap_correctness()
{
	struct bucket *b = bucket_new(1, BUCKET_RUN, CONTAINER_CTREE,
		(RUNSIZE / 10), TEST_MAX_UNIT);
	UT_ASSERT(b != NULL);

	/* 54 set (not available for allocations), and 10 clear (available) */
	uint64_t bitmap_lastval =
	0b1111111111111111111111111111111111111111111111111111110000000000;

	struct bucket_run *r = (struct bucket_run *)b;
	UT_ASSERTeq(r->bitmap_lastval, bitmap_lastval);

	bucket_delete(b);
}

static void
test_bucket()
{
	struct bucket *b = bucket_new(1, BUCKET_HUGE, CONTAINER_CTREE,
		TEST_UNIT_SIZE, TEST_MAX_UNIT);
	UT_ASSERT(b != NULL);

	UT_ASSERT(b->unit_size == TEST_UNIT_SIZE);
	UT_ASSERT(b->type == BUCKET_HUGE);
	UT_ASSERT(b->calc_units(b, TEST_SIZE) == TEST_SIZE_UNITS);

	bucket_delete(b);
}

static void
test_bucket_insert_get()
{
	struct bucket *b = bucket_new(1, BUCKET_RUN, CONTAINER_CTREE,
		TEST_UNIT_SIZE, TEST_MAX_UNIT);
	UT_ASSERT(b != NULL);

	struct memory_block m = {TEST_CHUNK_ID, TEST_ZONE_ID,
		TEST_SIZE_IDX, TEST_BLOCK_OFF};

	/* get from empty */
	UT_ASSERT(CNT_OP(b, get_rm_bestfit, &m) != 0);

	UT_ASSERT(CNT_OP(b, insert, NULL, m) == 0);

	UT_ASSERT(CNT_OP(b, get_rm_bestfit, &m) == 0);

	UT_ASSERT(m.chunk_id == TEST_CHUNK_ID);
	UT_ASSERT(m.zone_id == TEST_ZONE_ID);
	UT_ASSERT(m.size_idx == TEST_SIZE_IDX);
	UT_ASSERT(m.block_off == TEST_BLOCK_OFF);

	bucket_delete(b);
}

static void
test_bucket_remove()
{
	struct bucket *b = bucket_new(1, BUCKET_RUN, CONTAINER_CTREE,
		TEST_UNIT_SIZE, TEST_MAX_UNIT);
	UT_ASSERT(b != NULL);

	struct memory_block m = {TEST_CHUNK_ID, TEST_ZONE_ID,
		TEST_SIZE_IDX, TEST_BLOCK_OFF};

	UT_ASSERT(CNT_OP(b, insert, NULL, m) == 0);

	UT_ASSERT(CNT_OP(b, get_rm_exact, m) == 0);

	bucket_delete(b);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_bucket");

	test_new_delete_bucket();
	test_bucket();
	test_bucket_insert_get();
	test_bucket_remove();
	test_bucket_bitmap_correctness();

	DONE(NULL);
}
