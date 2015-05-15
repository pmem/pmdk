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
 * obj_tx_alloc.c -- unit test for pmemobj_tx_alloc and pmemobj_tx_zalloc
 */
#include <sys/param.h>
#include <string.h>

#include "unittest.h"
#include "libpmemobj.h"
#include "util.h"
#include "valgrind_internal.h"

#define	LAYOUT_NAME "tx_alloc"

#define	TEST_VALUE_1	1
#define	TEST_VALUE_2	2
#define	OBJ_SIZE	1024

enum type_number {
	TYPE_NO_TX,
	TYPE_COMMIT,
	TYPE_ABORT,
	TYPE_COMMIT_NESTED1,
	TYPE_COMMIT_NESTED2,
	TYPE_ABORT_NESTED1,
	TYPE_ABORT_NESTED2,
	TYPE_ABORT_AFTER_NESTED1,
	TYPE_ABORT_AFTER_NESTED2,
	TYPE_OOM,
};

struct object {
	size_t value;
	char data[OBJ_SIZE - sizeof (size_t)];
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
			OID_TYPE(struct object) obj;
			OID_ASSIGN(obj, pmemobj_tx_alloc(sizeof (struct object),
						TYPE_OOM));
			if (OID_IS_NULL(obj)) {
				pmemobj_tx_abort(-ENOMEM);
			} else {
				D_RW(obj)->value = alloc_cnt;
			}
		} TX_ONCOMMIT {
			alloc_cnt++;
		} TX_ONABORT {
			do_alloc = 0;
		} TX_END
	} while (do_alloc);

	size_t bitmap_size = howmany(alloc_cnt, 8);
	char *bitmap = malloc(bitmap_size);
	memset(bitmap, 0, bitmap_size);

	size_t obj_cnt = 0;
	OID_TYPE(struct object) i;
	POBJ_FOREACH_TYPE(pop, i, TYPE_OOM) {
		ASSERT(D_RO(i)->value < alloc_cnt);
		ASSERT(!isset(bitmap, D_RO(i)->value));
		setbit(bitmap, D_RO(i)->value);
		obj_cnt++;
	}

	free(bitmap);

	ASSERTeq(obj_cnt, alloc_cnt);
}

/*
 * do_tx_alloc_abort_after_nested -- aborts transaction after allocation
 * in nested transaction
 */
static void
do_tx_alloc_abort_after_nested(PMEMobjpool *pop)
{
	OID_TYPE(struct object) obj1;
	OID_TYPE(struct object) obj2;

	TX_BEGIN(pop) {
		OID_ASSIGN(obj1, pmemobj_tx_alloc(sizeof (struct object),
				TYPE_ABORT_AFTER_NESTED1));
		ASSERT(!OID_IS_NULL(obj1));

		D_RW(obj1)->value = TEST_VALUE_1;

		TX_BEGIN(pop) {
			OID_ASSIGN(obj2, pmemobj_tx_zalloc(
					sizeof (struct object),
					TYPE_ABORT_AFTER_NESTED2));
			ASSERT(!OID_IS_NULL(obj2));
			ASSERT(util_is_zeroed(D_RO(obj2),
					sizeof (struct object)));

			D_RW(obj2)->value = TEST_VALUE_2;

		} TX_ONCOMMIT {
			ASSERTeq(D_RO(obj2)->value, TEST_VALUE_2);
		} TX_ONABORT {
			ASSERT(0);
		} TX_END

		pmemobj_tx_abort(-1);

	} TX_ONCOMMIT {
		ASSERT(0);
	} TX_ONABORT {
		OID_ASSIGN(obj1, OID_NULL);
		OID_ASSIGN(obj2, OID_NULL);
	} TX_END

	OID_TYPE(struct object) first;

	/* check the obj1 object */
	ASSERT(OID_IS_NULL(obj1));

	first.oid = pmemobj_first(pop, TYPE_ABORT_AFTER_NESTED1);
	ASSERT(OID_IS_NULL(first));

	/* check the obj2 object */
	ASSERT(OID_IS_NULL(obj2));

	first.oid = pmemobj_first(pop, TYPE_ABORT_AFTER_NESTED2);
	ASSERT(OID_IS_NULL(first));
}

