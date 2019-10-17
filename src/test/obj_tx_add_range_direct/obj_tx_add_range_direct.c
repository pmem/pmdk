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
 * obj_tx_add_range_direct.c -- unit test for pmemobj_tx_add_range_direct
 */
#include <string.h>
#include <stddef.h>

#include "tx.h"
#include "unittest.h"
#include "util.h"
#include "valgrind_internal.h"

#define LAYOUT_NAME "tx_add_range_direct"

#define OBJ_SIZE	1024

enum type_number {
	TYPE_OBJ,
	TYPE_OBJ_ABORT,
};

TOID_DECLARE(struct object, 0);

struct object {
	size_t value;
	unsigned char data[OBJ_SIZE - sizeof(size_t)];
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
do_tx_zalloc(PMEMobjpool *pop, unsigned type_num)
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
		pmemobj_memset(pop, pmemobj_direct(ret), 0, init_num, 0);
	} TX_END

	return ret;
}

/*
 * do_tx_add_range_alloc_commit -- call add_range_direct on object allocated
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

		char *ptr = (char *)pmemobj_direct(obj.oid);
		ret = pmemobj_tx_add_range_direct(ptr + VALUE_OFF,
				VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;

		ret = pmemobj_tx_add_range_direct(ptr + DATA_OFF,
				DATA_SIZE);
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
 * do_tx_add_range_alloc_abort -- call add_range_direct on object allocated
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

		char *ptr = (char *)pmemobj_direct(obj.oid);
		ret = pmemobj_tx_add_range_direct(ptr + VALUE_OFF,
				VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;

		ret = pmemobj_tx_add_range_direct(ptr + DATA_OFF,
				DATA_SIZE);
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
 * do_tx_add_range_twice_commit -- call add_range_direct one the same area
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
		char *ptr = (char *)pmemobj_direct(obj.oid);
		ret = pmemobj_tx_add_range_direct(ptr + VALUE_OFF,
				VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;

		ret = pmemobj_tx_add_range_direct(ptr + VALUE_OFF,
				VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_2;
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_2);
}

/*
 * do_tx_add_range_twice_abort -- call add_range_direct one the same area
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
		char *ptr = (char *)pmemobj_direct(obj.oid);
		ret = pmemobj_tx_add_range_direct(ptr + VALUE_OFF,
				VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;

		ret = pmemobj_tx_add_range_direct(ptr + VALUE_OFF,
				VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_2;

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, 0);
}

/*
 * do_tx_add_range_abort_after_nested -- call add_range_direct and
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
		char *ptr1 = (char *)pmemobj_direct(obj1.oid);
		ret = pmemobj_tx_add_range_direct(ptr1 + VALUE_OFF,
				VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj1)->value = TEST_VALUE_1;

		TX_BEGIN(pop) {
			char *ptr2 = (char *)pmemobj_direct(obj2.oid);
			ret = pmemobj_tx_add_range_direct(ptr2 + DATA_OFF,
					DATA_SIZE);
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
 * do_tx_add_range_abort_nested -- call add_range_direct and
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
		char *ptr1 = (char *)pmemobj_direct(obj1.oid);
		ret = pmemobj_tx_add_range_direct(ptr1 + VALUE_OFF,
				VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj1)->value = TEST_VALUE_1;

		TX_BEGIN(pop) {
			char *ptr2 = (char *)pmemobj_direct(obj2.oid);
			ret = pmemobj_tx_add_range_direct(ptr2 + DATA_OFF,
					DATA_SIZE);
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
 * do_tx_add_range_commit_nested -- call add_range_direct and commit the tx
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
		char *ptr1 = (char *)pmemobj_direct(obj1.oid);
		ret = pmemobj_tx_add_range_direct(ptr1 + VALUE_OFF,
				VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj1)->value = TEST_VALUE_1;

		TX_BEGIN(pop) {
			char *ptr2 = (char *)pmemobj_direct(obj2.oid);
			ret = pmemobj_tx_add_range_direct(ptr2 + DATA_OFF,
					DATA_SIZE);
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
 * do_tx_add_range_abort -- call add_range_direct and abort the tx
 */
static void
do_tx_add_range_abort(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));

	TX_BEGIN(pop) {
		char *ptr = (char *)pmemobj_direct(obj.oid);
		ret = pmemobj_tx_add_range_direct(ptr + VALUE_OFF,
				VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, 0);
}

/*
 * do_tx_add_range_commit -- call add_range_direct and commit tx
 */
static void
do_tx_add_range_commit(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));

	TX_BEGIN(pop) {
		char *ptr = (char *)pmemobj_direct(obj.oid);
		ret = pmemobj_tx_add_range_direct(ptr + VALUE_OFF,
				VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
}

/*
 * do_tx_xadd_range_no_flush_commit -- call xadd_range_direct with
 * POBJ_XADD_NO_FLUSH flag set and commit tx
 */
static void
do_tx_xadd_range_no_flush_commit(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));

	TX_BEGIN(pop) {
		char *ptr = (char *)pmemobj_direct(obj.oid);
		ret = pmemobj_tx_xadd_range_direct(ptr + VALUE_OFF,
				VALUE_SIZE, POBJ_XADD_NO_FLUSH);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;
		/* let pmemcheck find we didn't flush it */
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
}

