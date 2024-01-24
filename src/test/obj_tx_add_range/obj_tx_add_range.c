// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2024, Intel Corporation */

/*
 * obj_tx_add_range.c -- unit test for pmemobj_tx_add_range
 */
#include <string.h>
#include <stddef.h>

#include "tx.h"
#include "unittest.h"
#include "util.h"
#include "valgrind_internal.h"

#define LAYOUT_NAME "tx_add_range"

#define OBJ_SIZE	1024
#define OVERLAP_SIZE	100
#define ROOT_TAB_SIZE\
	(TX_DEFAULT_RANGE_CACHE_SIZE / sizeof(int))

#define REOPEN_COUNT	10

enum type_number {
	TYPE_OBJ,
	TYPE_OBJ_ABORT,
	TYPE_OBJ_WRONG_UUID,
};

TOID_DECLARE(struct object, 0);
TOID_DECLARE(struct overlap_object, 1);
TOID_DECLARE_ROOT(struct root);

struct root {
	int val;
	int tab[ROOT_TAB_SIZE];
};

struct object {
	size_t value;
	char data[OBJ_SIZE - sizeof(size_t)];
};

struct overlap_object {
	uint8_t data[OVERLAP_SIZE];
};

#define VALUE_OFF	(offsetof(struct object, value))
#define VALUE_SIZE	(sizeof(size_t))
#define DATA_OFF	(offsetof(struct object, data))
#define DATA_SIZE	(OBJ_SIZE - sizeof(size_t))
#define TEST_VALUE_1	1
#define TEST_VALUE_2	2

/*
 * do_tx_zalloc -- do tx allocation with specified type number
 */
static PMEMoid
do_tx_zalloc(PMEMobjpool *pop, uint64_t type_num)
{
	PMEMoid ret = OID_NULL;

	TX_BEGIN(pop) {
		ret = pmemobj_tx_zalloc(sizeof(struct object), type_num);
	} TX_END

	return ret;
}

/*
 * do_tx_alloc -- do tx allocation and initialize first num bytes
 */
static PMEMoid
do_tx_alloc(PMEMobjpool *pop, uint64_t type_num, uint64_t init_num)
{
	PMEMoid ret = OID_NULL;

	TX_BEGIN(pop) {
		ret = pmemobj_tx_alloc(sizeof(struct object), type_num);
		pmemobj_memset(pop, pmemobj_direct(ret), 0,
			sizeof(struct object), 0);
	} TX_END

	return ret;
}

/*
 * do_tx_add_range_alloc_commit -- call pmemobj_add_range on object allocated
 * within the same transaction and commit the transaction
 */
static void
do_tx_add_range_alloc_commit(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));
		UT_ASSERT(!TOID_IS_NULL(obj));

		ret = pmemobj_tx_add_range(obj.oid, VALUE_OFF, VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;

		ret = pmemobj_tx_add_range(obj.oid, DATA_OFF, DATA_SIZE);
		UT_ASSERTeq(ret, 0);

		pmemobj_memset_persist(pop, D_RW(obj)->data, TEST_VALUE_2,
			DATA_SIZE);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);

	size_t i;
	for (i = 0; i < DATA_SIZE; i++)
		UT_ASSERTeq(D_RO(obj)->data[i], TEST_VALUE_2);
}

/*
 * do_tx_add_range_alloc_abort -- call pmemobj_add_range on object allocated
 * within the same transaction and abort the transaction
 */
static void
do_tx_add_range_alloc_abort(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ_ABORT));
		UT_ASSERT(!TOID_IS_NULL(obj));

		ret = pmemobj_tx_add_range(obj.oid, VALUE_OFF, VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;

		ret = pmemobj_tx_add_range(obj.oid, DATA_OFF, DATA_SIZE);
		UT_ASSERTeq(ret, 0);

		pmemobj_memset_persist(pop, D_RW(obj)->data, TEST_VALUE_2,
			DATA_SIZE);

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_OBJ_ABORT));
	UT_ASSERT(TOID_IS_NULL(obj));
}

