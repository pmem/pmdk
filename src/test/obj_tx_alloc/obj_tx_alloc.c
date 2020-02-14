// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2020, Intel Corporation */

/*
 * obj_tx_alloc.c -- unit test for pmemobj_tx_alloc and pmemobj_tx_zalloc
 */
#include <assert.h>
#include <sys/param.h>
#include <string.h>

#include "unittest.h"
#include "libpmemobj.h"
#include "util.h"
#include "valgrind_internal.h"

#define LAYOUT_NAME "tx_alloc"

#define TEST_VALUE_1	1
#define TEST_VALUE_2	2
#define OBJ_SIZE	(200 * 1024)

enum type_number {
	TYPE_NO_TX,
	TYPE_COMMIT,
	TYPE_ABORT,
	TYPE_ZEROED_COMMIT,
	TYPE_ZEROED_ABORT,
	TYPE_XCOMMIT,
	TYPE_XABORT,
	TYPE_XZEROED_COMMIT,
	TYPE_XZEROED_ABORT,
	TYPE_XNOFLUSHED_COMMIT,
	TYPE_COMMIT_NESTED1,
	TYPE_COMMIT_NESTED2,
	TYPE_ABORT_NESTED1,
	TYPE_ABORT_NESTED2,
	TYPE_ABORT_AFTER_NESTED1,
	TYPE_ABORT_AFTER_NESTED2,
	TYPE_OOM,
};

TOID_DECLARE(struct object, TYPE_OOM);

struct object {
	size_t value;
	char data[OBJ_SIZE - sizeof(size_t)];
};

/*
 * do_tx_alloc_oom -- allocates objects until OOM
 */
static void
do_tx_alloc_oom(PMEMobjpool *pop)
{
	int do_alloc = 1;
	size_t alloc_cnt = 0;
	do {
		TX_BEGIN(pop) {
			TOID(struct object) obj = TX_NEW(struct object);
			D_RW(obj)->value = alloc_cnt;
		} TX_ONCOMMIT {
			alloc_cnt++;
		} TX_ONABORT {
			do_alloc = 0;
		} TX_END
	} while (do_alloc);

	size_t bitmap_size = howmany(alloc_cnt, 8);
	char *bitmap = (char *)MALLOC(bitmap_size);
	memset(bitmap, 0, bitmap_size);

	size_t obj_cnt = 0;
	TOID(struct object) i;
	POBJ_FOREACH_TYPE(pop, i) {
		UT_ASSERT(D_RO(i)->value < alloc_cnt);
		UT_ASSERT(!isset(bitmap, D_RO(i)->value));
		setbit(bitmap, D_RO(i)->value);
		obj_cnt++;
	}

	FREE(bitmap);

	UT_ASSERTeq(obj_cnt, alloc_cnt);

	TOID(struct object) o = POBJ_FIRST(pop, struct object);
	while (!TOID_IS_NULL(o)) {
		TOID(struct object) next = POBJ_NEXT(o);
		POBJ_FREE(&o);
		o = next;
	}
}

/*
 * do_tx_alloc_abort_after_nested -- aborts transaction after allocation
 * in nested transaction
 */
static void
do_tx_alloc_abort_after_nested(PMEMobjpool *pop)
{
	TOID(struct object) obj1;
	TOID(struct object) obj2;

	TX_BEGIN(pop) {
		TOID_ASSIGN(obj1, pmemobj_tx_alloc(sizeof(struct object),
				TYPE_ABORT_AFTER_NESTED1));
		UT_ASSERT(!TOID_IS_NULL(obj1));

		D_RW(obj1)->value = TEST_VALUE_1;

		TX_BEGIN(pop) {
			TOID_ASSIGN(obj2, pmemobj_tx_zalloc(
					sizeof(struct object),
					TYPE_ABORT_AFTER_NESTED2));
			UT_ASSERT(!TOID_IS_NULL(obj2));
			UT_ASSERT(util_is_zeroed(D_RO(obj2),
					sizeof(struct object)));

			D_RW(obj2)->value = TEST_VALUE_2;

		} TX_ONCOMMIT {
			UT_ASSERTeq(D_RO(obj2)->value, TEST_VALUE_2);
		} TX_ONABORT {
			UT_ASSERT(0);
		} TX_END

		pmemobj_tx_abort(-1);

	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		TOID_ASSIGN(obj1, OID_NULL);
		TOID_ASSIGN(obj2, OID_NULL);
	} TX_END

	TOID(struct object) first;

	/* check the obj1 object */
	UT_ASSERT(TOID_IS_NULL(obj1));

	first.oid = POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT_AFTER_NESTED1);
	UT_ASSERT(TOID_IS_NULL(first));

	/* check the obj2 object */
	UT_ASSERT(TOID_IS_NULL(obj2));

	first.oid = POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT_AFTER_NESTED2);
	UT_ASSERT(TOID_IS_NULL(first));
}

