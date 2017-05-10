/*
 * Copyright 2017, Intel Corporation
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

#define _GNU_SOURCE
#include <pthread.h>

#include "os_thread.h"

/*
 * os_thread_once -- pthread_once abstraction layer
 */
int
os_thread_once(os_thread_once_t *o, void (*func)(void))
{
	return pthread_once((pthread_once_t *)o, func);
}

/*
 * os_thread_key_create -- pthread_key_create abstraction layer
 */
int
os_thread_key_create(os_thread_key_t *key, void (*destructor)(void *))
{
	return pthread_key_create((pthread_key_t *)key, destructor);
}

/*
 * os_thread_key_delete -- pthread_key_delete abstraction layer
 */
int
os_thread_key_delete(os_thread_key_t key)
{
	return pthread_key_delete((pthread_key_t)key);
}

/*
 * os_thread_setspecific -- pthread_key_setspecific abstraction layer
 */
int
os_thread_setspecific(os_thread_key_t key, const void *value)
{
	return pthread_setspecific((pthread_key_t)key, value);
}

/*
 * os_thread_getspecific -- pthread_key_getspecific abstraction layer
 */
void *
os_thread_getspecific(os_thread_key_t key)
{
	return pthread_getspecific((pthread_key_t)key);
}

/*
 * os_thread_mutex_init -- pthread_mutex_init abstraction layer
 */
int
os_thread_mutex_init(os_thread_mutex_t *__restrict mutex)
{
	return pthread_mutex_init((pthread_mutex_t *)mutex, NULL);
}

/*
 * os_thread_mutex_destroy -- pthread_mutex_destroy abstraction layer
 */
int
os_thread_mutex_destroy(os_thread_mutex_t *__restrict mutex)
{
	return pthread_mutex_destroy((pthread_mutex_t *)mutex);
}

/*
 * os_thread_mutex_lock -- pthread_mutex_lock abstraction layer
 */
int
os_thread_mutex_lock(os_thread_mutex_t *__restrict mutex)
{
	return pthread_mutex_lock((pthread_mutex_t *)mutex);
}

/*
 * os_thread_mutex_trylock -- pthread_mutex_trylock abstraction layer
 */
int
os_thread_mutex_trylock(os_thread_mutex_t *__restrict mutex)
{
	return pthread_mutex_trylock((pthread_mutex_t *)mutex);
}

/*
 * os_thread_mutex_unlock -- pthread_mutex_unlock abstraction layer
 */
