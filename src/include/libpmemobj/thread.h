/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2023, Intel Corporation */

/*
 * libpmemobj/thread.h -- definitions of libpmemobj thread/locking entry points
 */

#ifndef LIBPMEMOBJ_THREAD_H
#define LIBPMEMOBJ_THREAD_H 1

#include <time.h>
#include <libpmemobj/base.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Locking.
 */
#define _POBJ_CL_SIZE 64 /* cache line size */

typedef union {
	long long align;
	char padding[_POBJ_CL_SIZE];
} PMEMmutex;

typedef union {
	long long align;
	char padding[_POBJ_CL_SIZE];
} PMEMrwlock;

typedef union {
	long long align;
	char padding[_POBJ_CL_SIZE];
} PMEMcond;

void pmemobj_mutex_zero(PMEMobjpool *pop, PMEMmutex *mutexp);
int pmemobj_mutex_lock(PMEMobjpool *pop, PMEMmutex *mutexp);
int pmemobj_mutex_timedlock(PMEMobjpool *pop, PMEMmutex *__restrict mutexp,
	const struct timespec *__restrict abs_timeout);
int pmemobj_mutex_trylock(PMEMobjpool *pop, PMEMmutex *mutexp);
int pmemobj_mutex_unlock(PMEMobjpool *pop, PMEMmutex *mutexp);

void pmemobj_rwlock_zero(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_rdlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_wrlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_timedrdlock(PMEMobjpool *pop,
	PMEMrwlock *__restrict rwlockp,
	const struct timespec *__restrict abs_timeout);
int pmemobj_rwlock_timedwrlock(PMEMobjpool *pop,
	PMEMrwlock *__restrict rwlockp,
	const struct timespec *__restrict abs_timeout);
int pmemobj_rwlock_tryrdlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_trywrlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_unlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);

void pmemobj_cond_zero(PMEMobjpool *pop, PMEMcond *condp);
int pmemobj_cond_broadcast(PMEMobjpool *pop, PMEMcond *condp);
int pmemobj_cond_signal(PMEMobjpool *pop, PMEMcond *condp);
int pmemobj_cond_timedwait(PMEMobjpool *pop, PMEMcond *__restrict condp,
	PMEMmutex *__restrict mutexp,
	const struct timespec *__restrict abs_timeout);
int pmemobj_cond_wait(PMEMobjpool *pop, PMEMcond *condp,
	PMEMmutex *__restrict mutexp);

#ifdef __cplusplus
}
#endif

#endif	/* libpmemobj/thread.h */
