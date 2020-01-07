// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * obj_tx_locks_nested.c -- unit test for transaction locks
 */
#include "unittest.h"

#define LAYOUT_NAME "locks"

TOID_DECLARE_ROOT(struct root_obj);
TOID_DECLARE(struct obj, 1);

struct root_obj {
	PMEMmutex lock;
	TOID(struct obj) head;
};

struct obj {
	int data;
	PMEMmutex lock;
	TOID(struct obj) next;
};

/*
 * do_nested_tx-- (internal) nested transaction
 */
static void
do_nested_tx(PMEMobjpool *pop, TOID(struct obj) o, int value)
{
	TX_BEGIN_PARAM(pop, TX_PARAM_MUTEX, &D_RW(o)->lock, TX_PARAM_NONE) {
		TX_ADD(o);
		D_RW(o)->data = value;
		if (!TOID_IS_NULL(D_RO(o)->next)) {
			/*
			 * Add the object to undo log, while the mutex
			 * it contains is not locked.
			 */
			TX_ADD(D_RO(o)->next);
			do_nested_tx(pop, D_RO(o)->next, value);
		}
	} TX_END;
}

/*
 * do_aborted_nested_tx -- (internal) aborted nested transaction
 */
static void
do_aborted_nested_tx(PMEMobjpool *pop, TOID(struct obj) oid, int value)
{
	TOID(struct obj) o = oid;

	TX_BEGIN_PARAM(pop, TX_PARAM_MUTEX, &D_RW(o)->lock, TX_PARAM_NONE) {
		TX_ADD(o);
		D_RW(o)->data = value;
		if (!TOID_IS_NULL(D_RO(o)->next)) {
			/*
			 * Add the object to undo log, while the mutex
			 * it contains is not locked.
			 */
			TX_ADD(D_RO(o)->next);
			do_nested_tx(pop, D_RO(o)->next, value);
		}
		pmemobj_tx_abort(EINVAL);
	} TX_FINALLY {
		o = oid;

		while (!TOID_IS_NULL(o)) {
			if (pmemobj_mutex_trylock(pop, &D_RW(o)->lock)) {
				UT_OUT("trylock failed");
			} else {
				UT_OUT("trylock succeeded");
				pmemobj_mutex_unlock(pop, &D_RW(o)->lock);
			}
			o = D_RO(o)->next;
		}
	} TX_END;
}

/*
 * do_check -- (internal) print 'data' value of each object on the list
 */
static void
do_check(TOID(struct obj) o)
{
	while (!TOID_IS_NULL(o)) {
		UT_OUT("data = %d", D_RO(o)->data);
		o = D_RO(o)->next;
	}
}

int
main(int argc, char *argv[])
{
	PMEMobjpool *pop;

	START(argc, argv, "obj_tx_locks_abort");

	if (argc > 3)
		UT_FATAL("usage: %s <file>", argv[0]);

	pop = pmemobj_create(argv[1], LAYOUT_NAME,
			PMEMOBJ_MIN_POOL * 4, S_IWUSR | S_IRUSR);
	if (pop == NULL)
		UT_FATAL("!pmemobj_create");

	TOID(struct root_obj) root = POBJ_ROOT(pop, struct root_obj);

	TX_BEGIN_PARAM(pop, TX_PARAM_MUTEX, &D_RW(root)->lock) {
		TX_ADD(root);
		D_RW(root)->head = TX_ZNEW(struct obj);
		TOID(struct obj) o;
		o = D_RW(root)->head;
		D_RW(o)->data = 100;
		pmemobj_mutex_zero(pop, &D_RW(o)->lock);
		for (int i = 0; i < 3; i++) {
			D_RW(o)->next = TX_ZNEW(struct obj);
			o = D_RO(o)->next;
			D_RW(o)->data = 101 + i;
			pmemobj_mutex_zero(pop, &D_RW(o)->lock);
		}
		TOID_ASSIGN(D_RW(o)->next, OID_NULL);
	} TX_END;

	UT_OUT("initial state");
	do_check(D_RO(root)->head);

	UT_OUT("nested tx");
	do_nested_tx(pop, D_RW(root)->head, 200);
	do_check(D_RO(root)->head);

	UT_OUT("aborted nested tx");
	do_aborted_nested_tx(pop, D_RW(root)->head, 300);
	do_check(D_RO(root)->head);

	pmemobj_close(pop);

	DONE(NULL);
}