int
os_thread_mutex_unlock(os_thread_mutex_t *__restrict mutex)
{
	return pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

/*
 * os_thread_mutex_timedlock -- pthread_mutex_timedlock abstraction layer
 */
int
os_thread_mutex_timedlock(os_thread_mutex_t *__restrict mutex,
	const struct timespec *abstime)
{
	return pthread_mutex_timedlock((pthread_mutex_t *)mutex, abstime);
}

/*
 * os_thread_rwlock_iniy -- pthread_rwlock_init abstraction layer
 */
int
os_thread_rwlock_init(os_thread_rwlock_t *__restrict rwlock)
{
	return pthread_rwlock_init((pthread_rwlock_t *)rwlock, NULL);
}

/*
 * os_thread_rwlock_destroy -- pthread_rwlock_destroy abstraction layer
 */
int
os_thread_rwlock_destroy(os_thread_rwlock_t *__restrict rwlock)
{
	return pthread_rwlock_destroy((pthread_rwlock_t *)rwlock);
}

/*
 * os_thread_rwlock_rdlock - pthread_rwlock_rdlock abstraction layer
 */
int
os_thread_rwlock_rdlock(os_thread_rwlock_t *__restrict rwlock)
{
	return pthread_rwlock_rdlock((pthread_rwlock_t *)rwlock);
}

/*
 * os_thread_rwlock_wrlock -- pthread_rwlock_wrlock abstraction layer
 */
int
os_thread_rwlock_wrlock(os_thread_rwlock_t *__restrict rwlock)
{
	return pthread_rwlock_wrlock((pthread_rwlock_t *)rwlock);
}

/*
 * os_thread_rwlock_unlock -- pthread_rwlock_unlock abstraction layer
 */
int
os_thread_rwlock_unlock(os_thread_rwlock_t *__restrict rwlock)
{
	return pthread_rwlock_unlock((pthread_rwlock_t *)rwlock);
}

/*
 * os_thread_rwlock_tryrdlock -- pthread_rwlock_tryrdlock abstraction layer
 */
int
os_thread_rwlock_tryrdlock(os_thread_rwlock_t *__restrict rwlock)
{
	return pthread_rwlock_tryrdlock((pthread_rwlock_t *)rwlock);
}

/*
 * os_thread_rwlock_tryrwlock -- pthread_rwlock_trywrlock abstraction layer
 */
int
os_thread_rwlock_trywrlock(os_thread_rwlock_t *__restrict rwlock)
{
	return pthread_rwlock_trywrlock((pthread_rwlock_t *)rwlock);
}

/*
 * os_thread_rwlock_timedrdlock -- pthread_rwlock_timedrdlock abstraction layer
 */
int
os_thread_rwlock_timedrdlock(os_thread_rwlock_t *__restrict rwlock,
	const struct timespec *abstime)
{
	return pthread_rwlock_timedrdlock((pthread_rwlock_t *)rwlock, abstime);
}

/*
 * os_thread_rwlock_timedwrlock -- pthread_rwlock_timedwrlock abstraction layer
 */
int
os_thread_rwlock_timedwrlock(os_thread_rwlock_t *__restrict rwlock,
	const struct timespec *abstime)
{
	return pthread_rwlock_timedwrlock((pthread_rwlock_t *)rwlock, abstime);
}
int
os_thread_spin_init(os_thread_spinlock_t *lock, int pshared)
{
	return pthread_spin_init(lock, pshared);
}
int
os_thread_spin_destroy(os_thread_spinlock_t *lock)
{
	return pthread_spin_destroy(lock);
}
int
os_thread_spin_lock(os_thread_spinlock_t *lock)
{
	return pthread_spin_lock(lock);
}
int
os_thread_spin_unlock(os_thread_spinlock_t *lock)
{
	return pthread_spin_unlock(lock);
}

int
os_thread_spin_trylock(os_thread_spinlock_t *lock)
{
	return pthread_spin_trylock(lock);
}
/*
 * os_thread_cond_init -- pthread_cond_init abstraction layer
 */
int
os_thread_cond_init(os_thread_cond_t *__restrict cond)
{
	return pthread_cond_init((pthread_cond_t *)cond, NULL);
}

/*
 * os_thread_cond_destroy -- pthread_cond_destroy abstraction layer
 */
int
os_thread_cond_destroy(os_thread_cond_t *__restrict cond)
{
	return pthread_cond_destroy((pthread_cond_t *)cond);
}

/*
 * os_thread_cond_broadcast -- pthread_cond_broadcast abstraction layer
 */
int
os_thread_cond_broadcast(os_thread_cond_t *__restrict cond)
{
	return pthread_cond_broadcast((pthread_cond_t *)cond);
}

/*
 * os_thread_cond_signal -- pthread_cond_signal abstraction layer
 */
int
os_thread_cond_signal(os_thread_cond_t *__restrict cond)
{
	return pthread_cond_signal((pthread_cond_t *)cond);
}

/*
 * os_thread_cond_timedwait -- pthread_cond_timedwait abstraction layer
 */
int
os_thread_cond_timedwait(os_thread_cond_t *__restrict cond,
	os_thread_mutex_t *__restrict mutex, const struct timespec *abstime)
{
	return pthread_cond_timedwait((pthread_cond_t *)cond,
		(pthread_mutex_t *)mutex, abstime);
}

/*
 * os_thread_cond_wait -- pthread_cond_wait abstraction layer
 */
int
os_thread_cond_wait(os_thread_cond_t *__restrict cond,
	os_thread_mutex_t *__restrict mutex)
{
	return pthread_cond_wait((pthread_cond_t *)cond, mutex);
}

/*
 * os_thread_create - pthread_create abstraction layer
 */
int
os_thread_create(os_thread_t *thread, const os_thread_attr_t *attr,
		void *(*start_routine)(void *), void *arg) {
	return pthread_create((pthread_t *)thread, attr, start_routine, arg);
}

/*
 * os_thread_join - pthread_join abstraction layer
 */
int
os_thread_join(os_thread_t thread, void **result)
{
	return pthread_join((pthread_t)thread, result);
}

/*
 * os_thread_atfork - pthread_atfork abstraction layer
 */
int
os_thread_atfork(void (*prepare)(void), void (*parent)(void),
	void (*child)(void))
{
	return pthread_atfork(prepare, parent, child);
}

int
os_thread_setaffinity_np(os_thread_t thread, size_t set_size,
			const os_cpu_set_t *set)
{
	return pthread_setaffinity_np(thread, set_size, set);
}
