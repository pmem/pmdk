// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */

/*
 * obj_tx_realloc.c -- unit test for pmemobj_tx_realloc and pmemobj_tx_zrealloc
 */
#include <sys/param.h>
#include <string.h>

#include "unittest.h"
#include "util.h"

#define LAYOUT_NAME "tx_realloc"

#define TEST_VALUE_1	1
#define OBJ_SIZE	1024

enum type_number {
	TYPE_NO_TX,
	TYPE_COMMIT,
	TYPE_ABORT,
	TYPE_TYPE,
	TYPE_COMMIT_ZERO,
	TYPE_COMMIT_ZERO_MACRO,
	TYPE_ABORT_ZERO,
	TYPE_ABORT_ZERO_MACRO,
	TYPE_COMMIT_ALLOC,
	TYPE_ABORT_ALLOC,
	TYPE_ABORT_HUGE,
	TYPE_ABORT_ZERO_HUGE,
	TYPE_ABORT_ZERO_HUGE_MACRO,
	TYPE_FREE,
};

struct object {
	size_t value;
	char data[OBJ_SIZE - sizeof(size_t)];
};

TOID_DECLARE(struct object, 0);

struct object_macro {
	size_t value;
	char data[OBJ_SIZE - sizeof(size_t)];
};

TOID_DECLARE(struct object_macro, TYPE_COMMIT_ZERO_MACRO);

/*
 * do_tx_alloc -- do tx allocation with specified type number
 */
static PMEMoid
do_tx_alloc(PMEMobjpool *pop, unsigned type_num, size_t value)
{
	TOID(struct object) obj;
	TOID_ASSIGN(obj, OID_NULL);

	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_alloc(
				sizeof(struct object), type_num));
		if (!TOID_IS_NULL(obj)) {
			D_RW(obj)->value = value;
		}
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	return obj.oid;
}

/*
 * do_tx_realloc_commit -- reallocate an object and commit the transaction
 */
static void
do_tx_realloc_commit(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_alloc(pop, TYPE_COMMIT, TEST_VALUE_1));
	size_t new_size = 2 * pmemobj_alloc_usable_size(obj.oid);

	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_realloc(obj.oid,
			new_size, TYPE_COMMIT));
		UT_ASSERT(!TOID_IS_NULL(obj));
		UT_ASSERT(pmemobj_alloc_usable_size(obj.oid) >= new_size);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_COMMIT));
	UT_ASSERT(!TOID_IS_NULL(obj));
	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	UT_ASSERT(pmemobj_alloc_usable_size(obj.oid) >= new_size);

	TOID_ASSIGN(obj, POBJ_NEXT_TYPE_NUM(obj.oid));
	UT_ASSERT(TOID_IS_NULL(obj));
}

/*
 * do_tx_realloc_abort -- reallocate an object and commit the transaction
 */
static void
do_tx_realloc_abort(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_alloc(pop, TYPE_ABORT, TEST_VALUE_1));
	size_t new_size = 2 * pmemobj_alloc_usable_size(obj.oid);

	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_realloc(obj.oid,
			new_size, TYPE_ABORT));
		UT_ASSERT(!TOID_IS_NULL(obj));
		UT_ASSERT(pmemobj_alloc_usable_size(obj.oid) >= new_size);

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT));
	UT_ASSERT(!TOID_IS_NULL(obj));
	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	UT_ASSERT(pmemobj_alloc_usable_size(obj.oid) < new_size);

	TOID_ASSIGN(obj, POBJ_NEXT_TYPE_NUM(obj.oid));
	UT_ASSERT(TOID_IS_NULL(obj));
}

/*
 * do_tx_realloc_huge -- reallocate an object to a huge size to trigger tx abort
 */