/*
 * do_tx_add_range_twice_commit -- call pmemobj_add_range one the same area
 * twice and commit the transaction
 */
static void
do_tx_add_range_twice_commit(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;

	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));
	UT_ASSERT(!TOID_IS_NULL(obj));

	TX_BEGIN(pop) {
		ret = pmemobj_tx_add_range(obj.oid, VALUE_OFF, VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;

		ret = pmemobj_tx_add_range(obj.oid, VALUE_OFF, VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_2;
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_2);
}

/*
 * do_tx_add_range_twice_abort -- call pmemobj_add_range one the same area
 * twice and abort the transaction
 */
static void
do_tx_add_range_twice_abort(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;

	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));
	UT_ASSERT(!TOID_IS_NULL(obj));

	TX_BEGIN(pop) {
		ret = pmemobj_tx_add_range(obj.oid, VALUE_OFF, VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;

		ret = pmemobj_tx_add_range(obj.oid, VALUE_OFF, VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_2;

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, 0);
}

/*
 * do_tx_add_range_abort_after_nested -- call pmemobj_tx_add_range and
 * commit the tx
 */
static void
do_tx_add_range_abort_after_nested(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj1;
	TOID(struct object) obj2;
	TOID_ASSIGN(obj1, do_tx_zalloc(pop, TYPE_OBJ));
	TOID_ASSIGN(obj2, do_tx_zalloc(pop, TYPE_OBJ));

	TX_BEGIN(pop) {
		ret = pmemobj_tx_add_range(obj1.oid, VALUE_OFF, VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj1)->value = TEST_VALUE_1;

		TX_BEGIN(pop) {
			ret = pmemobj_tx_add_range(obj2.oid,
					DATA_OFF, DATA_SIZE);
			UT_ASSERTeq(ret, 0);

			pmemobj_memset_persist(pop, D_RW(obj2)->data,
				TEST_VALUE_2, DATA_SIZE);
		} TX_ONABORT {
			UT_ASSERT(0);
		} TX_END

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj1)->value, 0);

	size_t i;
	for (i = 0; i < DATA_SIZE; i++)
		UT_ASSERTeq(D_RO(obj2)->data[i], 0);
}

/*
 * do_tx_add_range_abort_nested -- call pmemobj_tx_add_range and
 * commit the tx
 */
static void
do_tx_add_range_abort_nested(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj1;
	TOID(struct object) obj2;
	TOID_ASSIGN(obj1, do_tx_zalloc(pop, TYPE_OBJ));
	TOID_ASSIGN(obj2, do_tx_zalloc(pop, TYPE_OBJ));

	TX_BEGIN(pop) {
		ret = pmemobj_tx_add_range(obj1.oid, VALUE_OFF, VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj1)->value = TEST_VALUE_1;

		TX_BEGIN(pop) {
			ret = pmemobj_tx_add_range(obj2.oid,
					DATA_OFF, DATA_SIZE);
			UT_ASSERTeq(ret, 0);

			pmemobj_memset_persist(pop, D_RW(obj2)->data,
				TEST_VALUE_2, DATA_SIZE);

			pmemobj_tx_abort(-1);
		} TX_ONCOMMIT {
			UT_ASSERT(0);
		} TX_END
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj1)->value, 0);

	size_t i;
	for (i = 0; i < DATA_SIZE; i++)
		UT_ASSERTeq(D_RO(obj2)->data[i], 0);
}

/*
 * do_tx_add_range_abort_after_commit -- call pmemobj_tx_add_range with
 * non-zero data, commit first tx, and abort second tx
 *
 * This is the test for issue injected in commit:
 * 2ab13304664b353b82730f49b78fc67eea33b25b (ulog-invalidation).
 */
