// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2020, Intel Corporation */

/*
 * obj_tx_free.c -- unit test for pmemobj_tx_free
 */
#include <sys/param.h>
#include <string.h>

#include "unittest.h"
#include "util.h"
#include "valgrind_internal.h"

#define LAYOUT_NAME "tx_free"

#define OBJ_SIZE	(200 * 1024)

enum type_number {
	TYPE_FREE_NO_TX,
	TYPE_FREE_WRONG_UUID,
	TYPE_FREE_COMMIT,
	TYPE_FREE_ABORT,
	TYPE_FREE_COMMIT_NESTED1,
	TYPE_FREE_COMMIT_NESTED2,
	TYPE_FREE_ABORT_NESTED1,
	TYPE_FREE_ABORT_NESTED2,
	TYPE_FREE_ABORT_AFTER_NESTED1,
	TYPE_FREE_ABORT_AFTER_NESTED2,
	TYPE_FREE_OOM,
	TYPE_FREE_ALLOC,
	TYPE_FREE_AFTER_ABORT,
	TYPE_FREE_MANY_TIMES,
};

TOID_DECLARE(struct object, 0);

struct object {
	size_t value;
	char data[OBJ_SIZE - sizeof(size_t)];
};

/*
 * do_tx_alloc -- do tx allocation with specified type number
 */
static PMEMoid
do_tx_alloc(PMEMobjpool *pop, unsigned type_num)
{
	PMEMoid ret = OID_NULL;

	TX_BEGIN(pop) {
		ret = pmemobj_tx_alloc(sizeof(struct object), type_num);
	} TX_END

	return ret;
}

/*
 * do_tx_free_wrong_uuid -- try to free object with invalid uuid
 */
static void
do_tx_free_wrong_uuid(PMEMobjpool *pop)
{
	volatile int ret = 0;
	PMEMoid oid = do_tx_alloc(pop, TYPE_FREE_WRONG_UUID);
	oid.pool_uuid_lo = ~oid.pool_uuid_lo;

	TX_BEGIN(pop) {
		ret = pmemobj_tx_free(oid);
		UT_ASSERTeq(ret, 0);
	} TX_ONABORT {
		ret = -1;
	} TX_END

	UT_ASSERTeq(ret, -1);

	/* POBJ_XFREE_NO_ABORT flag is set */
	TX_BEGIN(pop) {
		ret = pmemobj_tx_xfree(oid, POBJ_XFREE_NO_ABORT);
	} TX_ONCOMMIT {
		UT_ASSERTeq(ret, EINVAL);
	} TX_ONABORT {
		UT_ASSERT(0); /* should not get to this point */
	} TX_END

	TOID(struct object) obj;
	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_FREE_WRONG_UUID));
	UT_ASSERT(!TOID_IS_NULL(obj));
}

/*
 * do_tx_free_wrong_uuid_abort_on_failure -- try to free object with
 * invalid uuid in a transaction where pmemobj_tx_set_failure_behavior
 * was called.
 */
