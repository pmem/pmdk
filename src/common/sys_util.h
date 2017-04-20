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
#include <pthread.h>

#include "out.h"

/*
 * util_mutex_init -- pthread_mutex_init variant that never fails from
 * caller perspective. If pthread_mutex_init failed, this function aborts
 * the program.
 */
static inline void
util_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *mutexattr)
{
	int tmp = pthread_mutex_init(m, mutexattr);
	if (tmp) {
		errno = tmp;
		FATAL("!pthread_mutex_init");
	}
}

/*
 * util_mutex_destroy -- pthread_mutex_destroy variant that never fails from
 * caller perspective. If pthread_mutex_destroy failed, this function aborts
 * the program.
 */
static inline void
util_mutex_destroy(pthread_mutex_t *m)
{
	int tmp = pthread_mutex_destroy(m);
	if (tmp) {
		errno = tmp;
		FATAL("!pthread_mutex_destroy");
	}
}

/*
 * util_mutex_lock -- pthread_mutex_lock variant that never fails from
 * caller perspective. If pthread_mutex_lock failed, this function aborts
 * the program.
 */
static inline void
util_mutex_lock(pthread_mutex_t *m)
{
	int tmp = pthread_mutex_lock(m);
	if (tmp) {
		errno = tmp;
		FATAL("!pthread_mutex_lock");
	}
}

/*
 * util_mutex_unlock -- pthread_mutex_unlock variant that never fails from
 * caller perspective. If pthread_mutex_unlock failed, this function aborts
 * the program.
 */
static inline void
util_mutex_unlock(pthread_mutex_t *m)
{
	int tmp = pthread_mutex_unlock(m);
	if (tmp) {
		errno = tmp;
		FATAL("!pthread_mutex_unlock");
	}
}

/*
 * util_rwlock_unlock -- pthread_rwlock_unlock variant that never fails from
 * caller perspective. If pthread_rwlock_unlock failed, this function aborts
 * the program.
 */
static inline void
util_rwlock_unlock(pthread_rwlock_t *m)
{
	int tmp = pthread_rwlock_unlock(m);
	if (tmp) {
		errno = tmp;
		FATAL("!pthread_rwlock_unlock");
	}
}

/*
 * util_spin_init -- pthread_spin_init variant that logs on fail and sets errno.
 */
static inline int
util_spin_init(pthread_spinlock_t *lock, int pshared)
{
	int tmp = pthread_spin_init(lock, pshared);
	if (tmp) {
		errno = tmp;
		ERR("!pthread_spin_init");
	}
	return tmp;
}

/*
 * util_spin_destroy -- pthread_spin_destroy variant that never fails from
 * caller perspective. If pthread_spin_destroy failed, this function aborts
 * the program.
 */
static inline void
util_spin_destroy(pthread_spinlock_t *lock)
{
	int tmp = pthread_spin_destroy(lock);
	if (tmp) {
		errno = tmp;
		FATAL("!pthread_spin_destroy");
	}
}

/*
 * util_spin_lock -- pthread_spin_lock variant that never fails from caller
 * perspective. If pthread_spin_lock failed, this function aborts the program.
 */
static inline void
util_spin_lock(pthread_spinlock_t *lock)
{
	int tmp = pthread_spin_lock(lock);
	if (tmp) {
		errno = tmp;
		FATAL("!pthread_spin_lock");
	}
}

/*
 * util_spin_unlock -- pthread_spin_unlock variant that never fails from caller
 * perspective. If pthread_spin_unlock failed, this function aborts the program.
 */
static inline void
util_spin_unlock(pthread_spinlock_t *lock)
{
	int tmp = pthread_spin_unlock(lock);
	if (tmp) {
		errno = tmp;
		FATAL("!pthread_spin_unlock");
	}
}

#endif