static void
do_tx_add_range_abort_after_commit(PMEMobjpool *pop)
{
	int ret;
	size_t i;
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));

	/* 1. Set data to non-zero value. */
	pmemobj_memset_persist(pop, D_RW(obj)->data,
		TEST_VALUE_1, DATA_SIZE);
	for (i = 0; i < DATA_SIZE; i++)
		UT_ASSERTeq(D_RO(obj)->data[i], TEST_VALUE_1);

	/* 2. Do the snapshot using non-zero value. */
	TX_BEGIN(pop) {
		ret = pmemobj_tx_add_range(obj.oid,
				DATA_OFF, DATA_SIZE);
		UT_ASSERTeq(ret, 0);
		/*
		 * You can modify data here, but it is not necessary
		 * to reproduce abort/apply ulog issue.
		 */
		pmemobj_memset_persist(pop, D_RW(obj)->data,
			TEST_VALUE_2, DATA_SIZE);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	for (i = 0; i < DATA_SIZE; i++)
		UT_ASSERTeq(D_RO(obj)->data[i], TEST_VALUE_2);

	/*
	 * 3. Do the second snapshot and then abort the transaction.
	 */
	for (i = 0; i < DATA_SIZE; i++)
		UT_ASSERTeq(D_RO(obj)->data[i], TEST_VALUE_2);

	TX_BEGIN(pop) {
		ret = pmemobj_tx_add_range(obj.oid, VALUE_OFF, VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	/* 4. All data must be recovered after tx abort. */
	UT_ASSERTeq(D_RO(obj)->value, 0);
}

/*
 * do_tx_add_range_commit_nested -- call pmemobj_tx_add_range and commit the tx
 */
static void
do_tx_add_range_commit_nested(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj1;
	TOID(struct object) obj2;
	TOID_ASSIGN(obj1, do_tx_zalloc(pop, TYPE_OBJ));
	TOID_ASSIGN(obj2, do_tx_zalloc(pop, TYPE_OBJ));

	TX_BEGIN(pop) {
		ret = pmemobj_tx_add_range(obj1.oid, VALUE_OFF, VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj1)->value = TEST_VALUE_1;

		TX_BEGIN(pop) {
			ret = pmemobj_tx_add_range(obj2.oid,
					DATA_OFF, DATA_SIZE);
			UT_ASSERTeq(ret, 0);

			pmemobj_memset_persist(pop, D_RW(obj2)->data,
				TEST_VALUE_2, DATA_SIZE);
		} TX_ONABORT {
			UT_ASSERT(0);
		} TX_END
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj1)->value, TEST_VALUE_1);

	size_t i;
	for (i = 0; i < DATA_SIZE; i++)
		UT_ASSERTeq(D_RO(obj2)->data[i], TEST_VALUE_2);
}

/*
 * do_tx_add_range_abort -- call pmemobj_tx_add_range and abort the tx
 */
static void
do_tx_add_range_abort(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));

	TX_BEGIN(pop) {
		ret = pmemobj_tx_add_range(obj.oid, VALUE_OFF, VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, 0);
}

/*
 * do_tx_add_huge_range_abort -- call pmemobj_tx_add_range on a huge range and
 * commit the tx
 */
static void
do_tx_add_huge_range_abort(PMEMobjpool *pop)
{
	int ret;
	size_t snapshot_s = TX_DEFAULT_RANGE_CACHE_THRESHOLD + 1;

	PMEMoid obj;
	pmemobj_zalloc(pop, &obj, snapshot_s, 0);

	TX_BEGIN(pop) {
		ret = pmemobj_tx_add_range(obj, 0, snapshot_s);
		UT_ASSERTeq(ret, 0);
		memset(pmemobj_direct(obj), 0xc, snapshot_s);
		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERT(util_is_zeroed(pmemobj_direct(obj), snapshot_s));
}

/*
 * do_tx_add_range_commit -- call pmemobj_tx_add_range and commit the tx
 */
static void
do_tx_add_range_commit(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));

	TX_BEGIN(pop) {
		ret = pmemobj_tx_add_range(obj.oid, VALUE_OFF, VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
}

/*
 * do_tx_xadd_range_no_flush_commit -- call pmemobj_tx_xadd_range with
 * POBJ_XADD_NO_FLUSH set and commit the tx
 */
static void
do_tx_xadd_range_no_flush_commit(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));

	TX_BEGIN(pop) {
		ret = pmemobj_tx_xadd_range(obj.oid, VALUE_OFF, VALUE_SIZE,
				POBJ_XADD_NO_FLUSH);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;
		/* let pmemcheck find we didn't flush it */
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
}