/*
 * do_tx_xadd_range_no_snapshot_commit -- call xadd_range_direct with
 * POBJ_XADD_NO_SNAPSHOT flag, commit the transaction
 */
static void
do_tx_xadd_range_no_snapshot_commit(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));

	TX_BEGIN(pop) {
		char *ptr = (char *)pmemobj_direct(obj.oid);
		ret = pmemobj_tx_xadd_range_direct(ptr + VALUE_OFF,
				VALUE_SIZE, POBJ_XADD_NO_SNAPSHOT);
		UT_ASSERTeq(ret, 0);

		D_RW(obj)->value = TEST_VALUE_1;
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
}

/*
 * do_tx_xadd_range_no_snapshot_abort -- call xadd_range_direct with
 * POBJ_XADD_NO_SNAPSHOT flag, modify the value, abort the transaction
 */
static void
do_tx_xadd_range_no_snapshot_abort(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));
	D_RW(obj)->value = TEST_VALUE_1;

	TX_BEGIN(pop) {
		char *ptr = (char *)pmemobj_direct(obj.oid);
		ret = pmemobj_tx_xadd_range_direct(ptr + VALUE_OFF, VALUE_SIZE,
				POBJ_XADD_NO_SNAPSHOT);
		UT_ASSERTeq(ret, 0);
		D_RW(obj)->value = TEST_VALUE_2;
		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	/*
	 * value added with NO_SNAPSHOT flag should NOT be rolled back
	 * after abort
	 */
	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_2);
}

/*
 * do_tx_xadd_range_no_uninit_check -- call xdd_range_direct for
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
		char *ptr = (char *)pmemobj_direct(obj.oid);
		ret = pmemobj_tx_xadd_range_direct(ptr + VALUE_OFF, VALUE_SIZE,
				POBJ_XADD_ASSUME_INITIALIZED);
		UT_ASSERTeq(ret, 0);
		D_RW(obj)->value = TEST_VALUE_1;
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
}

/*
 * do_tx_xadd_range_no_uninit_check -- call xadd_range_direct for
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
		char *ptr = (char *)pmemobj_direct(obj.oid);
		ret = pmemobj_tx_xadd_range_direct(ptr + VALUE_OFF, VALUE_SIZE,
				POBJ_XADD_ASSUME_INITIALIZED);
		UT_ASSERTeq(ret, 0);

		ret = pmemobj_tx_xadd_range_direct(ptr + DATA_OFF, DATA_SIZE,
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
 * do_tx_xadd_range_no_uninit_check -- call xadd_range_direct for
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
		char *ptr = (char *)pmemobj_direct(obj.oid);
		ret = pmemobj_tx_add_range_direct(ptr + VALUE_OFF, VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		ret = pmemobj_tx_xadd_range_direct(ptr + DATA_OFF, DATA_SIZE,
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
 * do_tx_add_range_no_uninit_check -- call add_range_direct for
 * partially uninitialized memory.
 */
