// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2019, Intel Corporation */

/*
 * obj_tx_lock.c -- unit test for pmemobj_tx_lock()
 */
#include "unittest.h"
#include "libpmemobj.h"
#include "obj.h"

#define LAYOUT_NAME "obj_tx_lock"

#define NUM_LOCKS 2

struct transaction_data {
	PMEMmutex mutexes[NUM_LOCKS];
	PMEMrwlock rwlocks[NUM_LOCKS];
};

static PMEMobjpool *Pop;

#define DO_LOCK(mtx, rwlock)\
	pmemobj_tx_lock(TX_PARAM_MUTEX, &(mtx)[0]);\
	pmemobj_tx_lock(TX_PARAM_MUTEX, &(mtx)[1]);\
	pmemobj_tx_lock(TX_PARAM_RWLOCK, &(rwlock)[0]);\
	pmemobj_tx_lock(TX_PARAM_RWLOCK, &(rwlock)[1])

#define IS_UNLOCKED(pop, mtx, rwlock)\
	ret = 0;\
	ret += pmemobj_mutex_trylock((pop), &(mtx)[0]);\
	ret += pmemobj_mutex_trylock((pop), &(mtx)[1]);\
	ret += pmemobj_rwlock_trywrlock((pop), &(rwlock)[0]);\
	ret += pmemobj_rwlock_trywrlock((pop), &(rwlock)[1]);\
	UT_ASSERTeq(ret, 0);\
	pmemobj_mutex_unlock((pop), &(mtx)[0]);\
	pmemobj_mutex_unlock((pop), &(mtx)[1]);\
	pmemobj_rwlock_unlock((pop), &(rwlock)[0]);\
	pmemobj_rwlock_unlock((pop), &(rwlock)[1])

#define IS_LOCKED(pop, mtx, rwlock)\
	ret = pmemobj_mutex_trylock((pop), &(mtx)[0]);\
	UT_ASSERT(ret != 0);\
	ret = pmemobj_mutex_trylock((pop), &(mtx)[1]);\
	UT_ASSERT(ret != 0);\
	ret = pmemobj_rwlock_trywrlock((pop), &(rwlock)[0]);\
	UT_ASSERT(ret != 0);\
	ret = pmemobj_rwlock_trywrlock((pop), &(rwlock)[1]);\
	UT_ASSERT(ret != 0)

/*
 * do_tx_add_locks -- (internal) transaction where locks are added after
 * transaction begins
 */
static void *
do_tx_add_locks(struct transaction_data *data)
{
	int ret;
	IS_UNLOCKED(Pop, data->mutexes, data->rwlocks);
	TX_BEGIN(Pop) {
		DO_LOCK(data->mutexes, data->rwlocks);
		IS_LOCKED(Pop, data->mutexes, data->rwlocks);
	} TX_ONABORT { /* not called */
		UT_ASSERT(0);
	} TX_END
	IS_UNLOCKED(Pop, data->mutexes, data->rwlocks);
	return NULL;
}

/*
 * do_tx_add_locks_nested -- (internal) transaction where locks
 * are added after nested transaction begins
 */