static void
do_tx_realloc_huge(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_alloc(pop, TYPE_ABORT_HUGE, TEST_VALUE_1));
	size_t new_size = PMEMOBJ_MAX_ALLOC_SIZE + 1;

	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_realloc(obj.oid,
			new_size, TYPE_ABORT_HUGE));
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT_HUGE));
	UT_ASSERT(!TOID_IS_NULL(obj));
	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	UT_ASSERT(pmemobj_alloc_usable_size(obj.oid) < new_size);

	TOID_ASSIGN(obj, POBJ_NEXT_TYPE_NUM(obj.oid));
	UT_ASSERT(TOID_IS_NULL(obj));
}

/*
 * do_tx_zrealloc_commit_macro -- reallocate an object, zero it and commit
 * the transaction using macro
 */
static void
do_tx_zrealloc_commit_macro(PMEMobjpool *pop)
{
	TOID(struct object_macro) obj;
	TOID_ASSIGN(obj, do_tx_alloc(pop, TYPE_COMMIT_ZERO_MACRO,
								TEST_VALUE_1));
	size_t old_size = pmemobj_alloc_usable_size(obj.oid);
	size_t new_size = 2 * old_size;

	TX_BEGIN(pop) {
		obj = TX_ZREALLOC(obj, new_size);
		UT_ASSERT(!TOID_IS_NULL(obj));
		UT_ASSERT(pmemobj_alloc_usable_size(obj.oid) >= new_size);
		void *new_ptr = (void *)((uintptr_t)D_RW(obj) + old_size);
		UT_ASSERT(util_is_zeroed(new_ptr, new_size - old_size));
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_COMMIT_ZERO_MACRO));
	UT_ASSERT(!TOID_IS_NULL(obj));
	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	UT_ASSERT(pmemobj_alloc_usable_size(obj.oid) >= new_size);
	void *new_ptr = (void *)((uintptr_t)D_RW(obj) + old_size);
	UT_ASSERT(util_is_zeroed(new_ptr, new_size - old_size));

	TOID_ASSIGN(obj, POBJ_NEXT_TYPE_NUM(obj.oid));
	UT_ASSERT(TOID_IS_NULL(obj));
}

/*
 * do_tx_zrealloc_commit -- reallocate an object, zero it and commit
 * the transaction
 */
static void
do_tx_zrealloc_commit(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_alloc(pop, TYPE_COMMIT_ZERO, TEST_VALUE_1));
	size_t old_size = pmemobj_alloc_usable_size(obj.oid);
	size_t new_size = 2 * old_size;

	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_zrealloc(obj.oid,
			new_size, TYPE_COMMIT_ZERO));
		UT_ASSERT(!TOID_IS_NULL(obj));
		UT_ASSERT(pmemobj_alloc_usable_size(obj.oid) >= new_size);
		void *new_ptr = (void *)((uintptr_t)D_RW(obj) + old_size);
		UT_ASSERT(util_is_zeroed(new_ptr, new_size - old_size));
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_COMMIT_ZERO));
	UT_ASSERT(!TOID_IS_NULL(obj));
	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	UT_ASSERT(pmemobj_alloc_usable_size(obj.oid) >= new_size);
	void *new_ptr = (void *)((uintptr_t)D_RW(obj) + old_size);
	UT_ASSERT(util_is_zeroed(new_ptr, new_size - old_size));

	TOID_ASSIGN(obj, POBJ_NEXT_TYPE_NUM(obj.oid));
	UT_ASSERT(TOID_IS_NULL(obj));
}

/*
 * do_tx_realloc_abort_macro -- reallocate an object, zero it and commit the
 * transaction using macro
 */
static void
do_tx_zrealloc_abort_macro(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_alloc(pop, TYPE_ABORT_ZERO_MACRO, TEST_VALUE_1));
	size_t old_size = pmemobj_alloc_usable_size(obj.oid);
	size_t new_size = 2 * old_size;

	TX_BEGIN(pop) {
		obj = TX_ZREALLOC(obj, new_size);
		UT_ASSERT(!TOID_IS_NULL(obj));
		UT_ASSERT(pmemobj_alloc_usable_size(obj.oid) >= new_size);
		void *new_ptr = (void *)((uintptr_t)D_RW(obj) + old_size);
		UT_ASSERT(util_is_zeroed(new_ptr, new_size - old_size));

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT_ZERO_MACRO));
	UT_ASSERT(!TOID_IS_NULL(obj));
	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	UT_ASSERT(pmemobj_alloc_usable_size(obj.oid) < new_size);
	TOID_ASSIGN(obj, POBJ_NEXT_TYPE_NUM(obj.oid));
	UT_ASSERT(TOID_IS_NULL(obj));
}

