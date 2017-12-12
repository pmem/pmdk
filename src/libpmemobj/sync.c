/*
 * Copyright 2015-2017, Intel Corporation
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
 * sync.c -- persistent memory resident synchronization primitives
 */

#include <inttypes.h>

#include "obj.h"
#include "out.h"
#include "util.h"
#include "sync.h"
#include "sys_util.h"
#include "util.h"
#include "valgrind_internal.h"

#ifdef __FreeBSD__
#define RECORD_LOCK(init, type, p) \
	if (init) {\
		PMEM##type##_internal *head = pop->type##_head;\
		while (!util_bool_compare_and_swap64(&pop->type##_head, head,\
			p)) {\
			head = pop->type##_head;\
		}\
		p->PMEM##type##_next = head;\
	}
#else
#define RECORD_LOCK(init, type, p)
#endif

/*
 * _get_lock -- (internal) atomically initialize and return a lock.
 *	Returns -1 on error, 0 if the caller is not the lock
 *	initializer, 1 if the caller is the lock initializer.
 */
static int
_get_lock(uint64_t pop_runid, volatile uint64_t *runid, void *lock,
	int (*init_lock)(void *lock, void *arg))
{
	uint64_t tmp_runid;
	int initializer = 0;

	while ((tmp_runid = *runid) != pop_runid) {
		if (tmp_runid == pop_runid - 1)
			continue;

		if (!util_bool_compare_and_swap64(runid, tmp_runid,
				pop_runid - 1))
			continue;

		initializer = 1;

		if (init_lock(lock, NULL)) {
			ERR("error initializing lock");
			util_fetch_and_and64(runid, 0);
			return -1;
		}

		if (util_bool_compare_and_swap64(runid, pop_runid - 1,
				pop_runid) == 0) {
			ERR("error setting lock runid");
			return -1;
		}
	}

	return initializer;
}

/*
 * get_mutex -- (internal) atomically initialize, record and return a mutex
 */
static inline os_mutex_t *
get_mutex(PMEMobjpool *pop, PMEMmutex_internal *imp)
{
	if (likely(imp->pmemmutex.runid == pop->run_id))
		return &imp->PMEMmutex_lock;

	volatile uint64_t *runid = &imp->pmemmutex.runid;

	LOG(5, "PMEMmutex %p pop->run_id %" PRIu64 " pmemmutex.runid %" PRIu64,
		imp, pop->run_id, *runid);

	ASSERTeq((uintptr_t)runid % util_alignof(uint64_t), 0);

	COMPILE_ERROR_ON(sizeof(PMEMmutex) != sizeof(PMEMmutex_internal));
	COMPILE_ERROR_ON(util_alignof(PMEMmutex) != util_alignof(os_mutex_t));

	VALGRIND_REMOVE_PMEM_MAPPING(imp, _POBJ_CL_SIZE);

	int initializer = _get_lock(pop->run_id, runid, &imp->PMEMmutex_lock,
		(void *)os_mutex_init);
	if (initializer == -1) {
		return NULL;
	}

	RECORD_LOCK(initializer, mutex, imp);

	return &imp->PMEMmutex_lock;
}

/*
 * get_rwlock -- (internal) atomically initialize, record and return a rwlock
 */
static inline os_rwlock_t *
get_rwlock(PMEMobjpool *pop, PMEMrwlock_internal *irp)
{
	if (likely(irp->pmemrwlock.runid == pop->run_id))
		return &irp->PMEMrwlock_lock;

	volatile uint64_t *runid = &irp->pmemrwlock.runid;

	LOG(5, "PMEMrwlock %p pop->run_id %"\
		PRIu64 " pmemrwlock.runid %" PRIu64,
		irp, pop->run_id, *runid);

	ASSERTeq((uintptr_t)runid % util_alignof(uint64_t), 0);

	COMPILE_ERROR_ON(sizeof(PMEMrwlock) != sizeof(PMEMrwlock_internal));
	COMPILE_ERROR_ON(util_alignof(PMEMrwlock)
		!= util_alignof(os_rwlock_t));

	VALGRIND_REMOVE_PMEM_MAPPING(irp, _POBJ_CL_SIZE);

	int initializer = _get_lock(pop->run_id, runid, &irp->PMEMrwlock_lock,
		(void *)os_rwlock_init);
	if (initializer == -1) {
		return NULL;
	}

	RECORD_LOCK(initializer, rwlock, irp);

	return &irp->PMEMrwlock_lock;
}

/*
 * get_cond -- (internal) atomically initialize, record and return a
 *	condition variable
 */
