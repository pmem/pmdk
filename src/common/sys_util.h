// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2020, Intel Corporation */

/*
 * sys_util.h -- internal utility wrappers around system functions
 */

#ifndef PMDK_SYS_UTIL_H
#define PMDK_SYS_UTIL_H 1

#include <errno.h>

#include "os_thread.h"
#include "out.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * util_mutex_init -- os_mutex_init variant that never fails from
 * caller perspective. If os_mutex_init failed, this function aborts
 * the program.
 */
static inline void
util_mutex_init(os_mutex_t *m)
{
	int tmp = os_mutex_init(m);
	if (tmp) {
		errno = tmp;
		FATAL("!os_mutex_init");
	}
}

/*
 * util_mutex_destroy -- os_mutex_destroy variant that never fails from
 * caller perspective. If os_mutex_destroy failed, this function aborts
 * the program.
 */
static inline void
util_mutex_destroy(os_mutex_t *m)
{
	int tmp = os_mutex_destroy(m);
	if (tmp) {
		errno = tmp;
		FATAL("!os_mutex_destroy");
	}
}

/*
 * util_mutex_lock -- os_mutex_lock variant that never fails from
 * caller perspective. If os_mutex_lock failed, this function aborts
 * the program.
 */
static inline void
util_mutex_lock(os_mutex_t *m)
{
	int tmp = os_mutex_lock(m);
	if (tmp) {
		errno = tmp;
		FATAL("!os_mutex_lock");
	}
}

/*
 * util_mutex_trylock -- os_mutex_trylock variant that never fails from
 * caller perspective (other than EBUSY). If util_mutex_trylock failed, this
 * function aborts the program.
 * Returns 0 if locked successfully, otherwise returns EBUSY.
 */
static inline int
util_mutex_trylock(os_mutex_t *m)
{
	int tmp = os_mutex_trylock(m);
	if (tmp && tmp != EBUSY) {
		errno = tmp;
		FATAL("!os_mutex_trylock");
	}
	return tmp;
}

/*
 * util_mutex_unlock -- os_mutex_unlock variant that never fails from
 * caller perspective. If os_mutex_unlock failed, this function aborts
 * the program.
 */
static inline void
util_mutex_unlock(os_mutex_t *m)
{
	int tmp = os_mutex_unlock(m);
	if (tmp) {
		errno = tmp;
		FATAL("!os_mutex_unlock");
	}
}

/*
 * util_rwlock_init -- os_rwlock_init variant that never fails from
 * caller perspective. If os_rwlock_init failed, this function aborts
 * the program.
 */
static inline void
util_rwlock_init(os_rwlock_t *m)
{
	int tmp = os_rwlock_init(m);
	if (tmp) {
		errno = tmp;
		FATAL("!os_rwlock_init");
	}
}

/*
 * util_rwlock_rdlock -- os_rwlock_rdlock variant that never fails from
 * caller perspective. If os_rwlock_rdlock failed, this function aborts
 * the program.
 */
static inline void
util_rwlock_rdlock(os_rwlock_t *m)
{
	int tmp = os_rwlock_rdlock(m);
	if (tmp) {
		errno = tmp;
		FATAL("!os_rwlock_rdlock");
	}
}

/*
 * util_rwlock_wrlock -- os_rwlock_wrlock variant that never fails from
 * caller perspective. If os_rwlock_wrlock failed, this function aborts
 * the program.
 */
static inline void
util_rwlock_wrlock(os_rwlock_t *m)
{
	int tmp = os_rwlock_wrlock(m);
	if (tmp) {
		errno = tmp;
		FATAL("!os_rwlock_wrlock");
	}
}

/*
 * util_rwlock_unlock -- os_rwlock_unlock variant that never fails from
 * caller perspective. If os_rwlock_unlock failed, this function aborts
 * the program.
 */
static inline void
util_rwlock_unlock(os_rwlock_t *m)
{
	int tmp = os_rwlock_unlock(m);
	if (tmp) {
		errno = tmp;
		FATAL("!os_rwlock_unlock");
	}
}