/*
 * do_tx_realloc_abort -- reallocate an object and commit the transaction
 */
static void
do_tx_zrealloc_abort(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_alloc(pop, TYPE_ABORT_ZERO, TEST_VALUE_1));
	size_t old_size = pmemobj_alloc_usable_size(obj.oid);
	size_t new_size = 2 * old_size;

	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_zrealloc(obj.oid,
			new_size, TYPE_ABORT_ZERO));
		UT_ASSERT(!TOID_IS_NULL(obj));
		UT_ASSERT(pmemobj_alloc_usable_size(obj.oid) >= new_size);
		void *new_ptr = (void *)((uintptr_t)D_RW(obj) + old_size);
		UT_ASSERT(util_is_zeroed(new_ptr, new_size - old_size));

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT_ZERO));
	UT_ASSERT(!TOID_IS_NULL(obj));
	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	UT_ASSERT(pmemobj_alloc_usable_size(obj.oid) < new_size);
	TOID_ASSIGN(obj, POBJ_NEXT_TYPE_NUM(obj.oid));
	UT_ASSERT(TOID_IS_NULL(obj));
}

/*
 * do_tx_realloc_huge_macro -- reallocate an object to a huge size to trigger
 * tx abort and zero it using macro
 */
static void
do_tx_zrealloc_huge_macro(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_alloc(pop, TYPE_ABORT_ZERO_HUGE_MACRO,
								TEST_VALUE_1));
	size_t old_size = pmemobj_alloc_usable_size(obj.oid);
	size_t new_size = 2 * old_size;

	TX_BEGIN(pop) {
		obj = TX_ZREALLOC(obj, PMEMOBJ_MAX_ALLOC_SIZE + 1);
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT_ZERO_HUGE_MACRO));
	UT_ASSERT(!TOID_IS_NULL(obj));
	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	UT_ASSERT(pmemobj_alloc_usable_size(obj.oid) < new_size);

	TOID_ASSIGN(obj, POBJ_NEXT_TYPE_NUM(obj.oid));
	UT_ASSERT(TOID_IS_NULL(obj));
}

/*
 * do_tx_realloc_huge -- reallocate an object to a huge size to trigger tx abort
 */
static void
do_tx_zrealloc_huge(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_alloc(pop, TYPE_ABORT_ZERO_HUGE, TEST_VALUE_1));
	size_t old_size = pmemobj_alloc_usable_size(obj.oid);
	size_t new_size = 2 * old_size;

	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_zrealloc(obj.oid,
			PMEMOBJ_MAX_ALLOC_SIZE + 1, TYPE_ABORT_ZERO_HUGE));
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT_ZERO_HUGE));
	UT_ASSERT(!TOID_IS_NULL(obj));
	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	UT_ASSERT(pmemobj_alloc_usable_size(obj.oid) < new_size);

	TOID_ASSIGN(obj, POBJ_NEXT_TYPE_NUM(obj.oid));
	UT_ASSERT(TOID_IS_NULL(obj));
}

/*
 * do_tx_realloc_alloc_commit -- reallocate an allocated object
 * and commit the transaction
 */