static inline os_cond_t *
get_cond(PMEMobjpool *pop, PMEMcond_internal *icp)
{
	if (likely(icp->pmemcond.runid == pop->run_id))
		return &icp->PMEMcond_cond;

	volatile uint64_t *runid = &icp->pmemcond.runid;

	LOG(5, "PMEMcond %p pop->run_id %" PRIu64 " pmemcond.runid %" PRIu64,
		icp, pop->run_id, *runid);

	ASSERTeq((uintptr_t)runid % util_alignof(uint64_t), 0);

	COMPILE_ERROR_ON(sizeof(PMEMcond) != sizeof(PMEMcond_internal));
	COMPILE_ERROR_ON(util_alignof(PMEMcond) != util_alignof(os_cond_t));

	VALGRIND_REMOVE_PMEM_MAPPING(icp, _POBJ_CL_SIZE);

	int initializer = _get_lock(pop->run_id, runid, &icp->PMEMcond_cond,
		(void *)os_cond_init);
	if (initializer == -1) {
		return NULL;
	}

	RECORD_LOCK(initializer, cond, icp);

	return &icp->PMEMcond_cond;
}

/*
 * pmemobj_mutex_zero -- zero-initialize a pmem resident mutex
 *
 * This function is not MT safe.
 */
void
pmemobj_mutex_zero(PMEMobjpool *pop, PMEMmutex *mutexp)
{
	LOG(3, "pop %p mutex %p", pop, mutexp);

	ASSERTeq(pop, pmemobj_pool_by_ptr(mutexp));

	PMEMmutex_internal *mutexip = (PMEMmutex_internal *)mutexp;
	mutexip->pmemmutex.runid = 0;
	pmemops_persist(&pop->p_ops, &mutexip->pmemmutex.runid,
				sizeof(mutexip->pmemmutex.runid));
}

/*
 * pmemobj_mutex_lock -- lock a pmem resident mutex
 *
 * Atomically initializes and locks a PMEMmutex, otherwise behaves as its
 * POSIX counterpart.
 */
int
pmemobj_mutex_lock(PMEMobjpool *pop, PMEMmutex *mutexp)
{
	LOG(3, "pop %p mutex %p", pop, mutexp);

	ASSERTeq(pop, pmemobj_pool_by_ptr(mutexp));

	PMEMmutex_internal *mutexip = (PMEMmutex_internal *)mutexp;
	os_mutex_t *mutex = get_mutex(pop, mutexip);

	if (mutex == NULL)
		return EINVAL;

	ASSERTeq((uintptr_t)mutex % util_alignof(os_mutex_t), 0);

	return os_mutex_lock(mutex);
}

/*
 * pmemobj_mutex_assert_locked -- checks whether mutex is locked.
 *
 * Returns 0 when mutex is locked.
 */
int
pmemobj_mutex_assert_locked(PMEMobjpool *pop, PMEMmutex *mutexp)
{
	LOG(3, "pop %p mutex %p", pop, mutexp);

	ASSERTeq(pop, pmemobj_pool_by_ptr(mutexp));

	PMEMmutex_internal *mutexip = (PMEMmutex_internal *)mutexp;
	os_mutex_t *mutex = get_mutex(pop, mutexip);
	if (mutex == NULL)
		return EINVAL;

	ASSERTeq((uintptr_t)mutex % util_alignof(os_mutex_t), 0);

	int ret = os_mutex_trylock(mutex);
	if (ret == EBUSY)
		return 0;
	if (ret == 0) {
		util_mutex_unlock(mutex);
		/*
		 * There's no good error code for this case. EINVAL is used for
		 * something else here.
		 */
		return ENODEV;
	}
	return ret;
}

/*
 * pmemobj_mutex_timedlock -- lock a pmem resident mutex
 *
 * Atomically initializes and locks a PMEMmutex, otherwise behaves as its
 * POSIX counterpart.
 */
int
pmemobj_mutex_timedlock(PMEMobjpool *pop, PMEMmutex *__restrict mutexp,
		const struct timespec *__restrict abs_timeout)
{
	LOG(3, "pop %p mutex %p", pop, mutexp);

	ASSERTeq(pop, pmemobj_pool_by_ptr(mutexp));

	PMEMmutex_internal *mutexip = (PMEMmutex_internal *)mutexp;
	os_mutex_t *mutex = get_mutex(pop, mutexip);
	if (mutex == NULL)
		return EINVAL;

	ASSERTeq((uintptr_t)mutex % util_alignof(os_mutex_t), 0);

	return os_mutex_timedlock(mutex, abs_timeout);
}

