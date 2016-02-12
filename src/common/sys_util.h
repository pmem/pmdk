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
 * sys_util.h -- internal utility wrappers around system functions
 */

#include <errno.h>
#include <pthread.h>

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
