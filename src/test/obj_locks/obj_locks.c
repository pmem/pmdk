/*
 * Copyright 2016, Intel Corporation
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
 * obj_locks.c -- unit test for PMEMmutex, PMEMrwlock and PMEMcond
 */
#include <sys/param.h>
#include <string.h>

#include "unittest.h"
#include "libpmemobj.h"

#define LAYOUT_NAME "obj_locks"
#define NUM_LOCKS 5
#define NUM_THREADS 15
#define MAX_FUNC 8

TOID_DECLARE(struct locks, 0);

struct locks {
	PMEMobjpool *pop;
	PMEMmutex mtx[NUM_LOCKS];
	PMEMrwlock rwlk[NUM_LOCKS];
	PMEMcond cond[NUM_LOCKS];
	int data[NUM_LOCKS];
};

struct thread_args {
	pthread_t t;
	TOID(struct locks) lock;
	int t_id;
};

typedef void *(*fn_lock)(void *arg);
struct thread_args threads[NUM_THREADS];

/*
 * do_mutex_lock_one -- lock and unlock all mutexes sequentially
 */
static void *
do_mutex_lock_one(void *arg)
{
	struct thread_args *t = arg;
	struct locks *lock = D_RW(t->lock);
	for (int i = 0; i < NUM_LOCKS; i++) {
		pmemobj_mutex_lock(lock->pop, &lock->mtx[i]);
		lock->data[i]++;
		pmemobj_mutex_unlock(lock->pop, &lock->mtx[i]);
	}
	return NULL;
}

/*
 * do_mutex_lock_all -- lock and unlock all mutexes at one time
 */
static void *
do_mutex_lock_all(void *arg)
{
	struct thread_args *t = arg;
	struct locks *lock = D_RW(t->lock);
	for (int i = 0; i < NUM_LOCKS; i++) {
		pmemobj_mutex_lock(lock->pop, &lock->mtx[i]);
		lock->data[i]++;
	}

	for (int i = 0; i < NUM_LOCKS; i++)
		pmemobj_mutex_unlock(lock->pop, &lock->mtx[i]);

	return NULL;
}

/*
 * do_rwlock_wrlock_one -- lock and unlock all write rwlocks sequentially
 */
static void *
do_rwlock_wrlock_one(void *arg)
{
	struct thread_args *t = arg;
	struct locks *lock = D_RW(t->lock);
	for (int i = 0; i < NUM_LOCKS; i++) {
		pmemobj_rwlock_wrlock(lock->pop, &lock->rwlk[i]);
		lock->data[i]++;
		pmemobj_rwlock_unlock(lock->pop, &lock->rwlk[i]);
	}
	return NULL;
}

/*
 * do_rwlock_wrlock_all -- lock and unlock all write rwlocks at one time
 */
static void *
do_rwlock_wrlock_all(void *arg)
{
	struct thread_args *t = arg;
	struct locks *lock = D_RW(t->lock);
	for (int i = 0; i < NUM_LOCKS; i++) {
		pmemobj_rwlock_wrlock(lock->pop, &lock->rwlk[i]);
		lock->data[i]++;
	}

	for (int i = 0; i < NUM_LOCKS; i++)
		pmemobj_rwlock_unlock(lock->pop, &lock->rwlk[i]);

	return NULL;
}

/*
 * do_rwlock_rdlock_one -- lock and unlock all read rwlocks sequentially
 */
static void *
do_rwlock_rdlock_one(void *arg)
{
	struct thread_args *t = arg;
	struct locks *lock = D_RW(t->lock);
	for (int i = 0; i < NUM_LOCKS; i++) {
		pmemobj_rwlock_rdlock(lock->pop, &lock->rwlk[i]);
		pmemobj_rwlock_unlock(lock->pop, &lock->rwlk[i]);
	}
	return NULL;
}

/*
 * do_rwlock_rdlock_all -- lock and unlock all read rwlocks at one time
 */
static void *
do_rwlock_rdlock_all(void *arg)
{
	struct thread_args *t = arg;
	struct locks *lock = D_RW(t->lock);
	for (int i = 0; i < NUM_LOCKS; i++)
		pmemobj_rwlock_rdlock(lock->pop, &lock->rwlk[i]);

	for (int i = 0; i < NUM_LOCKS; i++)
		pmemobj_rwlock_unlock(lock->pop, &lock->rwlk[i]);

	return NULL;
}

