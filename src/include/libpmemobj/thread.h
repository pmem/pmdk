/*
 * Copyright 2014-2017, Intel Corporation
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
