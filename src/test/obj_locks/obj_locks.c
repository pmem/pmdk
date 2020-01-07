// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2020, Intel Corporation */

/*
 * obj_locks.c -- unit test for PMEMmutex, PMEMrwlock and PMEMcond
 */
#include <sys/param.h>
#include <string.h>

#include "unittest.h"
#include "libpmemobj.h"

#define LAYOUT_NAME "obj_locks"
#define NUM_THREADS 16
#define MAX_FUNC 5

TOID_DECLARE(struct locks, 0);

struct locks {
	PMEMobjpool *pop;
	PMEMmutex mtx;
	PMEMrwlock rwlk;
	PMEMcond cond;
	int data;
};

struct thread_args {
	os_thread_t t;
	TOID(struct locks) lock;
	int t_id;
};

typedef void *(*fn_lock)(void *arg);
static struct thread_args threads[NUM_THREADS];

/*
 * do_mutex_lock -- lock and unlock the mutex
 */
static void *
do_mutex_lock(void *arg)
{
	struct thread_args *t = (struct thread_args *)arg;
	struct locks *lock = D_RW(t->lock);
	pmemobj_mutex_lock(lock->pop, &lock->mtx);
	lock->data++;
	pmemobj_persist(lock->pop, &lock->data, sizeof(lock->data));
	pmemobj_mutex_unlock(lock->pop, &lock->mtx);
	return NULL;
}

/*
 * do_rwlock_wrlock -- lock and unlock the write rwlock
 */
static void *
do_rwlock_wrlock(void *arg)
{
	struct thread_args *t = (struct thread_args *)arg;
	struct locks *lock = D_RW(t->lock);
	pmemobj_rwlock_wrlock(lock->pop, &lock->rwlk);
	lock->data++;
	pmemobj_persist(lock->pop, &lock->data, sizeof(lock->data));
	pmemobj_rwlock_unlock(lock->pop, &lock->rwlk);
	return NULL;
}

/*
 * do_rwlock_rdlock -- lock and unlock the read rwlock
 */
static void *
do_rwlock_rdlock(void *arg)
{
	struct thread_args *t = (struct thread_args *)arg;
	struct locks *lock = D_RW(t->lock);
	pmemobj_rwlock_rdlock(lock->pop, &lock->rwlk);
	pmemobj_rwlock_unlock(lock->pop, &lock->rwlk);
	return NULL;
}

/*
 * do_cond_signal -- lock block on a condition variables,
 * and unlock them by signal
 */
static void *
do_cond_signal(void *arg)
{
	struct thread_args *t = (struct thread_args *)arg;
	struct locks *lock = D_RW(t->lock);
	if (t->t_id == 0) {
		pmemobj_mutex_lock(lock->pop, &lock->mtx);
		while (lock->data < (NUM_THREADS - 1))
			pmemobj_cond_wait(lock->pop, &lock->cond,
							&lock->mtx);
		lock->data++;
		pmemobj_persist(lock->pop, &lock->data, sizeof(lock->data));
		pmemobj_mutex_unlock(lock->pop, &lock->mtx);
	} else {
		pmemobj_mutex_lock(lock->pop, &lock->mtx);
		lock->data++;
		pmemobj_persist(lock->pop, &lock->data, sizeof(lock->data));
		pmemobj_cond_signal(lock->pop, &lock->cond);
		pmemobj_mutex_unlock(lock->pop, &lock->mtx);
	}

	return NULL;
}

/*
 * do_cond_broadcast -- lock block on a condition variables and unlock
 * by broadcasting
 */
static void *
do_cond_broadcast(void *arg)
{
	struct thread_args *t = (struct thread_args *)arg;
	struct locks *lock = D_RW(t->lock);
	if (t->t_id < (NUM_THREADS / 2)) {
		pmemobj_mutex_lock(lock->pop, &lock->mtx);
		while (lock->data < (NUM_THREADS / 2))
			pmemobj_cond_wait(lock->pop, &lock->cond,
							&lock->mtx);
		lock->data++;
		pmemobj_persist(lock->pop, &lock->data, sizeof(lock->data));
		pmemobj_mutex_unlock(lock->pop, &lock->mtx);
	} else {
		pmemobj_mutex_lock(lock->pop, &lock->mtx);
		lock->data++;
		pmemobj_persist(lock->pop, &lock->data, sizeof(lock->data));
		pmemobj_cond_broadcast(lock->pop, &lock->cond);
		pmemobj_mutex_unlock(lock->pop, &lock->mtx);
	}

	return NULL;
}

static fn_lock do_lock[MAX_FUNC] = {do_mutex_lock, do_rwlock_wrlock,
				do_rwlock_rdlock, do_cond_signal,
				do_cond_broadcast};

/*
 * do_lock_init -- initialize all types of locks
 */
static void
do_lock_init(struct locks *lock)
{
	pmemobj_mutex_zero(lock->pop, &lock->mtx);
	pmemobj_rwlock_zero(lock->pop, &lock->rwlk);
	pmemobj_cond_zero(lock->pop, &lock->cond);
}

/*
 * do_lock_mt -- perform multithread lock operations
 */
static void
do_lock_mt(TOID(struct locks) lock, unsigned f_num)
{
	D_RW(lock)->data = 0;
	for (int i = 0; i < NUM_THREADS; ++i) {
		threads[i].lock = lock;
		threads[i].t_id = i;
		THREAD_CREATE(&threads[i].t, NULL, do_lock[f_num],
								&threads[i]);
	}
	for (int i = 0; i < NUM_THREADS; ++i)
		THREAD_JOIN(&threads[i].t, NULL);

	/*
	 * If all threads passed function properly and used every lock, there
	 * should be every element in data array incremented exactly one time
	 * by every thread.
	 */
	UT_ASSERT((D_RO(lock)->data == NUM_THREADS) ||
					(D_RO(lock)->data == 0));
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_locks");

	if (argc != 2)
		UT_FATAL("usage: %s [file]", argv[0]);

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(argv[1], LAYOUT_NAME, PMEMOBJ_MIN_POOL,
	    S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create");
	TOID(struct locks) lock;
	POBJ_ALLOC(pop, &lock, struct locks, sizeof(struct locks), NULL, NULL);
	D_RW(lock)->pop = pop;

	do_lock_init(D_RW(lock));

	for (unsigned i = 0; i < MAX_FUNC; i++)
		do_lock_mt(lock, i);

	POBJ_FREE(&lock);

	pmemobj_close(pop);
	DONE(NULL);
}