static void
do_tx_free_wrong_uuid_abort_on_failure(PMEMobjpool *pop)
{
	volatile int ret = 0;
	PMEMoid oid = do_tx_alloc(pop, TYPE_FREE_WRONG_UUID);
	oid.pool_uuid_lo = ~oid.pool_uuid_lo;

	/* pmemobj_tx_set_failure_behavior is called */
	TX_BEGIN(pop) {
		pmemobj_tx_set_failure_behavior(POBJ_TX_FAILURE_RETURN);

		UT_ASSERTeq(pmemobj_tx_get_failure_behavior(),
			POBJ_TX_FAILURE_RETURN);
		ret = pmemobj_tx_free(oid);
	} TX_ONCOMMIT {
		UT_ASSERTeq(ret, EINVAL);
	} TX_ONABORT {
		UT_ASSERT(0); /* should not get to this point */
	} TX_END

	/* pmemobj_tx_set_failure_behavior is called */
	TX_BEGIN(pop) {
		pmemobj_tx_set_failure_behavior(POBJ_TX_FAILURE_RETURN);

		UT_ASSERTeq(pmemobj_tx_get_failure_behavior(),
			POBJ_TX_FAILURE_RETURN);
		ret = pmemobj_tx_xfree(oid, 0);
	} TX_ONCOMMIT {
		UT_ASSERTeq(ret, EINVAL);
	} TX_ONABORT {
		UT_ASSERT(0); /* should not get to this point */
	} TX_END

	/* pmemobj_tx_set_failure_behavior is called in outer tx */
	TX_BEGIN(pop) {
		pmemobj_tx_set_failure_behavior(POBJ_TX_FAILURE_RETURN);
		TX_BEGIN(pop) {
			UT_ASSERTeq(pmemobj_tx_get_failure_behavior(),
				POBJ_TX_FAILURE_RETURN);
			ret = pmemobj_tx_free(oid);
		} TX_ONCOMMIT {
			UT_ASSERTeq(ret, EINVAL);
		} TX_ONABORT {
			UT_ASSERT(0); /* should not get to this point */
		} TX_END
		ret = pmemobj_tx_free(oid);
	} TX_ONCOMMIT {
		UT_ASSERTeq(ret, EINVAL);
	} TX_ONABORT {
		UT_ASSERT(0); /* should not get to this point */
	} TX_END

	/* pmemobj_tx_set_failure_behavior is called in neighbour tx */
	TX_BEGIN(pop) {
		TX_BEGIN(pop) {
			pmemobj_tx_set_failure_behavior(POBJ_TX_FAILURE_RETURN);
			ret = pmemobj_tx_free(oid);
		} TX_ONCOMMIT {
			UT_ASSERTeq(ret, EINVAL);
		} TX_ONABORT {
			UT_ASSERT(0); /* should not get to this point */
		} TX_END

		TX_BEGIN(pop) {
			UT_ASSERTeq(pmemobj_tx_get_failure_behavior(),
				POBJ_TX_FAILURE_ABORT);
		} TX_ONCOMMIT {
			UT_ASSERTeq(ret, EINVAL);
		} TX_ONABORT {
			UT_ASSERT(0); /* should not get to this point */
		} TX_END
	} TX_ONCOMMIT {
		UT_ASSERTeq(ret, EINVAL);
	} TX_ONABORT {
		UT_ASSERT(0); /* should not get to this point */
	} TX_END

	/* pmemobj_tx_set_failure_behavior is called in neighbour tx */
	TX_BEGIN(pop) {
		pmemobj_tx_set_failure_behavior(POBJ_TX_FAILURE_RETURN);
		TX_BEGIN(pop) {
			pmemobj_tx_set_failure_behavior(POBJ_TX_FAILURE_ABORT);
			UT_ASSERTeq(pmemobj_tx_get_failure_behavior(),
				POBJ_TX_FAILURE_ABORT);
		} TX_ONCOMMIT {
			UT_ASSERTeq(ret, EINVAL);
		} TX_ONABORT {
			UT_ASSERT(0); /* should not get to this point */
		} TX_END

		TX_BEGIN(pop) {
			UT_ASSERTeq(pmemobj_tx_get_failure_behavior(),
				POBJ_TX_FAILURE_RETURN);
			ret = pmemobj_tx_free(oid);
		} TX_ONCOMMIT {
			UT_ASSERTeq(ret, EINVAL);
		} TX_ONABORT {
			UT_ASSERT(0); /* should not get to this point */
		} TX_END
	} TX_ONCOMMIT {
		UT_ASSERTeq(ret, EINVAL);
	} TX_ONABORT {
		UT_ASSERT(0); /* should not get to this point */
	} TX_END

	TOID(struct object) obj;
	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_FREE_WRONG_UUID));
	UT_ASSERT(!TOID_IS_NULL(obj));
}

/*
 * do_tx_free_null_oid -- call pmemobj_tx_free with OID_NULL
 */
static void
do_tx_free_null_oid(PMEMobjpool *pop)
{
	volatile int ret = 0;

	TX_BEGIN(pop) {
		ret = pmemobj_tx_free(OID_NULL);
	} TX_ONABORT {
		ret = -1;
	} TX_END

	UT_ASSERTeq(ret, 0);
}