/*
 * util_rwlock_destroy -- os_rwlock_destroy variant that never fails from
 * caller perspective. If os_rwlock_destroy failed, this function aborts
 * the program.
 */
static inline void
util_rwlock_destroy(os_rwlock_t *m)
{
	int tmp = os_rwlock_destroy(m);
	if (tmp) {
		errno = tmp;
		FATAL("!os_rwlock_destroy");
	}
}

/*
 * util_spin_init -- os_spin_init variant that logs on fail and sets errno.
 */
static inline int
util_spin_init(os_spinlock_t *lock, int pshared)
{
	int tmp = os_spin_init(lock, pshared);
	if (tmp) {
		errno = tmp;
		ERR("!os_spin_init");
	}
	return tmp;
}

/*
 * util_spin_destroy -- os_spin_destroy variant that never fails from
 * caller perspective. If os_spin_destroy failed, this function aborts
 * the program.
 */
static inline void
util_spin_destroy(os_spinlock_t *lock)
{
	int tmp = os_spin_destroy(lock);
	if (tmp) {
		errno = tmp;
		FATAL("!os_spin_destroy");
	}
}

/*
 * util_spin_lock -- os_spin_lock variant that never fails from caller
 * perspective. If os_spin_lock failed, this function aborts the program.
 */
static inline void
util_spin_lock(os_spinlock_t *lock)
{
	int tmp = os_spin_lock(lock);
	if (tmp) {
		errno = tmp;
		FATAL("!os_spin_lock");
	}
}

/*
 * util_spin_unlock -- os_spin_unlock variant that never fails
 * from caller perspective. If os_spin_unlock failed,
 * this function aborts the program.
 */
static inline void
util_spin_unlock(os_spinlock_t *lock)
{
	int tmp = os_spin_unlock(lock);
	if (tmp) {
		errno = tmp;
		FATAL("!os_spin_unlock");
	}
}

/*
 * util_semaphore_init -- os_semaphore_init variant that never fails
 * from caller perspective. If os_semaphore_init failed,
 * this function aborts the program.
 */
static inline void
util_semaphore_init(os_semaphore_t *sem, unsigned value)
{
	if (os_semaphore_init(sem, value))
		FATAL("!os_semaphore_init");
}

/*
 * util_semaphore_destroy -- deletes a semaphore instance
 */
static inline void
util_semaphore_destroy(os_semaphore_t *sem)
{
	if (os_semaphore_destroy(sem) != 0)
		FATAL("!os_semaphore_destroy");
}

/*
 * util_semaphore_wait -- decreases the value of the semaphore
 */
static inline void
util_semaphore_wait(os_semaphore_t *sem)
{
	errno = 0;

	int ret;
	do {
		ret = os_semaphore_wait(sem);
	} while (errno == EINTR); /* signal interrupt */

	if (ret != 0)
		FATAL("!os_semaphore_wait");
}

/*
 * util_semaphore_trywait -- tries to decrease the value of the semaphore
 */
static inline int
util_semaphore_trywait(os_semaphore_t *sem)
{
	errno = 0;
	int ret;
	do {
		ret = os_semaphore_trywait(sem);
	} while (errno == EINTR); /* signal interrupt */

	if (ret != 0 && errno != EAGAIN)
		FATAL("!os_semaphore_trywait");

	return ret;
}

/*
 * util_semaphore_post -- increases the value of the semaphore
 */
static inline void
util_semaphore_post(os_semaphore_t *sem)
{
	if (os_semaphore_post(sem) != 0)
		FATAL("!os_semaphore_post");
}

static inline void
util_cond_init(os_cond_t *__restrict cond)
{
	if (os_cond_init(cond))
		FATAL("!os_cond_init");
}

static inline void
util_cond_destroy(os_cond_t *__restrict cond)
{
	if (os_cond_destroy(cond))
		FATAL("!os_cond_destroy");
}

#ifdef __cplusplus
}
#endif

#endif
