// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */

/*
 * obj_recovery.c -- unit test for pool recovery
 */
#include "unittest.h"
#include "valgrind_internal.h"
#if VG_PMEMCHECK_ENABLED
#define VALGRIND_PMEMCHECK_END_TX VALGRIND_PMC_END_TX
#else
#define VALGRIND_PMEMCHECK_END_TX
#endif

POBJ_LAYOUT_BEGIN(recovery);
POBJ_LAYOUT_ROOT(recovery, struct root);
POBJ_LAYOUT_TOID(recovery, struct foo);
POBJ_LAYOUT_END(recovery);

#define MB (1 << 20)

struct foo {
	int bar;
};

struct root {
	PMEMmutex lock;
	TOID(struct foo) foo;
	char large_data[MB];
};

#define BAR_VALUE 5

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_recovery");

	/* root doesn't count */
	UT_COMPILE_ERROR_ON(POBJ_LAYOUT_TYPES_NUM(recovery) != 1);

	if (argc != 5)
		UT_FATAL("usage: %s [file] [lock: y/n] "
			"[cmd: c/o] [type: n/f/s/l]",
			argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop = NULL;
	int exists = argv[3][0] == 'o';
	enum { TEST_NEW, TEST_FREE, TEST_SET, TEST_LARGE } type;

	if (argv[4][0] == 'n')
		type = TEST_NEW;
	else if (argv[4][0] == 'f')
		type = TEST_FREE;
	else if (argv[4][0] == 's')
		type = TEST_SET;
	else if (argv[4][0] == 'l')
		type = TEST_LARGE;
	else
		UT_FATAL("invalid type");

	if (!exists) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(recovery),
			0, S_IWUSR | S_IRUSR)) == NULL) {
			UT_FATAL("failed to create pool\n");
		}
	} else {
		if ((pop = pmemobj_open(path, POBJ_LAYOUT_NAME(recovery)))
						== NULL) {
			UT_FATAL("failed to open pool\n");
		}
	}

	TOID(struct root) root = POBJ_ROOT(pop, struct root);

	int lock_type = TX_PARAM_NONE;
	void *lock = NULL;

	if (argv[2][0] == 'y') {
		lock_type = TX_PARAM_MUTEX;
		lock = &D_RW(root)->lock;
	}

	if (type == TEST_SET) {
		if (!exists) {
			TX_BEGIN_PARAM(pop, lock_type, lock) {
				TX_ADD(root);

				TOID(struct foo) f = TX_NEW(struct foo);
				D_RW(root)->foo = f;
				D_RW(f)->bar = BAR_VALUE;
			} TX_END

			TX_BEGIN_PARAM(pop, lock_type, lock) {
				TX_ADD_FIELD(D_RW(root)->foo, bar);

				D_RW(D_RW(root)->foo)->bar = BAR_VALUE * 2;

				/*
				 * Even though flushes are not required inside
				 * of a transaction, this is done here to
				 * suppress irrelevant pmemcheck issues, because
				 * we exit the program before the data is
				 * flushed, while preserving any real ones.
				 */
				pmemobj_persist(pop,
					&D_RW(D_RW(root)->foo)->bar,
					sizeof(int));
				/*
				 * We also need to cleanup the transaction state
				 * of pmemcheck.
				 */
				VALGRIND_PMEMCHECK_END_TX;

				exit(0); /* simulate a crash */
			} TX_END
		} else {
			UT_ASSERT(D_RW(D_RW(root)->foo)->bar == BAR_VALUE);
		}
	} else if (type == TEST_LARGE) {
		if (!exists) {
			TX_BEGIN(pop) {
				TX_MEMSET(D_RW(root)->large_data, 0xc, MB);
				pmemobj_persist(pop,
					D_RW(root)->large_data, MB);
				VALGRIND_PMEMCHECK_END_TX;

				exit(0);
			} TX_END
		} else {
			UT_ASSERT(util_is_zeroed(D_RW(root)->large_data, MB));

			TX_BEGIN(pop) { /* we should be able to start TX */
				TX_MEMSET(D_RW(root)->large_data, 0xc, MB);
				pmemobj_persist(pop,
					D_RW(root)->large_data, MB);
				VALGRIND_PMEMCHECK_END_TX;

				pmemobj_tx_abort(0);
			} TX_END
		}
	} else if (type == TEST_NEW) {
		if (!exists) {
			TX_BEGIN_PARAM(pop, lock_type, lock) {
				TOID(struct foo) f = TX_NEW(struct foo);
				TX_SET(root, foo, f);
				pmemobj_persist(pop,
					&D_RW(root)->foo,
					sizeof(PMEMoid));
				VALGRIND_PMEMCHECK_END_TX;

				exit(0); /* simulate a crash */
			} TX_END

		} else {
			UT_ASSERT(TOID_IS_NULL(D_RW(root)->foo));
		}
	} else { /* TEST_FREE */
		if (!exists) {
			TX_BEGIN_PARAM(pop, lock_type, lock) {
				TX_ADD(root);

				TOID(struct foo) f = TX_NEW(struct foo);
				D_RW(root)->foo = f;
				D_RW(f)->bar = BAR_VALUE;
			} TX_END

			TX_BEGIN_PARAM(pop, lock_type, lock) {
				TX_ADD(root);
				TX_FREE(D_RW(root)->foo);
				D_RW(root)->foo = TOID_NULL(struct foo);
				pmemobj_persist(pop,
					&D_RW(root)->foo,
					sizeof(PMEMoid));
				VALGRIND_PMEMCHECK_END_TX;

				exit(0); /* simulate a crash */
			} TX_END

		} else {
			UT_ASSERT(!TOID_IS_NULL(D_RW(root)->foo));
		}
	}

	UT_ASSERT(pmemobj_check(path, POBJ_LAYOUT_NAME(recovery)));

	pmemobj_close(pop);

	DONE(NULL);
}
