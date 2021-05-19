// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2021, Intel Corporation */

/*
 * obj_bucket.c -- unit test for bucket
 */
#include "bucket.h"
#include "container_ravl.h"
#include "util.h"
#include "unittest.h"
#include "obj.h"

#define TEST_CHUNK_ID	10
#define TEST_ZONE_ID	20
#define TEST_SIZE_IDX	30
#define TEST_BLOCK_OFF	40

struct container_test {
	struct block_container super;
};

static const struct memory_block *inserted_memblock;

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
	.is_empty = NULL,
	.rm_all = NULL,
	.destroy = container_test_destroy,
};

static struct block_container *
container_new_test(void)
{
	struct container_test *c = MALLOC(sizeof(struct container_test));
	c->super.c_ops = &container_test_ops;
	return &c->super;
}

static void *
mock_get_real_data(const struct memory_block *m)
{
	return NULL;
}

static size_t
mock_get_real_size(const struct memory_block *m)
{
	return 0;
}

static const struct memory_block_ops mock_ops = {
	.block_size = NULL,
	.prep_hdr = NULL,
	.get_lock = NULL,
	.get_state = NULL,
	.get_user_data = NULL,
	.get_real_data = mock_get_real_data,
	.get_user_size = NULL,
	.get_real_size = mock_get_real_size,
	.write_header = NULL,
	.reinit_header = NULL,
	.get_extra = NULL,
	.get_flags = NULL,
};

static void
test_fault_injection()
{
	if (!pmemobj_fault_injection_enabled())
		return;

	pmemobj_inject_fault_at(PMEM_MALLOC, 1, "bucket_locked_new");

	struct bucket_locked *b = bucket_locked_new(container_new_test(), NULL);
	UT_ASSERTeq(b, NULL);
	UT_ASSERTeq(errno, ENOMEM);
}

static void
test_bucket_insert_get(void)
{
	struct bucket_locked *locked =
		bucket_locked_new(container_new_test(), NULL);
	UT_ASSERT(locked != NULL);

	struct bucket *b = bucket_acquire(locked);

	struct memory_block m = {TEST_CHUNK_ID, TEST_ZONE_ID,
		TEST_SIZE_IDX, TEST_BLOCK_OFF};
	m.m_ops = &mock_ops;

	/* get from empty */

	UT_ASSERT(bucket_remove_block(b, &m) != 0);

	UT_ASSERT(bucket_insert_block(b, &m) == 0);

	UT_ASSERT(bucket_remove_block(b, &m) == 0);

	UT_ASSERT(m.chunk_id == TEST_CHUNK_ID);
	UT_ASSERT(m.zone_id == TEST_ZONE_ID);
	UT_ASSERT(m.size_idx == TEST_SIZE_IDX);
	UT_ASSERT(m.block_off == TEST_BLOCK_OFF);

	bucket_release(b);
	bucket_locked_delete(locked);
}

static void
test_bucket_remove(void)
{
	struct bucket_locked *locked =
		bucket_locked_new(container_new_test(), NULL);
	UT_ASSERT(locked != NULL);

	struct bucket *b = bucket_acquire(locked);

	struct memory_block m = {TEST_CHUNK_ID, TEST_ZONE_ID,
		TEST_SIZE_IDX, TEST_BLOCK_OFF};
	m.m_ops = &mock_ops;

	UT_ASSERT(bucket_insert_block(b, &m) == 0);

	UT_ASSERT(bucket_remove_block(b, &m) == 0);

	bucket_release(b);
	bucket_locked_delete(locked);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_bucket");

	test_bucket_insert_get();
	test_bucket_remove();
	test_fault_injection();

	DONE(NULL);
}

#ifdef _MSC_VER
/*
 * Since libpmemobj is linked statically, we need to invoke its ctor/dtor.
 */
MSVC_CONSTR(libpmemobj_init)
MSVC_DESTR(libpmemobj_fini)
#endif