/*
 * do_tx_alloc_abort_nested -- aborts transaction in nested transaction
 */
static void
do_tx_alloc_abort_nested(PMEMobjpool *pop)
{
	OID_TYPE(struct object) obj1;
	OID_TYPE(struct object) obj2;

	TX_BEGIN(pop) {
		OID_ASSIGN(obj1, pmemobj_tx_alloc(sizeof (struct object),
				TYPE_ABORT_NESTED1));
		ASSERT(!OID_IS_NULL(obj1));

		D_RW(obj1)->value = TEST_VALUE_1;

		TX_BEGIN(pop) {
			OID_ASSIGN(obj2, pmemobj_tx_zalloc(
					sizeof (struct object),
					TYPE_ABORT_NESTED2));
			ASSERT(!OID_IS_NULL(obj2));
			ASSERT(util_is_zeroed(D_RO(obj2),
					sizeof (struct object)));

			D_RW(obj2)->value = TEST_VALUE_2;

			pmemobj_tx_abort(-1);
		} TX_ONCOMMIT {
			ASSERT(0);
		} TX_ONABORT {
			OID_ASSIGN(obj2, OID_NULL);
		} TX_END

	} TX_ONCOMMIT {
		ASSERT(0);
	} TX_ONABORT {
		OID_ASSIGN(obj1, OID_NULL);
	} TX_END

	OID_TYPE(struct object) first;

	/* check the obj1 object */
	ASSERT(OID_IS_NULL(obj1));

	first.oid = pmemobj_first(pop, TYPE_ABORT_NESTED1);
	ASSERT(OID_IS_NULL(first));

	/* check the obj2 object */
	ASSERT(OID_IS_NULL(obj2));

	first.oid = pmemobj_first(pop, TYPE_ABORT_NESTED2);
	ASSERT(OID_IS_NULL(first));
}

/*
 * do_tx_alloc_commit_nested -- allocates two objects, one in nested transaction
 */
static void
do_tx_alloc_commit_nested(PMEMobjpool *pop)
{
	OID_TYPE(struct object) obj1;
	OID_TYPE(struct object) obj2;

	TX_BEGIN(pop) {
		OID_ASSIGN(obj1, pmemobj_tx_alloc(sizeof (struct object),
				TYPE_COMMIT_NESTED1));
		ASSERT(!OID_IS_NULL(obj1));

		D_RW(obj1)->value = TEST_VALUE_1;

		TX_BEGIN(pop) {
			OID_ASSIGN(obj2, pmemobj_tx_zalloc(
					sizeof (struct object),
					TYPE_COMMIT_NESTED2));
			ASSERT(!OID_IS_NULL(obj2));
			ASSERT(util_is_zeroed(D_RO(obj2),
					sizeof (struct object)));

			D_RW(obj2)->value = TEST_VALUE_2;
		} TX_ONCOMMIT {
			ASSERTeq(D_RO(obj1)->value, TEST_VALUE_1);
			ASSERTeq(D_RO(obj2)->value, TEST_VALUE_2);
		} TX_ONABORT {
			ASSERT(0);
		} TX_END

	} TX_ONCOMMIT {
		ASSERTeq(D_RO(obj1)->value, TEST_VALUE_1);
		ASSERTeq(D_RO(obj2)->value, TEST_VALUE_2);
	} TX_ONABORT {
		ASSERT(0);
	} TX_END

	OID_TYPE(struct object) first;
	OID_TYPE(struct object) next;

	/* check the obj1 object */
	OID_ASSIGN(first, pmemobj_first(pop, TYPE_COMMIT_NESTED1));
	ASSERT(OID_EQUALS(first, obj1));
	ASSERTeq(D_RO(first)->value, TEST_VALUE_1);

	OID_ASSIGN(next, pmemobj_next(first.oid));
	ASSERT(OID_IS_NULL(next));

	/* check the obj2 object */
	OID_ASSIGN(first, pmemobj_first(pop, TYPE_COMMIT_NESTED2));
	ASSERT(OID_EQUALS(first, obj2));
	ASSERTeq(D_RO(first)->value, TEST_VALUE_2);

	OID_ASSIGN(next, pmemobj_next(first.oid));
	ASSERT(OID_IS_NULL(next));
}