/*
 * do_tx_free_commit -- do the basic transactional deallocation of object
 */
static void
do_tx_free_commit(PMEMobjpool *pop)
{
	int ret;
	PMEMoid oid = do_tx_alloc(pop, TYPE_FREE_COMMIT);

	TX_BEGIN(pop) {
		ret = pmemobj_tx_free(oid);
		UT_ASSERTeq(ret, 0);

	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TOID(struct object) obj;
	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_FREE_COMMIT));
	UT_ASSERT(TOID_IS_NULL(obj));
}

/*
 * do_tx_free_abort -- abort deallocation of object
 */
static void
do_tx_free_abort(PMEMobjpool *pop)
{
	int ret;
	PMEMoid oid = do_tx_alloc(pop, TYPE_FREE_ABORT);

	TX_BEGIN(pop) {
		ret = pmemobj_tx_free(oid);
		UT_ASSERTeq(ret, 0);

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TOID(struct object) obj;
	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_FREE_ABORT));
	UT_ASSERT(!TOID_IS_NULL(obj));
}

/*
 * do_tx_free_commit_nested -- do allocation in nested transaction
 */
static void
do_tx_free_commit_nested(PMEMobjpool *pop)
{
	int ret;
	PMEMoid oid1 = do_tx_alloc(pop, TYPE_FREE_COMMIT_NESTED1);
	PMEMoid oid2 = do_tx_alloc(pop, TYPE_FREE_COMMIT_NESTED2);

	TX_BEGIN(pop) {
		ret = pmemobj_tx_free(oid1);
		UT_ASSERTeq(ret, 0);

		TX_BEGIN(pop) {
			ret = pmemobj_tx_free(oid2);
			UT_ASSERTeq(ret, 0);

		} TX_ONABORT {
			UT_ASSERT(0);
		} TX_END
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TOID(struct object) obj;

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_FREE_COMMIT_NESTED1));
	UT_ASSERT(TOID_IS_NULL(obj));

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_FREE_COMMIT_NESTED2));
	UT_ASSERT(TOID_IS_NULL(obj));
}

/*
 * do_tx_free_abort_nested -- abort allocation in nested transaction
 */
static void
do_tx_free_abort_nested(PMEMobjpool *pop)
{
	int ret;
	PMEMoid oid1 = do_tx_alloc(pop, TYPE_FREE_ABORT_NESTED1);
	PMEMoid oid2 = do_tx_alloc(pop, TYPE_FREE_ABORT_NESTED2);

	TX_BEGIN(pop) {
		ret = pmemobj_tx_free(oid1);
		UT_ASSERTeq(ret, 0);

		TX_BEGIN(pop) {
			ret = pmemobj_tx_free(oid2);
			UT_ASSERTeq(ret, 0);

			pmemobj_tx_abort(-1);
		} TX_ONCOMMIT {
			UT_ASSERT(0);
		} TX_END
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TOID(struct object) obj;

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_FREE_ABORT_NESTED1));
	UT_ASSERT(!TOID_IS_NULL(obj));

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_FREE_ABORT_NESTED2));
	UT_ASSERT(!TOID_IS_NULL(obj));
}

/*
 * do_tx_free_abort_after_nested -- abort transaction after nested
 * pmemobj_tx_free
 */
static void
do_tx_free_abort_after_nested(PMEMobjpool *pop)
{
	int ret;
	PMEMoid oid1 = do_tx_alloc(pop, TYPE_FREE_ABORT_AFTER_NESTED1);
	PMEMoid oid2 = do_tx_alloc(pop, TYPE_FREE_ABORT_AFTER_NESTED2);

	TX_BEGIN(pop) {
		ret = pmemobj_tx_free(oid1);
		UT_ASSERTeq(ret, 0);

		TX_BEGIN(pop) {
			ret = pmemobj_tx_free(oid2);
			UT_ASSERTeq(ret, 0);
		} TX_END

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TOID(struct object) obj;

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop,
		TYPE_FREE_ABORT_AFTER_NESTED1));
	UT_ASSERT(!TOID_IS_NULL(obj));

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop,
		TYPE_FREE_ABORT_AFTER_NESTED2));
	UT_ASSERT(!TOID_IS_NULL(obj));
}