/*
 * do_cond_signal -- lock block on a condition variables,
 * and unlock them by signal
 */
static void *
do_cond_signal(void *arg)
{
	struct thread_args *t = arg;
	struct locks *lock = D_RW(t->lock);
	if (t->t_id % 2 != 0) {
		for (int i = 0; i < NUM_LOCKS; i++) {
			pmemobj_mutex_lock(lock->pop, &lock->mtx[i]);
			pmemobj_cond_wait(lock->pop, &lock->cond[i],
								&lock->mtx[i]);
			lock->data[i]++;
			pmemobj_mutex_unlock(lock->pop, &lock->mtx[i]);
		}
	} else {
		for (int i = 0; i < NUM_LOCKS; i++) {
			while (1) {
				pmemobj_mutex_lock(lock->pop, &lock->mtx[i]);
				pmemobj_cond_signal(lock->pop, &lock->cond[i]);

				/*
				 * If there are all threads with unpaired
				 * indexes unblocked there is no need to
				 * send signal
				 */
				if (lock->data[i] >= NUM_THREADS / 2)
					break;

				pmemobj_mutex_unlock(lock->pop, &lock->mtx[i]);
			}
			lock->data[i]++;
			pmemobj_mutex_unlock(lock->pop, &lock->mtx[i]);
		}
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
	struct thread_args *t = arg;
	struct locks *lock = D_RW(t->lock);
	for (int i = 0; i < NUM_LOCKS && t->t_id != 0; i++) {
		pmemobj_mutex_lock(lock->pop, &lock->mtx[i]);
		pmemobj_cond_wait(lock->pop, &lock->cond[i],
							&lock->mtx[i]);
		lock->data[i]++;
		pmemobj_mutex_unlock(lock->pop, &lock->mtx[i]);
	}


	for (int i = 0; i < NUM_LOCKS && t->t_id == 0; i++) {
		while (1) {
			pmemobj_mutex_lock(lock->pop, &lock->mtx[i]);
			pmemobj_cond_broadcast(lock->pop, &lock->cond[i]);

			/*
			 * If there are all threads, besides first one,
			 * unblocked there is no need to send signal
			 */
			if (lock->data[i] >= NUM_THREADS - 1)
				break;

			pmemobj_mutex_unlock(lock->pop, &lock->mtx[i]);
		}
		lock->data[i]++;
		pmemobj_mutex_unlock(lock->pop, &lock->mtx[i]);
	}

	return NULL;
}

fn_lock do_lock[MAX_FUNC] = {do_mutex_lock_one, do_mutex_lock_all,
				do_rwlock_wrlock_one, do_rwlock_wrlock_all,
				do_rwlock_rdlock_one, do_rwlock_rdlock_all,
				do_cond_signal, do_cond_broadcast};

/*
 * do_lock_init -- initialize all types of locks
 */
static void
do_lock_init(struct locks *lock)
{
	for (int i = 0; i < NUM_LOCKS; i++) {
		pmemobj_mutex_zero(lock->pop, &lock->mtx[i]);
		pmemobj_rwlock_zero(lock->pop, &lock->rwlk[i]);
		pmemobj_cond_zero(lock->pop, &lock->cond[i]);
	}
}

/*
 * do_lock_mt -- perform multithread lock operations
 */
static void
do_lock_mt(TOID(struct locks) lock, unsigned f_num)
{
	memset(D_RW(lock)->data, 0, sizeof(int) * NUM_LOCKS);
	for (int i = 0; i < NUM_THREADS; ++i) {
		threads[i].lock = lock;
		threads[i].t_id = i;
		PTHREAD_CREATE(&threads[i].t, NULL, do_lock[f_num],
								&threads[i]);
	}
	for (int i = 0; i < NUM_THREADS; ++i)
		PTHREAD_JOIN(threads[i].t, NULL);

	/*
	 * If all threads passed function properly and used every lock, there
	 * should be every element in data array incremented exactly one time
	 * by every thread.
	 */
	for (int i = 0; i < NUM_LOCKS; i++)
		UT_ASSERT((D_RO(lock)->data[i] == NUM_THREADS) ||
						(D_RO(lock)->data[i] == 0));
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

	for (int i = 0; i < MAX_FUNC; i++)
		do_lock_mt(lock, i);

	POBJ_FREE(&lock);

	pmemobj_close(pop);
	DONE(NULL);
}