/*
 * pmemobj_mutex_trylock -- trylock a pmem resident mutex
 *
 * Atomically initializes and trylocks a PMEMmutex, otherwise behaves as its
 * POSIX counterpart.
 */
int
pmemobj_mutex_trylock(PMEMobjpool *pop, PMEMmutex *mutexp)
{
	LOG(3, "pop %p mutex %p", pop, mutexp);

	ASSERTeq(pop, pmemobj_pool_by_ptr(mutexp));

	PMEMmutex_internal *mutexip = (PMEMmutex_internal *)mutexp;
	os_mutex_t *mutex = get_mutex(pop, mutexip);
	if (mutex == NULL)
		return EINVAL;

	ASSERTeq((uintptr_t)mutex % util_alignof(os_mutex_t), 0);

	return os_mutex_trylock(mutex);
}

/*
 * pmemobj_mutex_unlock -- unlock a pmem resident mutex
 */
int
pmemobj_mutex_unlock(PMEMobjpool *pop, PMEMmutex *mutexp)
{
	LOG(3, "pop %p mutex %p", pop, mutexp);

	ASSERTeq(pop, pmemobj_pool_by_ptr(mutexp));

	/* XXX potential performance improvement - move GET to debug version */
	PMEMmutex_internal *mutexip = (PMEMmutex_internal *)mutexp;
	os_mutex_t *mutex = get_mutex(pop, mutexip);
	if (mutex == NULL)
		return EINVAL;

	ASSERTeq((uintptr_t)mutex % util_alignof(os_mutex_t), 0);

	return os_mutex_unlock(mutex);
}

/*
 * pmemobj_rwlock_zero -- zero-initialize a pmem resident rwlock
 *
 * This function is not MT safe.
 */
void
pmemobj_rwlock_zero(PMEMobjpool *pop, PMEMrwlock *rwlockp)
{
	LOG(3, "pop %p rwlock %p", pop, rwlockp);

	ASSERTeq(pop, pmemobj_pool_by_ptr(rwlockp));

	PMEMrwlock_internal *rwlockip = (PMEMrwlock_internal *)rwlockp;
	rwlockip->pmemrwlock.runid = 0;
	pmemops_persist(&pop->p_ops, &rwlockip->pmemrwlock.runid,
				sizeof(rwlockip->pmemrwlock.runid));
}

/*
 * pmemobj_rwlock_rdlock -- rdlock a pmem resident mutex
 *
 * Atomically initializes and rdlocks a PMEMrwlock, otherwise behaves as its
 * POSIX counterpart.
 */
int
pmemobj_rwlock_rdlock(PMEMobjpool *pop, PMEMrwlock *rwlockp)
{
	LOG(3, "pop %p rwlock %p", pop, rwlockp);

	ASSERTeq(pop, pmemobj_pool_by_ptr(rwlockp));

	PMEMrwlock_internal *rwlockip = (PMEMrwlock_internal *)rwlockp;
	os_rwlock_t *rwlock = get_rwlock(pop, rwlockip);
	if (rwlock == NULL)
		return EINVAL;

	ASSERTeq((uintptr_t)rwlock % util_alignof(os_rwlock_t), 0);

	return os_rwlock_rdlock(rwlock);
}

/*
 * pmemobj_rwlock_wrlock -- wrlock a pmem resident mutex
 *
 * Atomically initializes and wrlocks a PMEMrwlock, otherwise behaves as its
 * POSIX counterpart.
 */
int
pmemobj_rwlock_wrlock(PMEMobjpool *pop, PMEMrwlock *rwlockp)
{
	LOG(3, "pop %p rwlock %p", pop, rwlockp);

	ASSERTeq(pop, pmemobj_pool_by_ptr(rwlockp));

	PMEMrwlock_internal *rwlockip = (PMEMrwlock_internal *)rwlockp;
	os_rwlock_t *rwlock = get_rwlock(pop, rwlockip);
	if (rwlock == NULL)
		return EINVAL;

	ASSERTeq((uintptr_t)rwlock % util_alignof(os_rwlock_t), 0);

	return os_rwlock_wrlock(rwlock);
}

/*
 * pmemobj_rwlock_timedrdlock -- timedrdlock a pmem resident mutex
 *
 * Atomically initializes and timedrdlocks a PMEMrwlock, otherwise behaves as
 * its POSIX counterpart.
 */
