/*
 * Copyright 2016-2017, Intel Corporation
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
 * sys_util.h -- internal utility wrappers around system functions
 */

#ifndef NVML_SYS_UTIL_H
#define NVML_SYS_UTIL_H 1

#include <errno.h>

#include "os_thread.h"

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

#endif