static void *
do_tx_add_locks_nested(struct transaction_data *data)
{
	int ret;
	TX_BEGIN(Pop) {
		IS_UNLOCKED(Pop, data->mutexes, data->rwlocks);
		TX_BEGIN(Pop) {
			DO_LOCK(data->mutexes, data->rwlocks);
			IS_LOCKED(Pop, data->mutexes, data->rwlocks);
		} TX_END
		IS_LOCKED(Pop, data->mutexes, data->rwlocks);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
	IS_UNLOCKED(Pop, data->mutexes, data->rwlocks);
	return NULL;
}

/*
 * do_tx_add_locks_nested_all -- (internal) transaction where all locks
 * are added in both transactions after transaction begins
 */
static void *
do_tx_add_locks_nested_all(struct transaction_data *data)
{
	int ret;
	TX_BEGIN(Pop) {
		IS_UNLOCKED(Pop, data->mutexes, data->rwlocks);
		DO_LOCK(data->mutexes, data->rwlocks);
		IS_LOCKED(Pop, data->mutexes, data->rwlocks);
		TX_BEGIN(Pop) {
			IS_LOCKED(Pop, data->mutexes, data->rwlocks);
			DO_LOCK(data->mutexes, data->rwlocks);
			IS_LOCKED(Pop, data->mutexes, data->rwlocks);
		} TX_END
		IS_LOCKED(Pop, data->mutexes, data->rwlocks);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
	IS_UNLOCKED(Pop, data->mutexes, data->rwlocks);
	return NULL;
}

/*
 * do_tx_add_taken_lock -- (internal) verify that failed tx_lock doesn't add
 * the lock to transaction
 */
static void *
do_tx_add_taken_lock(struct transaction_data *data)
{
	/* wrlocks on Windows don't detect self-deadlocks */
#ifdef _WIN32
	(void) data;
#else
	UT_ASSERTeq(pmemobj_rwlock_wrlock(Pop, &data->rwlocks[0]), 0);

	TX_BEGIN(Pop) {
		UT_ASSERTne(pmemobj_tx_lock(TX_PARAM_RWLOCK, &data->rwlocks[0]),
				0);
	} TX_END

	UT_ASSERTne(pmemobj_rwlock_trywrlock(Pop, &data->rwlocks[0]), 0);
	UT_ASSERTeq(pmemobj_rwlock_unlock(Pop, &data->rwlocks[0]), 0);
#endif
	return NULL;
}

/*
 * do_tx_lock_fail -- call pmemobj_tx_lock with POBJ_TX_NO_ABORT flag
 * and taken lock
 */
static void *
do_tx_lock_fail(struct transaction_data *data)
{
	/* wrlocks on Windows don't detect self-deadlocks */
#ifdef _WIN32
	(void) data;
#else
	UT_ASSERTeq(pmemobj_rwlock_wrlock(Pop, &data->rwlocks[0]), 0);
	int ret = 0;
	/* return errno and abort transaction */
	TX_BEGIN(Pop) {
		pmemobj_tx_xlock(TX_PARAM_RWLOCK, &data->rwlocks[0], 0);
	} TX_ONABORT {
		UT_ASSERTne(errno, 0);
		UT_ASSERTeq(pmemobj_rwlock_unlock(Pop, &data->rwlocks[0]), 0);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END
	/* return ret without abort transaction */
	UT_ASSERTeq(pmemobj_rwlock_wrlock(Pop, &data->rwlocks[0]), 0);
	TX_BEGIN(Pop) {
		ret = pmemobj_tx_xlock(TX_PARAM_RWLOCK, &data->rwlocks[0],
				POBJ_XLOCK_NO_ABORT);
	} TX_ONCOMMIT {
		UT_ASSERTne(ret, 0);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
#endif
	return NULL;
}

static void
do_fault_injection(struct transaction_data *data)
{
	if (!pmemobj_fault_injection_enabled())
		return;

	pmemobj_inject_fault_at(PMEM_MALLOC, 1, "add_to_tx_and_lock");

	int ret;
	IS_UNLOCKED(Pop, data->mutexes, data->rwlocks);
	TX_BEGIN(Pop) {
		int err = pmemobj_tx_lock(TX_PARAM_MUTEX, &data->mutexes[0]);
		if (err)
			pmemobj_tx_abort(err);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		UT_ASSERTeq(errno, ENOMEM);
	} TX_END
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_tx_lock");

	if (argc < 3)
		UT_FATAL("usage: %s <file> [l|n|a|t|f|w]", argv[0]);

	if ((Pop = pmemobj_create(argv[1], LAYOUT_NAME,
	    PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create");

	PMEMoid root = pmemobj_root(Pop, sizeof(struct transaction_data));

	struct transaction_data *test_obj =
			(struct transaction_data *)pmemobj_direct(root);

	/* go through all arguments one by one */
	for (int arg = 2; arg < argc; arg++) {
		/* Scan the character of each argument. */
		if (strchr("lnatfw", argv[arg][0]) == NULL ||
				argv[arg][1] != '\0')
			UT_FATAL("op must be l or n or a or t or f or w");

		switch (argv[arg][0]) {
		case 'l':
			do_tx_add_locks(test_obj);
			break;

		case 'n':
			do_tx_add_locks_nested(test_obj);
			break;

		case 'a':
			do_tx_add_locks_nested_all(test_obj);
			break;

		case 't':
			do_tx_add_taken_lock(test_obj);
			break;
		case 'f':
			do_fault_injection(test_obj);
			break;
		case 'w':
			do_tx_lock_fail(test_obj);
			break;
		}
	}
	pmemobj_close(Pop);

	DONE(NULL);
}