int
pmemobj_rwlock_timedrdlock(PMEMobjpool *pop, PMEMrwlock *__restrict rwlockp,
			const struct timespec *__restrict abs_timeout)
{
	LOG(3, "pop %p rwlock %p timeout sec %ld nsec %ld", pop, rwlockp,
		abs_timeout->tv_sec, abs_timeout->tv_nsec);

	ASSERTeq(pop, pmemobj_pool_by_ptr(rwlockp));

	PMEMrwlock_internal *rwlockip = (PMEMrwlock_internal *)rwlockp;
	os_rwlock_t *rwlock = get_rwlock(pop, rwlockip);
	if (rwlock == NULL)
		return EINVAL;

	ASSERTeq((uintptr_t)rwlock % util_alignof(os_rwlock_t), 0);

	return os_rwlock_timedrdlock(rwlock, abs_timeout);
}

/*
 * pmemobj_rwlock_timedwrlock -- timedwrlock a pmem resident mutex
 *
 * Atomically initializes and timedwrlocks a PMEMrwlock, otherwise behaves as
 * its POSIX counterpart.
 */
int
pmemobj_rwlock_timedwrlock(PMEMobjpool *pop, PMEMrwlock *__restrict rwlockp,
			const struct timespec *__restrict abs_timeout)
{
	LOG(3, "pop %p rwlock %p timeout sec %ld nsec %ld", pop, rwlockp,
		abs_timeout->tv_sec, abs_timeout->tv_nsec);

	ASSERTeq(pop, pmemobj_pool_by_ptr(rwlockp));

	PMEMrwlock_internal *rwlockip = (PMEMrwlock_internal *)rwlockp;
	os_rwlock_t *rwlock = get_rwlock(pop, rwlockip);
	if (rwlock == NULL)
		return EINVAL;

	ASSERTeq((uintptr_t)rwlock % util_alignof(os_rwlock_t), 0);

	return os_rwlock_timedwrlock(rwlock, abs_timeout);
}

/*
 * pmemobj_rwlock_tryrdlock -- tryrdlock a pmem resident mutex
 *
 * Atomically initializes and tryrdlocks a PMEMrwlock, otherwise behaves as its
 * POSIX counterpart.
 */
int
pmemobj_rwlock_tryrdlock(PMEMobjpool *pop, PMEMrwlock *rwlockp)
{
	LOG(3, "pop %p rwlock %p", pop, rwlockp);

	ASSERTeq(pop, pmemobj_pool_by_ptr(rwlockp));

	PMEMrwlock_internal *rwlockip = (PMEMrwlock_internal *)rwlockp;
	os_rwlock_t *rwlock = get_rwlock(pop, rwlockip);
	if (rwlock == NULL)
		return EINVAL;

	ASSERTeq((uintptr_t)rwlock % util_alignof(os_rwlock_t), 0);

	return os_rwlock_tryrdlock(rwlock);
}

/*
 * pmemobj_rwlock_trywrlock -- trywrlock a pmem resident mutex
 *
 * Atomically initializes and trywrlocks a PMEMrwlock, otherwise behaves as its
 * POSIX counterpart.
 */
int
pmemobj_rwlock_trywrlock(PMEMobjpool *pop, PMEMrwlock *rwlockp)
{
	LOG(3, "pop %p rwlock %p", pop, rwlockp);

	ASSERTeq(pop, pmemobj_pool_by_ptr(rwlockp));

	PMEMrwlock_internal *rwlockip = (PMEMrwlock_internal *)rwlockp;
	os_rwlock_t *rwlock = get_rwlock(pop, rwlockip);
	if (rwlock == NULL)
		return EINVAL;

	ASSERTeq((uintptr_t)rwlock % util_alignof(os_rwlock_t), 0);

	return os_rwlock_trywrlock(rwlock);
}

/*
 * pmemobj_rwlock_unlock -- unlock a pmem resident rwlock
 */
int
pmemobj_rwlock_unlock(PMEMobjpool *pop, PMEMrwlock *rwlockp)
{
	LOG(3, "pop %p rwlock %p", pop, rwlockp);

	ASSERTeq(pop, pmemobj_pool_by_ptr(rwlockp));

	/* XXX potential performance improvement - move GET to debug version */
	PMEMrwlock_internal *rwlockip = (PMEMrwlock_internal *)rwlockp;
	os_rwlock_t *rwlock = get_rwlock(pop, rwlockip);
	if (rwlock == NULL)
		return EINVAL;

	ASSERTeq((uintptr_t)rwlock % util_alignof(os_rwlock_t), 0);

	return os_rwlock_unlock(rwlock);
}

/*
 * pmemobj_cond_zero -- zero-initialize a pmem resident condition variable
 *
 * This function is not MT safe.
 */