/*
 * do_tx_xadd_range_no_snapshot_commit -- call pmemobj_tx_xadd_range with
 * POBJ_XADD_NO_SNAPSHOT flag set and commit the tx
 */
static void
do_tx_xadd_range_no_snapshot_commit(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));

	TX_BEGIN(pop) {
		ret = pmemobj_tx_xadd_range(obj.oid, VALUE_OFF, VALUE_SIZE,
				POBJ_XADD_NO_SNAPSHOT);
		UT_ASSERTeq(ret, 0);
		D_RW(obj)->value = TEST_VALUE_1;
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
}

/*
 * do_tx_xadd_range_twice_no_snapshot_abort -- call pmemobj_tx_add_range twice
 * - with POBJ_XADD_NO_SNAPSHOT flag set and without it - and abort the tx
 */
static void
do_tx_xadd_range_twice_no_snapshot_abort(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));

	TX_BEGIN(pop) {
		ret = pmemobj_tx_xadd_range(obj.oid, VALUE_OFF, VALUE_SIZE,
				POBJ_XADD_NO_SNAPSHOT);
		UT_ASSERTeq(ret, 0);

		/* Previously set flag on this range should NOT be overridden */
		ret = pmemobj_tx_add_range(obj.oid, VALUE_OFF, VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;
		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
}

/*
 * do_tx_xadd_range_no_snapshot_abort -- call pmemobj_tx_range with
 * POBJ_XADD_NO_SNAPSHOT flag, modify the value inside aborted transaction
 */
static void
do_tx_xadd_range_no_snapshot_abort(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));
	D_RW(obj)->value = TEST_VALUE_1;

	TX_BEGIN(pop) {
		ret = pmemobj_tx_xadd_range(obj.oid, VALUE_OFF, VALUE_SIZE,
				POBJ_XADD_NO_SNAPSHOT);
		UT_ASSERTeq(ret, 0);
		D_RW(obj)->value = TEST_VALUE_2;
		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	/*
	 * value added with NO_SNAPSHOT flag should NOT
	 * be rolled back after abort
	 */
	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_2);
}

/*
 * do_tx_xadd_range_no_snapshot_fields -- call pmemobj_tx_add_range
 * on selected fields and NO_SNAPSHOT flag set
 */
static void
do_tx_xadd_range_no_snapshot_fields(PMEMobjpool *pop)
{
	TOID(struct overlap_object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, 1));

	char after_abort[OVERLAP_SIZE];
	memcpy(after_abort, D_RO(obj)->data, OVERLAP_SIZE);
	TX_BEGIN(pop) {
		/*
		 * changes of ranges with NO_SNAPSHOT flag set
		 * should not be reverted after abort
		 */
		TX_XADD_FIELD(obj, data[1], POBJ_XADD_NO_SNAPSHOT);
		D_RW(obj)->data[1] = 1;
		after_abort[1] = 1;

		TX_ADD_FIELD(obj, data[2]);
		D_RW(obj)->data[2] = 2;

		TX_XADD_FIELD(obj, data[5], POBJ_XADD_NO_SNAPSHOT);
		D_RW(obj)->data[5] = 5;
		after_abort[5] = 5;

		TX_ADD_FIELD(obj, data[7]);
		D_RW(obj)->data[7] = 7;

		TX_XADD_FIELD(obj, data[8], POBJ_XADD_NO_SNAPSHOT);
		D_RW(obj)->data[8] = 8;
		after_abort[8] = 8;

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(memcmp(D_RW(obj)->data, after_abort, OVERLAP_SIZE), 0);
}

