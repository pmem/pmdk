/*
 * Copyright (c) 2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * fake pthread.h
 */

#include <stdint.h>

#define	pthread_t int
#define	pthread_attr_t int
#define	pthread_mutex_t	int
#define	pthread_rwlock_t int
#define	pthread_cond_t int

#define	pthread_mutex_destroy(m) 0
#define	pthread_mutex_lock(m) 0
#define	pthread_mutex_unlock(m) 0
#define	pthread_mutex_trylock(m) 0

#define	pthread_rwlock_destroy(r) 0
#define	pthread_rwlock_lock(r) 0
#define	pthread_rwlock_unlock(r) 0
#define	pthread_rwlock_trywrlock(r) 0
#define	pthread_rwlock_rdlock(r) 0
#define	pthread_rwlock_wrlock(r) 0
#define	pthread_rwlock_timedrdlock(r, t) 0
#define	pthread_rwlock_tryrdlock(r) 0
#define	pthread_rwlock_timedwrlock(r, t) 0

#define	pthread_cond_destroy(c) 0
#define	pthread_cond_lock(c) 0
#define	pthread_cond_unlock(c) 0
#define	pthread_cond_broadcast(c) 0
#define	pthread_cond_wait(c, m) 0
#define	pthread_cond_signal(c) 0
#define	pthread_cond_timedwait(c, m, t) 0

#define	pthread_once_t int
#define	pthread_key_t int
#define	pthread_mutexattr_t int
#define	pthread_rwlockattr_t int
#define	pthread_condattr_t int

#define	pthread_mutexattr_init(a) 0
#define	pthread_mutexattr_settype(a, t) 0
#define	pthread_mutexattr_destroy(a) 0

#define	PTHREAD_MUTEX_RECURSIVE 0
#define	PTHREAD_ONCE_INIT 0

#define	pthread_key_create(k, d) 0
#define	pthread_getspecific(k) NULL
#define	pthread_setspecific(k, v) 0
#define	pthread_once(k, f) 0

int pthread_mutex_init(pthread_mutex_t *restrict mutex,
	const pthread_mutexattr_t *restrict attr);

int pthread_rwlock_init(pthread_rwlock_t *restrict rwlock,
	const pthread_rwlockattr_t *restrict attr);

int pthread_cond_init(pthread_cond_t *restrict cond,
	const pthread_condattr_t *restrict attr);