/*
 * do_tx_alloc_abort -- allocates an object and aborts the transaction
 */
static void
do_tx_alloc_abort(PMEMobjpool *pop)
{
	OID_TYPE(struct object) obj;
	TX_BEGIN(pop) {
		OID_ASSIGN(obj, pmemobj_tx_alloc(sizeof (struct object),
				TYPE_ABORT));
		ASSERT(!OID_IS_NULL(obj));

		D_RW(obj)->value = TEST_VALUE_1;
		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		ASSERT(0);
	} TX_ONABORT {
		OID_ASSIGN(obj, OID_NULL);
	} TX_END

	ASSERT(OID_IS_NULL(obj));

	OID_TYPE(struct object) first;
	OID_ASSIGN(first, pmemobj_first(pop, TYPE_ABORT));
	ASSERT(OID_IS_NULL(first));
}

/*
 * do_tx_alloc_commit -- allocates and object
 */
static void
do_tx_alloc_commit(PMEMobjpool *pop)
{
	OID_TYPE(struct object) obj;
	TX_BEGIN(pop) {
		OID_ASSIGN(obj, pmemobj_tx_alloc(sizeof (struct object),
				TYPE_COMMIT));
		ASSERT(!OID_IS_NULL(obj));

		D_RW(obj)->value = TEST_VALUE_1;
	} TX_ONCOMMIT {
		ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	} TX_ONABORT {
		ASSERT(0);
	} TX_END

	OID_TYPE(struct object) first;
	OID_ASSIGN(first, pmemobj_first(pop, TYPE_COMMIT));
	ASSERT(OID_EQUALS(first, obj));
	ASSERTeq(D_RO(first)->value, D_RO(obj)->value);

	OID_TYPE(struct object) next;
	OID_ASSIGN(next, pmemobj_next(first.oid));
	ASSERT(OID_IS_NULL(next));
}

/*
 * do_tx_alloc_no_tx -- allocates object without transaction
 */
static void
do_tx_alloc_no_tx(PMEMobjpool *pop)
{
	OID_TYPE(struct object) obj;
	OID_ASSIGN(obj, pmemobj_tx_alloc(sizeof (struct object), TYPE_NO_TX));
	ASSERT(OID_IS_NULL(obj));
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_tx_alloc");

	if (argc != 2)
		FATAL("usage: %s [file]", argv[0]);

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(argv[1], LAYOUT_NAME, PMEMOBJ_MIN_POOL,
			S_IRWXU)) == NULL)
		FATAL("!pmemobj_create");

	do_tx_alloc_no_tx(pop);
	VALGRIND_WRITE_STATS;
	do_tx_alloc_commit(pop);
	VALGRIND_WRITE_STATS;
	do_tx_alloc_abort(pop);
	VALGRIND_WRITE_STATS;
	do_tx_alloc_commit_nested(pop);
	VALGRIND_WRITE_STATS;
	do_tx_alloc_abort_nested(pop);
	VALGRIND_WRITE_STATS;
	do_tx_alloc_abort_after_nested(pop);
	VALGRIND_WRITE_STATS;
	do_tx_alloc_oom(pop);
	VALGRIND_WRITE_STATS;

	pmemobj_close(pop);

	DONE(NULL);
}