/*
 * do_tx_alloc_abort_nested -- aborts transaction in nested transaction
 */
static void
do_tx_alloc_abort_nested(PMEMobjpool *pop)
{
	TOID(struct object) obj1;
	TOID(struct object) obj2;

	TX_BEGIN(pop) {
		TOID_ASSIGN(obj1, pmemobj_tx_alloc(sizeof(struct object),
				TYPE_ABORT_NESTED1));
		UT_ASSERT(!TOID_IS_NULL(obj1));

		D_RW(obj1)->value = TEST_VALUE_1;

		TX_BEGIN(pop) {
			TOID_ASSIGN(obj2, pmemobj_tx_zalloc(
					sizeof(struct object),
					TYPE_ABORT_NESTED2));
			UT_ASSERT(!TOID_IS_NULL(obj2));
			UT_ASSERT(util_is_zeroed(D_RO(obj2),
					sizeof(struct object)));

			D_RW(obj2)->value = TEST_VALUE_2;

			pmemobj_tx_abort(-1);
		} TX_ONCOMMIT {
			UT_ASSERT(0);
		} TX_ONABORT {
			TOID_ASSIGN(obj2, OID_NULL);
		} TX_END

	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		TOID_ASSIGN(obj1, OID_NULL);
	} TX_END

	TOID(struct object) first;

	/* check the obj1 object */
	UT_ASSERT(TOID_IS_NULL(obj1));

	first.oid = POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT_NESTED1);
	UT_ASSERT(TOID_IS_NULL(first));

	/* check the obj2 object */
	UT_ASSERT(TOID_IS_NULL(obj2));

	first.oid = POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT_NESTED2);
	UT_ASSERT(TOID_IS_NULL(first));
}

/*
 * do_tx_alloc_commit_nested -- allocates two objects, one in nested transaction
 */
static void
do_tx_alloc_commit_nested(PMEMobjpool *pop)
{
	TOID(struct object) obj1;
	TOID(struct object) obj2;

	TX_BEGIN(pop) {
		TOID_ASSIGN(obj1, pmemobj_tx_alloc(sizeof(struct object),
				TYPE_COMMIT_NESTED1));
		UT_ASSERT(!TOID_IS_NULL(obj1));

		D_RW(obj1)->value = TEST_VALUE_1;

		TX_BEGIN(pop) {
			TOID_ASSIGN(obj2, pmemobj_tx_zalloc(
					sizeof(struct object),
					TYPE_COMMIT_NESTED2));
			UT_ASSERT(!TOID_IS_NULL(obj2));
			UT_ASSERT(util_is_zeroed(D_RO(obj2),
					sizeof(struct object)));

			D_RW(obj2)->value = TEST_VALUE_2;
		} TX_ONCOMMIT {
			UT_ASSERTeq(D_RO(obj1)->value, TEST_VALUE_1);
			UT_ASSERTeq(D_RO(obj2)->value, TEST_VALUE_2);
		} TX_ONABORT {
			UT_ASSERT(0);
		} TX_END

	} TX_ONCOMMIT {
		UT_ASSERTeq(D_RO(obj1)->value, TEST_VALUE_1);
		UT_ASSERTeq(D_RO(obj2)->value, TEST_VALUE_2);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TOID(struct object) first;
	TOID(struct object) next;

	/* check the obj1 object */
	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_COMMIT_NESTED1));
	UT_ASSERT(TOID_EQUALS(first, obj1));
	UT_ASSERTeq(D_RO(first)->value, TEST_VALUE_1);

	TOID_ASSIGN(next, POBJ_NEXT_TYPE_NUM(first.oid));
	UT_ASSERT(TOID_IS_NULL(next));

	/* check the obj2 object */
	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_COMMIT_NESTED2));
	UT_ASSERT(TOID_EQUALS(first, obj2));
	UT_ASSERTeq(D_RO(first)->value, TEST_VALUE_2);

	TOID_ASSIGN(next, POBJ_NEXT_TYPE_NUM(first.oid));
	UT_ASSERT(TOID_IS_NULL(next));
}