/*
 * do_tx_xadd_range_no_uninit_check -- call pmemobj_tx_xadd_range for
 * initialized memory with POBJ_XADD_ASSUME_INITIALIZED flag set and commit the
 * tx
 */
static void
do_tx_xadd_range_no_uninit_check_commit(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));

	TX_BEGIN(pop) {
		ret = pmemobj_tx_xadd_range(obj.oid, VALUE_OFF, VALUE_SIZE,
				POBJ_XADD_ASSUME_INITIALIZED);
		UT_ASSERTeq(ret, 0);
		D_RW(obj)->value = TEST_VALUE_1;
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
}

/*
 * do_tx_xadd_range_no_uninit_check -- call pmemobj_tx_xadd_range for
 * uninitialized memory with POBJ_XADD_ASSUME_INITIALIZED flag set and commit
 * the tx
 */
static void
do_tx_xadd_range_no_uninit_check_commit_uninit(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_alloc(pop, TYPE_OBJ, 0));

	TX_BEGIN(pop) {
		ret = pmemobj_tx_xadd_range(obj.oid, VALUE_OFF, VALUE_SIZE,
				POBJ_XADD_ASSUME_INITIALIZED);
		UT_ASSERTeq(ret, 0);

		ret = pmemobj_tx_xadd_range(obj.oid, DATA_OFF, DATA_SIZE,
				POBJ_XADD_ASSUME_INITIALIZED);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;
		D_RW(obj)->data[256] = TEST_VALUE_2;
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	UT_ASSERTeq(D_RO(obj)->data[256], TEST_VALUE_2);
}

/*
 * do_tx_xadd_range_no_uninit_check -- call pmemobj_tx_xadd_range for
 * partially uninitialized memory with POBJ_XADD_ASSUME_INITIALIZED flag set
 * only for uninitialized part and commit the tx
 */