static void
do_tx_add_range_no_uninit_check_commit_no_flag(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_alloc(pop, TYPE_OBJ, VALUE_SIZE));

	TX_BEGIN(pop) {
		char *ptr = (char *)pmemobj_direct(obj.oid);
		ret = pmemobj_tx_add_range_direct(ptr + VALUE_OFF, VALUE_SIZE);
		UT_ASSERTeq(ret, 0);

		ret = pmemobj_tx_add_range_direct(ptr + DATA_OFF, DATA_SIZE);
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
		char *ptr = (char *)pmemobj_direct(obj.oid);
		ret = pmemobj_tx_xadd_range_direct(ptr + VALUE_OFF, VALUE_SIZE,
				POBJ_XADD_ASSUME_INITIALIZED);
		UT_ASSERTeq(ret, 0);

		ret = pmemobj_tx_xadd_range_direct(ptr + DATA_OFF, DATA_SIZE,
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
 * do_tx_commit_and_abort -- use range cache, commit and then abort to make
 *	sure that it won't affect previously modified data.
 */
static void
do_tx_commit_and_abort(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));

	TX_BEGIN(pop) {
		TX_SET(obj, value, TEST_VALUE_1); /* this will land in cache */
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TX_BEGIN(pop) {
		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
}

/*
 * test_add_direct_macros -- test TX_ADD_DIRECT, TX_ADD_FIELD_DIRECT and
 * TX_SET_DIRECT
 */
static void
test_add_direct_macros(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));

	TX_BEGIN(pop) {
		struct object *o = D_RW(obj);
		TX_SET_DIRECT(o, value, TEST_VALUE_1);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);

	TX_BEGIN(pop) {
		struct object *o = D_RW(obj);
		TX_ADD_DIRECT(o);
		o->value = TEST_VALUE_2;
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_2);

	TX_BEGIN(pop) {
		struct object *o = D_RW(obj);
		TX_ADD_FIELD_DIRECT(o, value);
		o->value = TEST_VALUE_1;
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
}

#define MAX_CACHED_RANGES 100

/*
 * test_tx_corruption_bug -- test whether tx_adds for small objects from one
 * transaction does NOT leak to the next transaction
 */
static void
test_tx_corruption_bug(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));
	struct object *o = D_RW(obj);
	unsigned char i;
	UT_COMPILE_ERROR_ON(1.5 * MAX_CACHED_RANGES > 255);

	TX_BEGIN(pop) {
		for (i = 0; i < 1.5 * MAX_CACHED_RANGES; ++i) {
			TX_ADD_DIRECT(&o->data[i]);
			o->data[i] = i;
		}
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	for (i = 0; i < 1.5 * MAX_CACHED_RANGES; ++i)
		UT_ASSERTeq((unsigned char)o->data[i], i);

	TX_BEGIN(pop) {
		for (i = 0; i < 0.1 * MAX_CACHED_RANGES; ++i) {
			TX_ADD_DIRECT(&o->data[i]);
			o->data[i] = i + 10;
		}
		pmemobj_tx_abort(EINVAL);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	for (i = 0; i < 1.5 * MAX_CACHED_RANGES; ++i)
		UT_ASSERTeq((unsigned char)o->data[i], i);

	pmemobj_free(&obj.oid);
}

static void
do_tx_add_range_too_large(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_zalloc(pop, TYPE_OBJ));
	int ret = 0;
	TX_BEGIN(pop) {
		ret = pmemobj_tx_add_range_direct(pmemobj_direct(obj.oid),
			PMEMOBJ_MAX_ALLOC_SIZE + 1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		UT_ASSERTeq(errno, EINVAL);
		UT_ASSERTeq(ret, 0);
	} TX_END

	errno = 0;
	ret = 0;

	TX_BEGIN(pop) {
		ret = pmemobj_tx_xadd_range_direct(pmemobj_direct(obj.oid),
				PMEMOBJ_MAX_ALLOC_SIZE + 1, POBJ_XADD_NO_ABORT);
	} TX_ONCOMMIT {
		UT_ASSERTeq(errno, EINVAL);
		UT_ASSERTeq(ret, EINVAL);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	errno = 0;
}

static void
do_tx_add_range_lots_of_small_snapshots(PMEMobjpool *pop)
{
	size_t s = TX_DEFAULT_RANGE_CACHE_SIZE * 2;
	size_t snapshot_s = 8;
	PMEMoid obj;
	int ret = pmemobj_zalloc(pop, &obj, s, 0);
	UT_ASSERTeq(ret, 0);

	TX_BEGIN(pop) {
		for (size_t n = 0; n < s; n += snapshot_s) {
			void *addr = (void *)((size_t)pmemobj_direct(obj) + n);
			pmemobj_tx_add_range_direct(addr, snapshot_s);
		}
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
}

static void
do_tx_add_cache_overflowing_range(PMEMobjpool *pop)
{
	/*
	 * This test adds snapshot to the cache, but in way that results in
	 * one of the add_range being split into two caches.
	 */
	size_t s = TX_DEFAULT_RANGE_CACHE_SIZE * 2;
	size_t snapshot_s = TX_DEFAULT_RANGE_CACHE_THRESHOLD - 8;
	PMEMoid obj;
	int ret = pmemobj_zalloc(pop, &obj, s, 0);
	UT_ASSERTeq(ret, 0);

	TX_BEGIN(pop) {
		size_t n = 0;
		while (n != s) {
			if (n + snapshot_s > s)
				snapshot_s = s - n;
			void *addr = (void *)((size_t)pmemobj_direct(obj) + n);
			pmemobj_tx_add_range_direct(addr, snapshot_s);
			memset(addr, 0xc, snapshot_s);
			n += snapshot_s;
		}
		pmemobj_tx_abort(0);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	UT_ASSERT(util_is_zeroed(pmemobj_direct(obj), s));

	UT_ASSERTne(errno, 0);
	errno = 0;
	pmemobj_free(&obj);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_tx_add_range_direct");
	util_init();

	if (argc != 2)
		UT_FATAL("usage: %s [file]", argv[0]);

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(argv[1], LAYOUT_NAME, PMEMOBJ_MIN_POOL * 4,
			S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create");

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
	do_tx_commit_and_abort(pop);
	VALGRIND_WRITE_STATS;
	test_add_direct_macros(pop);
	VALGRIND_WRITE_STATS;
	test_tx_corruption_bug(pop);
	VALGRIND_WRITE_STATS;
	do_tx_add_range_too_large(pop);
	VALGRIND_WRITE_STATS;
	do_tx_add_range_lots_of_small_snapshots(pop);
	VALGRIND_WRITE_STATS;
	do_tx_add_cache_overflowing_range(pop);
	VALGRIND_WRITE_STATS;
	do_tx_xadd_range_no_snapshot_commit(pop);
	VALGRIND_WRITE_STATS;
	do_tx_xadd_range_no_snapshot_abort(pop);
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
	do_tx_xadd_range_no_flush_commit(pop);
	pmemobj_close(pop);

	DONE(NULL);
}