/*
 * do_tx_alloc_abort -- allocates an object and aborts the transaction
 */
static void
do_tx_alloc_abort(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_alloc(sizeof(struct object),
				TYPE_ABORT));
		UT_ASSERT(!TOID_IS_NULL(obj));

		D_RW(obj)->value = TEST_VALUE_1;
		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		TOID_ASSIGN(obj, OID_NULL);
	} TX_END

	UT_ASSERT(TOID_IS_NULL(obj));

	TOID(struct object) first;
	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT));
	UT_ASSERT(TOID_IS_NULL(first));
}

/*
 * do_tx_alloc_zerolen -- allocates an object of zero size to trigger tx abort
 */
static void
do_tx_alloc_zerolen(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_alloc(0, TYPE_ABORT));
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		TOID_ASSIGN(obj, OID_NULL);
	} TX_END

	UT_ASSERT(TOID_IS_NULL(obj));

	TOID(struct object) first;
	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT));
	UT_ASSERT(TOID_IS_NULL(first));
}

/*
 * do_tx_alloc_huge -- allocates a huge object to trigger tx abort
 */
static void
do_tx_alloc_huge(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_alloc(PMEMOBJ_MAX_ALLOC_SIZE + 1,
				TYPE_ABORT));
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		TOID_ASSIGN(obj, OID_NULL);
	} TX_END

	UT_ASSERT(TOID_IS_NULL(obj));

	TOID(struct object) first;
	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT));
	UT_ASSERT(TOID_IS_NULL(first));
}

/*
 * do_tx_alloc_commit -- allocates and object
 */
static void
do_tx_alloc_commit(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_alloc(sizeof(struct object),
				TYPE_COMMIT));
		UT_ASSERT(!TOID_IS_NULL(obj));

		D_RW(obj)->value = TEST_VALUE_1;
	} TX_ONCOMMIT {
		UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TOID(struct object) first;
	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_COMMIT));
	UT_ASSERT(TOID_EQUALS(first, obj));
	UT_ASSERTeq(D_RO(first)->value, D_RO(obj)->value);

	TOID(struct object) next;
	next = POBJ_NEXT(first);
	UT_ASSERT(TOID_IS_NULL(next));
}

/*
 * do_tx_zalloc_abort -- allocates a zeroed object and aborts the transaction
 */
static void
do_tx_zalloc_abort(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_zalloc(sizeof(struct object),
				TYPE_ZEROED_ABORT));
		UT_ASSERT(!TOID_IS_NULL(obj));
		UT_ASSERT(util_is_zeroed(D_RO(obj), sizeof(struct object)));

		D_RW(obj)->value = TEST_VALUE_1;
		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		TOID_ASSIGN(obj, OID_NULL);
	} TX_END

	UT_ASSERT(TOID_IS_NULL(obj));

	TOID(struct object) first;
	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_ZEROED_ABORT));
	UT_ASSERT(TOID_IS_NULL(first));
}

/*
 * do_tx_zalloc_zerolen -- allocate an object of zero size to trigger tx abort
 */
static void
do_tx_zalloc_zerolen(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_zalloc(0, TYPE_ZEROED_ABORT));
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		TOID_ASSIGN(obj, OID_NULL);
	} TX_END

	UT_ASSERT(TOID_IS_NULL(obj));

	TOID(struct object) first;
	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_ZEROED_ABORT));
	UT_ASSERT(TOID_IS_NULL(first));
}

/*
 * do_tx_zalloc_huge -- allocates a huge object to trigger tx abort
 */