static void
do_tx_xadd_range_no_uninit_check_commit_part_uninit(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_alloc(pop, TYPE_OBJ, VALUE_SIZE));

	TX_BEGIN(pop) {
		ret = pmemobj_tx_add_range(obj.oid, VALUE_OFF, VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		ret = pmemobj_tx_xadd_range(obj.oid, DATA_OFF, DATA_SIZE,
				POBJ_XADD_ASSUME_INITIALIZED);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;
		D_RW(obj)->data[256] = TEST_VALUE_2;
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	UT_ASSERTeq(D_RO(obj)->data[256], TEST_VALUE_2);
}

/*
 * do_tx_add_range_no_uninit_check -- call pmemobj_tx_add_range for
 * partially uninitialized memory.
 */
static void
do_tx_add_range_no_uninit_check_commit_no_flag(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_alloc(pop, TYPE_OBJ, VALUE_SIZE));

	TX_BEGIN(pop) {
		ret = pmemobj_tx_add_range(obj.oid, VALUE_OFF, VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		ret = pmemobj_tx_add_range(obj.oid, DATA_OFF, DATA_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;
		D_RW(obj)->data[256] = TEST_VALUE_2;
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	UT_ASSERTeq(D_RO(obj)->data[256], TEST_VALUE_2);
}

/*
 * do_tx_xadd_range_no_uninit_check_abort -- call pmemobj_tx_range with
 * POBJ_XADD_ASSUME_INITIALIZED flag, modify the value inside aborted
 * transaction
 */
static void
do_tx_xadd_range_no_uninit_check_abort(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_alloc(pop, TYPE_OBJ, 0));

	TX_BEGIN(pop) {
		ret = pmemobj_tx_xadd_range(obj.oid, VALUE_OFF, VALUE_SIZE,
				POBJ_XADD_ASSUME_INITIALIZED);
		UT_ASSERTeq(ret, 0);

		ret = pmemobj_tx_xadd_range(obj.oid, DATA_OFF, DATA_SIZE,
				POBJ_XADD_ASSUME_INITIALIZED);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;
		D_RW(obj)->data[256] = TEST_VALUE_2;
		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END
}

/*
 * do_tx_add_range_overlapping -- call pmemobj_tx_add_range with overlapping
 */
static void
do_tx_add_range_overlapping(PMEMobjpool *pop)
{
	TOID(struct overlap_object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, 1));

	/*
	 * -+-+-+-+-
	 * +++++++++
	 */
	TX_BEGIN(pop) {
		TX_ADD_FIELD(obj, data[1]);
		D_RW(obj)->data[1] = 1;

		TX_ADD_FIELD(obj, data[3]);
		D_RW(obj)->data[3] = 3;

		TX_ADD_FIELD(obj, data[5]);
		D_RW(obj)->data[5] = 5;

		TX_ADD_FIELD(obj, data[7]);
		D_RW(obj)->data[7] = 7;
		TX_ADD(obj);
		memset(D_RW(obj)->data, 0xFF, OVERLAP_SIZE);

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERT(util_is_zeroed(D_RO(obj)->data, OVERLAP_SIZE));

	/*
	 * ++++----++++
	 * --++++++++--
	 */
	TX_BEGIN(pop) {
		pmemobj_tx_add_range(obj.oid, 0, 4);
		memset(D_RW(obj)->data + 0, 1, 4);

		pmemobj_tx_add_range(obj.oid, 8, 4);
		memset(D_RW(obj)->data + 8, 2, 4);

		pmemobj_tx_add_range(obj.oid, 2, 8);
		memset(D_RW(obj)->data + 2, 3, 8);

		TX_ADD(obj);
		memset(D_RW(obj)->data, 0xFF, OVERLAP_SIZE);

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERT(util_is_zeroed(D_RO(obj)->data, OVERLAP_SIZE));

	/*
	 * ++++----++++
	 * ----++++----
	 */
	TX_BEGIN(pop) {
		pmemobj_tx_add_range(obj.oid, 0, 4);
		memset(D_RW(obj)->data + 0, 1, 4);

		pmemobj_tx_add_range(obj.oid, 8, 4);
		memset(D_RW(obj)->data + 8, 2, 4);

		pmemobj_tx_add_range(obj.oid, 4, 4);
		memset(D_RW(obj)->data + 4, 3, 4);

		TX_ADD(obj);
		memset(D_RW(obj)->data, 0xFF, OVERLAP_SIZE);

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERT(util_is_zeroed(D_RO(obj)->data, OVERLAP_SIZE));

	/*
	 * ++++-++-++++
	 * --++++++++--
	 */
	TX_BEGIN(pop) {
		pmemobj_tx_add_range(obj.oid, 0, 4);
		memset(D_RW(obj)->data + 0, 1, 4);

		pmemobj_tx_add_range(obj.oid, 5, 2);
		memset(D_RW(obj)->data + 5, 2, 2);

		pmemobj_tx_add_range(obj.oid, 8, 4);
		memset(D_RW(obj)->data + 8, 3, 4);

		pmemobj_tx_add_range(obj.oid, 2, 8);
		memset(D_RW(obj)->data + 2, 4, 8);

		TX_ADD(obj);
		memset(D_RW(obj)->data, 0xFF, OVERLAP_SIZE);

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERT(util_is_zeroed(D_RO(obj)->data, OVERLAP_SIZE));

	/*
	 * ++++
	 * ++++
	 */
	TX_BEGIN(pop) {
		pmemobj_tx_add_range(obj.oid, 0, 4);
		memset(D_RW(obj)->data, 1, 4);

		pmemobj_tx_add_range(obj.oid, 0, 4);
		memset(D_RW(obj)->data, 2, 4);

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERT(util_is_zeroed(D_RO(obj)->data, OVERLAP_SIZE));
}

/*
 * do_tx_add_range_flag_merge_right -- call pmemobj_tx_add_range with
 * overlapping ranges, but different flags
 */
static void
do_tx_add_range_flag_merge_right(PMEMobjpool *pop)
{
	TOID(struct overlap_object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, 1));

	/*
	 * ++++--------
	 * --++++++++--
	 */
	TX_BEGIN(pop) {
		pmemobj_tx_xadd_range(obj.oid, 0, 4, POBJ_XADD_NO_FLUSH);
		memset(D_RW(obj)->data, 1, 4);

		pmemobj_tx_add_range(obj.oid, 2, 8);
		memset(D_RW(obj)->data + 2, 3, 8);

	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
}

/*
 * do_tx_add_range_flag_merge_left -- call pmemobj_tx_add_range with
 * overlapping ranges, but different flags
 */
static void
do_tx_add_range_flag_merge_left(PMEMobjpool *pop)
{
	TOID(struct overlap_object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, 1));

	/*
	 * --------++++
	 * --++++++++--
	 */
	TX_BEGIN(pop) {
		pmemobj_tx_xadd_range(obj.oid, 8, 4, POBJ_XADD_NO_FLUSH);
		memset(D_RW(obj)->data + 8, 2, 4);

		pmemobj_tx_add_range(obj.oid, 2, 8);
		memset(D_RW(obj)->data + 2, 3, 8);

	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
}

/*
 * do_tx_add_range_flag_merge_middle -- call pmemobj_tx_add_range with
 * three adjacent ranges, but different flags
 */
static void
do_tx_add_range_flag_merge_middle(PMEMobjpool *pop)
{
	TOID(struct overlap_object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, 1));

	/*
	 * ++++----++++
	 * ----++++----
	 */
	TX_BEGIN(pop) {
		pmemobj_tx_xadd_range(obj.oid, 0, 4, POBJ_XADD_NO_FLUSH);
		memset(D_RW(obj)->data, 1, 4);

		pmemobj_tx_xadd_range(obj.oid, 8, 4, POBJ_XADD_NO_FLUSH);
		memset(D_RW(obj)->data + 8, 2, 4);

		pmemobj_tx_add_range(obj.oid, 4, 4);
		memset(D_RW(obj)->data + 4, 3, 4);

	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
}

/*
 * do_tx_add_range_reopen -- check for persistent memory leak in undo log set
 */
static void
do_tx_add_range_reopen(char *path)
{
	for (int i = 0; i < REOPEN_COUNT; i++) {
		PMEMobjpool *pop = pmemobj_open(path, LAYOUT_NAME);
		UT_ASSERTne(pop, NULL);
		TOID(struct root) root = POBJ_ROOT(pop, struct root);
		UT_ASSERT(!TOID_IS_NULL(root));
		UT_ASSERTeq(D_RO(root)->val, i);

		for (int j = 0; j < ROOT_TAB_SIZE; j++)
			UT_ASSERTeq(D_RO(root)->tab[j], i);

		TX_BEGIN(pop) {
			TX_SET(root, val, i + 1);
			TX_ADD_FIELD(root, tab);
			for (int j = 0; j < ROOT_TAB_SIZE; j++)
				D_RW(root)->tab[j] = i + 1;

		} TX_ONABORT {
			UT_ASSERT(0);
		} TX_END

		pmemobj_close(pop);
	}
}

static void
do_tx_add_range_too_large(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));

	TX_BEGIN(pop) {
		pmemobj_tx_add_range(obj.oid, 0,
			PMEMOBJ_MAX_ALLOC_SIZE + 1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTne(errno, 0);
}

static void
do_tx_add_range_zero(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));

	TX_BEGIN(pop) {
		pmemobj_tx_add_range(obj.oid, 0, 0);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTne(errno, 0);
}

/*
 * do_tx_add_range_wrong_uuid -- call pmemobj_tx_xadd_range with
 * POBJ_TX_NO_ABORT flag and wrong uuid
 */
static void
do_tx_add_range_wrong_uuid(PMEMobjpool *pop)
{
	PMEMoid oid = do_tx_alloc(pop, TYPE_OBJ_WRONG_UUID, 0);
	oid.pool_uuid_lo = ~oid.pool_uuid_lo;

	TX_BEGIN(pop) {
		pmemobj_tx_xadd_range(oid, 0, 0, 0);
	}TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(errno, EINVAL);

	TX_BEGIN(pop) {
		pmemobj_tx_xadd_range(oid, 0, 0, POBJ_XADD_NO_ABORT);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(errno, EINVAL);

	TX_BEGIN(pop) {
		pmemobj_tx_set_failure_behavior(POBJ_TX_FAILURE_RETURN);
		pmemobj_tx_add_range(oid, 0, 0);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(errno, EINVAL);

	TX_BEGIN(pop) {
		pmemobj_tx_set_failure_behavior(POBJ_TX_FAILURE_RETURN);
		pmemobj_tx_xadd_range(oid, 0, 0, 0);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(errno, EINVAL);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_tx_add_range");
	util_init();

	if (argc != 3)
		UT_FATAL("usage: %s [file] [0|1]", argv[0]);

	int do_reopen = atoi(argv[2]);

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(argv[1], LAYOUT_NAME, PMEMOBJ_MIN_POOL * 2,
	    S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create");

	if (do_reopen) {
		pmemobj_close(pop);
		do_tx_add_range_reopen(argv[1]);
	} else {
		do_tx_add_range_commit(pop);
		VALGRIND_WRITE_STATS;
		do_tx_add_range_abort(pop);
		VALGRIND_WRITE_STATS;
		do_tx_add_range_commit_nested(pop);
		VALGRIND_WRITE_STATS;
		do_tx_add_range_abort_nested(pop);
		VALGRIND_WRITE_STATS;
		do_tx_add_range_abort_after_nested(pop);
		VALGRIND_WRITE_STATS;
		do_tx_add_range_abort_after_commit(pop);
		VALGRIND_WRITE_STATS;
		do_tx_add_range_twice_commit(pop);
		VALGRIND_WRITE_STATS;
		do_tx_add_range_twice_abort(pop);
		VALGRIND_WRITE_STATS;
		do_tx_add_range_alloc_commit(pop);
		VALGRIND_WRITE_STATS;
		do_tx_add_range_alloc_abort(pop);
		VALGRIND_WRITE_STATS;
		do_tx_add_range_overlapping(pop);
		VALGRIND_WRITE_STATS;
		do_tx_add_range_too_large(pop);
		VALGRIND_WRITE_STATS;
		do_tx_add_huge_range_abort(pop);
		VALGRIND_WRITE_STATS;
		do_tx_add_range_zero(pop);
		VALGRIND_WRITE_STATS;
		do_tx_xadd_range_no_snapshot_commit(pop);
		VALGRIND_WRITE_STATS;
		do_tx_xadd_range_no_snapshot_abort(pop);
		VALGRIND_WRITE_STATS;
		do_tx_xadd_range_twice_no_snapshot_abort(pop);
		VALGRIND_WRITE_STATS;
		do_tx_xadd_range_no_snapshot_fields(pop);
		VALGRIND_WRITE_STATS;
		do_tx_xadd_range_no_uninit_check_commit(pop);
		VALGRIND_WRITE_STATS;
		do_tx_xadd_range_no_uninit_check_commit_uninit(pop);
		VALGRIND_WRITE_STATS;
		do_tx_xadd_range_no_uninit_check_commit_part_uninit(pop);
		VALGRIND_WRITE_STATS;
		do_tx_xadd_range_no_uninit_check_abort(pop);
		VALGRIND_WRITE_STATS;
		do_tx_add_range_no_uninit_check_commit_no_flag(pop);
		VALGRIND_WRITE_STATS;
		do_tx_add_range_wrong_uuid(pop);
		VALGRIND_WRITE_STATS;
		do_tx_add_range_flag_merge_left(pop);
		VALGRIND_WRITE_STATS;
		do_tx_add_range_flag_merge_right(pop);
		VALGRIND_WRITE_STATS;
		do_tx_add_range_flag_merge_middle(pop);
		VALGRIND_WRITE_STATS;
		do_tx_xadd_range_no_flush_commit(pop);
		pmemobj_close(pop);
	}

	DONE(NULL);
}