void
pmemobj_cond_zero(PMEMobjpool *pop, PMEMcond *condp)
{
	LOG(3, "pop %p cond %p", pop, condp);

	ASSERTeq(pop, pmemobj_pool_by_ptr(condp));

	PMEMcond_internal *condip = (PMEMcond_internal *)condp;
	condip->pmemcond.runid = 0;
	pmemops_persist(&pop->p_ops, &condip->pmemcond.runid,
			sizeof(condip->pmemcond.runid));
}

/*
 * pmemobj_cond_broadcast -- broadcast a pmem resident condition variable
 *
 * Atomically initializes and broadcast a PMEMcond, otherwise behaves as its
 * POSIX counterpart.
 */
int
pmemobj_cond_broadcast(PMEMobjpool *pop, PMEMcond *condp)
{
	LOG(3, "pop %p cond %p", pop, condp);

	ASSERTeq(pop, pmemobj_pool_by_ptr(condp));

	PMEMcond_internal *condip = (PMEMcond_internal *)condp;
	os_cond_t *cond = get_cond(pop, condip);
	if (cond == NULL)
		return EINVAL;

	ASSERTeq((uintptr_t)cond % util_alignof(os_cond_t), 0);

	return os_cond_broadcast(cond);
}

/*
 * pmemobj_cond_signal -- signal a pmem resident condition variable
 *
 * Atomically initializes and signal a PMEMcond, otherwise behaves as its
 * POSIX counterpart.
 */
int
pmemobj_cond_signal(PMEMobjpool *pop, PMEMcond *condp)
{
	LOG(3, "pop %p cond %p", pop, condp);

	ASSERTeq(pop, pmemobj_pool_by_ptr(condp));

	PMEMcond_internal *condip = (PMEMcond_internal *)condp;
	os_cond_t *cond = get_cond(pop, condip);
	if (cond == NULL)
		return EINVAL;

	ASSERTeq((uintptr_t)cond % util_alignof(os_cond_t), 0);

	return os_cond_signal(cond);
}

/*
 * pmemobj_cond_timedwait -- timedwait on a pmem resident condition variable
 *
 * Atomically initializes and timedwait on a PMEMcond, otherwise behaves as its
 * POSIX counterpart.
 */
int
pmemobj_cond_timedwait(PMEMobjpool *pop, PMEMcond *__restrict condp,
			PMEMmutex *__restrict mutexp,
			const struct timespec *__restrict abs_timeout)
{
	LOG(3, "pop %p cond %p mutex %p abstime sec %ld nsec %ld", pop, condp,
		mutexp, abs_timeout->tv_sec, abs_timeout->tv_nsec);

	ASSERTeq(pop, pmemobj_pool_by_ptr(mutexp));
	ASSERTeq(pop, pmemobj_pool_by_ptr(condp));

	PMEMcond_internal *condip = (PMEMcond_internal *)condp;
	PMEMmutex_internal *mutexip = (PMEMmutex_internal *)mutexp;
	os_cond_t *cond = get_cond(pop, condip);
	os_mutex_t *mutex = get_mutex(pop, mutexip);
	if ((cond == NULL) || (mutex == NULL))
		return EINVAL;

	ASSERTeq((uintptr_t)mutex % util_alignof(os_mutex_t), 0);
	ASSERTeq((uintptr_t)cond % util_alignof(os_cond_t), 0);

	return os_cond_timedwait(cond, mutex, abs_timeout);
}

/*
 * pmemobj_cond_wait -- wait on a pmem resident condition variable
 *
 * Atomically initializes and wait on a PMEMcond, otherwise behaves as its
 * POSIX counterpart.
 */
int
pmemobj_cond_wait(PMEMobjpool *pop, PMEMcond *condp,
			PMEMmutex *__restrict mutexp)
{
	LOG(3, "pop %p cond %p mutex %p", pop, condp, mutexp);

	ASSERTeq(pop, pmemobj_pool_by_ptr(mutexp));
	ASSERTeq(pop, pmemobj_pool_by_ptr(condp));

	PMEMcond_internal *condip = (PMEMcond_internal *)condp;
	PMEMmutex_internal *mutexip = (PMEMmutex_internal *)mutexp;
	os_cond_t *cond = get_cond(pop, condip);
	os_mutex_t *mutex = get_mutex(pop, mutexip);
	if ((cond == NULL) || (mutex == NULL))
		return EINVAL;

	ASSERTeq((uintptr_t)mutex % util_alignof(os_mutex_t), 0);
	ASSERTeq((uintptr_t)cond % util_alignof(os_cond_t), 0);

	return os_cond_wait(cond, mutex);
}