static void
do_tx_zalloc_huge(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_zalloc(PMEMOBJ_MAX_ALLOC_SIZE + 1,
				TYPE_ZEROED_ABORT));
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		TOID_ASSIGN(obj, OID_NULL);
	} TX_END

	UT_ASSERT(TOID_IS_NULL(obj));

	TOID(struct object) first;
	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_ZEROED_ABORT));
	UT_ASSERT(TOID_IS_NULL(first));
}

/*
 * do_tx_zalloc_commit -- allocates zeroed object
 */
static void
do_tx_zalloc_commit(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_zalloc(sizeof(struct object),
				TYPE_ZEROED_COMMIT));
		UT_ASSERT(!TOID_IS_NULL(obj));
		UT_ASSERT(util_is_zeroed(D_RO(obj), sizeof(struct object)));

		D_RW(obj)->value = TEST_VALUE_1;
	} TX_ONCOMMIT {
		UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TOID(struct object) first;
	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_ZEROED_COMMIT));
	UT_ASSERT(TOID_EQUALS(first, obj));
	UT_ASSERTeq(D_RO(first)->value, D_RO(obj)->value);

	TOID(struct object) next;
	next = POBJ_NEXT(first);
	UT_ASSERT(TOID_IS_NULL(next));
}

/*
 * do_tx_xalloc_abort -- allocates a zeroed object and aborts the transaction
 */
static void
do_tx_xalloc_abort(PMEMobjpool *pop)
{
	/* xalloc 0 */
	TOID(struct object) obj;
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_xalloc(sizeof(struct object),
				TYPE_XABORT, 0));
		UT_ASSERT(!TOID_IS_NULL(obj));

		D_RW(obj)->value = TEST_VALUE_1;
		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		TOID_ASSIGN(obj, OID_NULL);
	} TX_END

	UT_ASSERT(TOID_IS_NULL(obj));

	TOID(struct object) first;
	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_XABORT));
	UT_ASSERT(TOID_IS_NULL(first));

	/* xalloc ZERO */
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_xalloc(sizeof(struct object),
				TYPE_XZEROED_ABORT, POBJ_XALLOC_ZERO));
		UT_ASSERT(!TOID_IS_NULL(obj));
		UT_ASSERT(util_is_zeroed(D_RO(obj), sizeof(struct object)));

		D_RW(obj)->value = TEST_VALUE_1;
		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		TOID_ASSIGN(obj, OID_NULL);
	} TX_END

	UT_ASSERT(TOID_IS_NULL(obj));

	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_XZEROED_ABORT));
	UT_ASSERT(TOID_IS_NULL(first));
}

/*
 * do_tx_xalloc_zerolen -- allocate an object of zero size to trigger tx abort
 */
static void
do_tx_xalloc_zerolen(PMEMobjpool *pop)
{
	/* xalloc 0 */
	TOID(struct object) obj;
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_xalloc(0, TYPE_XABORT, 0));
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		TOID_ASSIGN(obj, OID_NULL);
	} TX_END

	UT_ASSERT(TOID_IS_NULL(obj));

	/* xalloc 0 with POBJ_XALLOC_NO_ABORT flag */
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_xalloc(0, TYPE_XABORT,
				POBJ_XALLOC_NO_ABORT));
	} TX_ONCOMMIT {
		TOID_ASSIGN(obj, OID_NULL);
	} TX_ONABORT {
		UT_ASSERT(0); /* should not get to this point */
	} TX_END

	UT_ASSERT(TOID_IS_NULL(obj));

	TOID(struct object) first;
	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_XABORT));
	UT_ASSERT(TOID_IS_NULL(first));

	/* xalloc ZERO */
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_xalloc(0, TYPE_XZEROED_ABORT,
				POBJ_XALLOC_ZERO));
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		TOID_ASSIGN(obj, OID_NULL);
	} TX_END

	UT_ASSERT(TOID_IS_NULL(obj));

	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_XZEROED_ABORT));
	UT_ASSERT(TOID_IS_NULL(first));
}

/*
 * do_tx_xalloc_huge -- allocates a huge object to trigger tx abort
 */
