/*
 * Copyright 2015-2019, Intel Corporation
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
 * do_tx_alloc -- do tx allocation with specified type number
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
		do_tx_xadd_range_no_flush_commit(pop);
		pmemobj_close(pop);
	}

	DONE(NULL);
}