/*
 * do_tx_free_alloc_abort -- free object allocated in the same transaction
 * and abort transaction
 */
static void
do_tx_free_alloc_abort(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;

	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_alloc(
				sizeof(struct object), TYPE_FREE_ALLOC));
		UT_ASSERT(!TOID_IS_NULL(obj));
		ret = pmemobj_tx_free(obj.oid);
		UT_ASSERTeq(ret, 0);
		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_FREE_ALLOC));
	UT_ASSERT(TOID_IS_NULL(obj));
}

/*
 * do_tx_free_alloc_abort -- free object allocated in the same transaction
 * and commit transaction
 */
static void
do_tx_free_alloc_commit(PMEMobjpool *pop)
{
	int ret;
	TOID(struct object) obj;

	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_alloc(
				sizeof(struct object), TYPE_FREE_ALLOC));
		UT_ASSERT(!TOID_IS_NULL(obj));
		ret = pmemobj_tx_free(obj.oid);
		UT_ASSERTeq(ret, 0);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_FREE_ALLOC));
	UT_ASSERT(TOID_IS_NULL(obj));
}

/*
 * do_tx_free_abort_free - allocate a new object, perform a transactional free
 * in an aborted transaction and then to actually free the object.
 *
 * This can expose any issues with not properly handled free undo log.
 */
static void
do_tx_free_abort_free(PMEMobjpool *pop)
{
	PMEMoid oid = do_tx_alloc(pop, TYPE_FREE_AFTER_ABORT);

	TX_BEGIN(pop) {
		pmemobj_tx_free(oid);
		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TX_BEGIN(pop) {
		pmemobj_tx_free(oid);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
}

/*
 * do_tx_free_many_times -- free enough objects to trigger vector array alloc
 */
static void
do_tx_free_many_times(PMEMobjpool *pop)
{
#define TX_FREE_COUNT ((1 << 3) + 1)

	PMEMoid oids[TX_FREE_COUNT];
	for (int i = 0; i < TX_FREE_COUNT; ++i)
		oids[i] = do_tx_alloc(pop, TYPE_FREE_MANY_TIMES);

	TX_BEGIN(pop) {
		for (int i = 0; i < TX_FREE_COUNT; ++i)
			pmemobj_tx_free(oids[i]);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

#undef TX_FREE_COUNT
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_tx_free");
	util_init();

	if (argc != 2)
		UT_FATAL("usage: %s [file]", argv[0]);

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(argv[1], LAYOUT_NAME, PMEMOBJ_MIN_POOL,
	    S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create");

	do_tx_free_wrong_uuid(pop);
	VALGRIND_WRITE_STATS;
	do_tx_free_wrong_uuid_abort_on_failure(pop);
	VALGRIND_WRITE_STATS;
	do_tx_free_null_oid(pop);
	VALGRIND_WRITE_STATS;
	do_tx_free_commit(pop);
	VALGRIND_WRITE_STATS;
	do_tx_free_abort(pop);
	VALGRIND_WRITE_STATS;
	do_tx_free_commit_nested(pop);
	VALGRIND_WRITE_STATS;
	do_tx_free_abort_nested(pop);
	VALGRIND_WRITE_STATS;
	do_tx_free_abort_after_nested(pop);
	VALGRIND_WRITE_STATS;
	do_tx_free_alloc_commit(pop);
	VALGRIND_WRITE_STATS;
	do_tx_free_alloc_abort(pop);
	VALGRIND_WRITE_STATS;
	do_tx_free_abort_free(pop);
	VALGRIND_WRITE_STATS;
	do_tx_free_many_times(pop);
	VALGRIND_WRITE_STATS;

	pmemobj_close(pop);

	DONE(NULL);
}