static void
do_tx_xalloc_huge(PMEMobjpool *pop)
{
	/* xalloc 0 */
	TOID(struct object) obj;
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_xalloc(PMEMOBJ_MAX_ALLOC_SIZE + 1,
				TYPE_XABORT, 0));
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		TOID_ASSIGN(obj, OID_NULL);
	} TX_END

	UT_ASSERT(TOID_IS_NULL(obj));

	TOID(struct object) first;
	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_XABORT));
	UT_ASSERT(TOID_IS_NULL(first));

	/* xalloc ZERO */
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_xalloc(PMEMOBJ_MAX_ALLOC_SIZE + 1,
				TYPE_XZEROED_ABORT, POBJ_XALLOC_ZERO));
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		TOID_ASSIGN(obj, OID_NULL);
	} TX_END

	UT_ASSERT(TOID_IS_NULL(obj));

	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_XZEROED_ABORT));
	UT_ASSERT(TOID_IS_NULL(first));

	/*
	 * do xalloc until overfilled and then
	 * free last successful allocation
	 */
	uint64_t tot_allocated = 0, alloc_size = (5 * 1024 *1024);
	int rc = 0;
	PMEMoid oid, prev_oid;
	POBJ_FOREACH_SAFE(pop, oid, prev_oid) {
		pmemobj_free(&oid);
	}
	TOID_ASSIGN(first, pmemobj_first(pop));
	UT_ASSERT(TOID_IS_NULL(first));

	TX_BEGIN(pop) {
		while (rc == 0) {
			oid = pmemobj_tx_xalloc(alloc_size, 0,
					POBJ_XALLOC_NO_ABORT);
			if (oid.off == 0)
				rc = -1;
			else {
				tot_allocated += alloc_size;
				prev_oid = oid;
			}
		}
		rc = pmemobj_tx_free(prev_oid);
	} TX_ONCOMMIT {
		UT_ASSERTeq(errno, ENOMEM);
		UT_ASSERTeq(rc, 0);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
}

/*
 * do_tx_xalloc_commit -- allocates zeroed object
 */
static void
do_tx_xalloc_commit(PMEMobjpool *pop)
{
	/* xalloc 0 */
	TOID(struct object) obj;
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_xalloc(sizeof(struct object),
				TYPE_XCOMMIT, 0));
		UT_ASSERT(!TOID_IS_NULL(obj));

		D_RW(obj)->value = TEST_VALUE_1;
	} TX_ONCOMMIT {
		UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TOID(struct object) first;
	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_XCOMMIT));
	UT_ASSERT(TOID_EQUALS(first, obj));
	UT_ASSERTeq(D_RO(first)->value, D_RO(obj)->value);

	TOID(struct object) next;
	TOID_ASSIGN(next, POBJ_NEXT_TYPE_NUM(first.oid));
	UT_ASSERT(TOID_IS_NULL(next));

	/* xalloc ZERO */
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_xalloc(sizeof(struct object),
				TYPE_XZEROED_COMMIT, POBJ_XALLOC_ZERO));
		UT_ASSERT(!TOID_IS_NULL(obj));
		UT_ASSERT(util_is_zeroed(D_RO(obj), sizeof(struct object)));

		D_RW(obj)->value = TEST_VALUE_1;
	} TX_ONCOMMIT {
		UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_XZEROED_COMMIT));
	UT_ASSERT(TOID_EQUALS(first, obj));
	UT_ASSERTeq(D_RO(first)->value, D_RO(obj)->value);

	TOID_ASSIGN(next, POBJ_NEXT_TYPE_NUM(first.oid));
	UT_ASSERT(TOID_IS_NULL(next));
}

/*
 * do_tx_xalloc_noflush -- allocates zeroed object
 */
static void
do_tx_xalloc_noflush(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_xalloc(sizeof(struct object),
				TYPE_XNOFLUSHED_COMMIT, POBJ_XALLOC_NO_FLUSH));
		UT_ASSERT(!TOID_IS_NULL(obj));

		D_RW(obj)->data[OBJ_SIZE - sizeof(size_t) - 1] = TEST_VALUE_1;
		/* let pmemcheck find we didn't flush it */
	} TX_ONCOMMIT {
		UT_ASSERTeq(D_RO(obj)->data[OBJ_SIZE - sizeof(size_t) - 1],
			TEST_VALUE_1);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TOID(struct object) first;
	TOID_ASSIGN(first, POBJ_FIRST_TYPE_NUM(pop, TYPE_XNOFLUSHED_COMMIT));
	UT_ASSERT(TOID_EQUALS(first, obj));
	UT_ASSERTeq(D_RO(first)->data[OBJ_SIZE - sizeof(size_t) - 1],
		D_RO(obj)->data[OBJ_SIZE - sizeof(size_t) - 1]);

	TOID(struct object) next;
	TOID_ASSIGN(next, POBJ_NEXT_TYPE_NUM(first.oid));
	UT_ASSERT(TOID_IS_NULL(next));
}