static void
do_tx_realloc_alloc_commit(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	size_t new_size = 0;

	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, do_tx_alloc(pop, TYPE_COMMIT_ALLOC,
					TEST_VALUE_1));
		UT_ASSERT(!TOID_IS_NULL(obj));
		new_size = 2 * pmemobj_alloc_usable_size(obj.oid);
		TOID_ASSIGN(obj, pmemobj_tx_realloc(obj.oid,
			new_size, TYPE_COMMIT_ALLOC));
		UT_ASSERT(!TOID_IS_NULL(obj));
		UT_ASSERT(pmemobj_alloc_usable_size(obj.oid) >= new_size);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_COMMIT_ALLOC));
	UT_ASSERT(!TOID_IS_NULL(obj));
	UT_ASSERTeq(D_RO(obj)->value, TEST_VALUE_1);
	UT_ASSERT(pmemobj_alloc_usable_size(obj.oid) >= new_size);

	TOID_ASSIGN(obj, POBJ_NEXT_TYPE_NUM(obj.oid));
	UT_ASSERT(TOID_IS_NULL(obj));
}

/*
 * do_tx_realloc_alloc_abort -- reallocate an allocated object
 * and commit the transaction
 */
static void
do_tx_realloc_alloc_abort(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	size_t new_size = 0;

	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, do_tx_alloc(pop, TYPE_ABORT_ALLOC,
					TEST_VALUE_1));
		UT_ASSERT(!TOID_IS_NULL(obj));
		new_size = 2 * pmemobj_alloc_usable_size(obj.oid);
		TOID_ASSIGN(obj, pmemobj_tx_realloc(obj.oid,
			new_size, TYPE_ABORT_ALLOC));
		UT_ASSERT(!TOID_IS_NULL(obj));
		UT_ASSERT(pmemobj_alloc_usable_size(obj.oid) >= new_size);

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_ABORT_ALLOC));
	UT_ASSERT(TOID_IS_NULL(obj));
}

/*
 * do_tx_root_realloc -- retrieve root inside of transaction
 */
static void
do_tx_root_realloc(PMEMobjpool *pop)
{
	TX_BEGIN(pop) {
		PMEMoid root = pmemobj_root(pop, sizeof(struct object));
		UT_ASSERT(!OID_IS_NULL(root));
		UT_ASSERT(util_is_zeroed(pmemobj_direct(root),
				sizeof(struct object)));
		UT_ASSERTeq(sizeof(struct object), pmemobj_root_size(pop));

		root = pmemobj_root(pop, 2 * sizeof(struct object));
		UT_ASSERT(!OID_IS_NULL(root));
		UT_ASSERT(util_is_zeroed(pmemobj_direct(root),
				2 * sizeof(struct object)));
		UT_ASSERTeq(2 * sizeof(struct object), pmemobj_root_size(pop));
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
}

/*
 * do_tx_realloc_free -- reallocate an allocated object
 * and commit the transaction
 */
static void
do_tx_realloc_free(PMEMobjpool *pop)
{
	TOID(struct object) obj;
	TOID_ASSIGN(obj, do_tx_alloc(pop, TYPE_FREE, TEST_VALUE_1));

	TX_BEGIN(pop) {
		TOID_ASSIGN(obj, pmemobj_tx_realloc(obj.oid,
			0, TYPE_COMMIT));
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TOID_ASSIGN(obj, POBJ_FIRST_TYPE_NUM(pop, TYPE_FREE));
	UT_ASSERT(TOID_IS_NULL(obj));
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_tx_realloc");

	if (argc != 2)
		UT_FATAL("usage: %s [file]", argv[0]);

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(argv[1], LAYOUT_NAME, 0,
				S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create");

	do_tx_root_realloc(pop);
	do_tx_realloc_commit(pop);
	do_tx_realloc_abort(pop);
	do_tx_realloc_huge(pop);
	do_tx_zrealloc_commit(pop);
	do_tx_zrealloc_commit_macro(pop);
	do_tx_zrealloc_abort(pop);
	do_tx_zrealloc_abort_macro(pop);
	do_tx_zrealloc_huge(pop);
	do_tx_zrealloc_huge_macro(pop);
	do_tx_realloc_alloc_commit(pop);
	do_tx_realloc_alloc_abort(pop);
	do_tx_realloc_free(pop);

	pmemobj_close(pop);

	DONE(NULL);
}