/*
 * do_tx_root -- retrieve root inside of transaction
 */
static void
do_tx_root(PMEMobjpool *pop)
{
	size_t root_size = 24;
	TX_BEGIN(pop) {
		PMEMoid root = pmemobj_root(pop, root_size);
		UT_ASSERT(!OID_IS_NULL(root));
		UT_ASSERT(util_is_zeroed(pmemobj_direct(root),
				root_size));
		UT_ASSERTeq(root_size, pmemobj_root_size(pop));
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
}

/*
 * do_tx_alloc_many -- allocates many objects inside of a single transaction
 */
static void
do_tx_alloc_many(PMEMobjpool *pop)
{
#define TX_ALLOC_COUNT 70 /* bigger than max reservations */
	PMEMoid oid, oid2;
	POBJ_FOREACH_SAFE(pop, oid, oid2) {
		pmemobj_free(&oid);
	}

	TOID(struct object) first;
	TOID_ASSIGN(first, pmemobj_first(pop));
	UT_ASSERT(TOID_IS_NULL(first));

	PMEMoid oids[TX_ALLOC_COUNT];
	TX_BEGIN(pop) {
		for (int i = 0; i < TX_ALLOC_COUNT; ++i) {
			oids[i] = pmemobj_tx_alloc(1, 0);
			UT_ASSERT(!OID_IS_NULL(oids[i]));
		}
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TX_BEGIN(pop) {
		/* empty tx to make sure there's no leftover state */
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TX_BEGIN(pop) {
		for (int i = 0; i < TX_ALLOC_COUNT; ++i) {
			pmemobj_tx_free(oids[i]);
		}
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(first, pmemobj_first(pop));
	UT_ASSERT(TOID_IS_NULL(first));
#undef TX_ALLOC_COUNT
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_tx_alloc");
	util_init();

	if (argc != 2)
		UT_FATAL("usage: %s [file]", argv[0]);

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(argv[1], LAYOUT_NAME, 0,
				S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create");

	do_tx_root(pop);
	VALGRIND_WRITE_STATS;

	/* alloc */
	do_tx_alloc_commit(pop);
	VALGRIND_WRITE_STATS;

	do_tx_alloc_abort(pop);
	VALGRIND_WRITE_STATS;

	do_tx_alloc_zerolen(pop);
	VALGRIND_WRITE_STATS;

	do_tx_alloc_huge(pop);
	VALGRIND_WRITE_STATS;

	/* zalloc */
	do_tx_zalloc_commit(pop);
	VALGRIND_WRITE_STATS;

	do_tx_zalloc_abort(pop);
	VALGRIND_WRITE_STATS;

	do_tx_zalloc_zerolen(pop);
	VALGRIND_WRITE_STATS;

	do_tx_zalloc_huge(pop);
	VALGRIND_WRITE_STATS;

	/* xalloc */
	do_tx_xalloc_commit(pop);
	VALGRIND_WRITE_STATS;

	do_tx_xalloc_abort(pop);
	VALGRIND_WRITE_STATS;

	do_tx_xalloc_zerolen(pop);
	VALGRIND_WRITE_STATS;

	do_tx_xalloc_huge(pop);
	VALGRIND_WRITE_STATS;

	/* alloc */
	do_tx_alloc_commit_nested(pop);
	VALGRIND_WRITE_STATS;

	do_tx_alloc_abort_nested(pop);
	VALGRIND_WRITE_STATS;

	do_tx_alloc_abort_after_nested(pop);
	VALGRIND_WRITE_STATS;

	do_tx_alloc_oom(pop);
	VALGRIND_WRITE_STATS;

	do_tx_alloc_many(pop);
	VALGRIND_WRITE_STATS;

	do_tx_xalloc_noflush(pop);

	pmemobj_close(pop);

	DONE(NULL);
}
