/*
 * Copyright 2017-2018, Intel Corporation
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
 * os_thread_posix.c -- Posix thread abstraction layer
 */

#define _GNU_SOURCE
#include <pthread.h>
#ifdef __FreeBSD__
#include <pthread_np.h>
#endif
#include <semaphore.h>

#include "os_thread.h"
#include "util.h"

typedef struct {
	pthread_t thread;
} internal_os_thread_t;

/*
 * os_once -- pthread_once abstraction layer
 */
int
os_once(os_once_t *o, void (*func)(void))
{
	COMPILE_ERROR_ON(sizeof(os_once_t) < sizeof(pthread_once_t));
	return pthread_once((pthread_once_t *)o, func);
}

/*
 * os_tls_key_create -- pthread_key_create abstraction layer
 */
int
os_tls_key_create(os_tls_key_t *key, void (*destructor)(void *))
{
	COMPILE_ERROR_ON(sizeof(os_tls_key_t) < sizeof(pthread_key_t));
	return pthread_key_create((pthread_key_t *)key, destructor);
}

/*
 * os_tls_key_delete -- pthread_key_delete abstraction layer
 */
int
os_tls_key_delete(os_tls_key_t key)
{
	return pthread_key_delete((pthread_key_t)key);
}

/*
 * os_tls_setspecific -- pthread_key_setspecific abstraction layer
 */
int
os_tls_set(os_tls_key_t key, const void *value)
{
	return pthread_setspecific((pthread_key_t)key, value);
}

/*
 * os_tls_get -- pthread_key_getspecific abstraction layer
 */
void *
os_tls_get(os_tls_key_t key)
{
	return pthread_getspecific((pthread_key_t)key);
}

/*
 * os_mutex_init -- pthread_mutex_init abstraction layer
 */
int
os_mutex_init(os_mutex_t *__restrict mutex)
{
	COMPILE_ERROR_ON(sizeof(os_mutex_t) < sizeof(pthread_mutex_t));
	return pthread_mutex_init((pthread_mutex_t *)mutex, NULL);
}

/*
 * os_mutex_destroy -- pthread_mutex_destroy abstraction layer
 */
int
os_mutex_destroy(os_mutex_t *__restrict mutex)
{
	return pthread_mutex_destroy((pthread_mutex_t *)mutex);
}

/*
 * os_mutex_lock -- pthread_mutex_lock abstraction layer
 */
int
os_mutex_lock(os_mutex_t *__restrict mutex)
{
	return pthread_mutex_lock((pthread_mutex_t *)mutex);
}

/*
 * os_mutex_trylock -- pthread_mutex_trylock abstraction layer
 */
int
os_mutex_trylock(os_mutex_t *__restrict mutex)
{
	return pthread_mutex_trylock((pthread_mutex_t *)mutex);
}

/*
 * os_mutex_unlock -- pthread_mutex_unlock abstraction layer
 */
int
os_mutex_unlock(os_mutex_t *__restrict mutex)
{
	return pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

/*
 * os_mutex_timedlock -- pthread_mutex_timedlock abstraction layer
 */
int
os_mutex_timedlock(os_mutex_t *__restrict mutex,
	const struct timespec *abstime)
{
	return pthread_mutex_timedlock((pthread_mutex_t *)mutex, abstime);
}

/*
 * os_rwlock_init -- pthread_rwlock_init abstraction layer
 */
int
os_rwlock_init(os_rwlock_t *__restrict rwlock)
{
	COMPILE_ERROR_ON(sizeof(os_rwlock_t) < sizeof(pthread_rwlock_t));
	return pthread_rwlock_init((pthread_rwlock_t *)rwlock, NULL);
}

/*
 * os_rwlock_destroy -- pthread_rwlock_destroy abstraction layer
 */
int
os_rwlock_destroy(os_rwlock_t *__restrict rwlock)
{
	return pthread_rwlock_destroy((pthread_rwlock_t *)rwlock);
}

/*
 * os_rwlock_rdlock - pthread_rwlock_rdlock abstraction layer
 */
int
os_rwlock_rdlock(os_rwlock_t *__restrict rwlock)
{
	return pthread_rwlock_rdlock((pthread_rwlock_t *)rwlock);
}

/*
 * os_rwlock_wrlock -- pthread_rwlock_wrlock abstraction layer
 */
int
os_rwlock_wrlock(os_rwlock_t *__restrict rwlock)
{
	return pthread_rwlock_wrlock((pthread_rwlock_t *)rwlock);
}

/*
 * os_rwlock_unlock -- pthread_rwlock_unlock abstraction layer
 */
int
os_rwlock_unlock(os_rwlock_t *__restrict rwlock)
{
	return pthread_rwlock_unlock((pthread_rwlock_t *)rwlock);
}

/*
 * os_rwlock_tryrdlock -- pthread_rwlock_tryrdlock abstraction layer
 */
int
os_rwlock_tryrdlock(os_rwlock_t *__restrict rwlock)
{
	return pthread_rwlock_tryrdlock((pthread_rwlock_t *)rwlock);
}

/*
 * os_rwlock_tryrwlock -- pthread_rwlock_trywrlock abstraction layer
 */
int
os_rwlock_trywrlock(os_rwlock_t *__restrict rwlock)
{
	return pthread_rwlock_trywrlock((pthread_rwlock_t *)rwlock);
}

/*
 * os_rwlock_timedrdlock -- pthread_rwlock_timedrdlock abstraction layer
 */
int
os_rwlock_timedrdlock(os_rwlock_t *__restrict rwlock,
	const struct timespec *abstime)
{
	return pthread_rwlock_timedrdlock((pthread_rwlock_t *)rwlock, abstime);
}

/*
 * os_rwlock_timedwrlock -- pthread_rwlock_timedwrlock abstraction layer
 */
int
os_rwlock_timedwrlock(os_rwlock_t *__restrict rwlock,
	const struct timespec *abstime)
{
	return pthread_rwlock_timedwrlock((pthread_rwlock_t *)rwlock, abstime);
}

/*
 * os_spin_init -- pthread_spin_init abstraction layer
 */
int
os_spin_init(os_spinlock_t *lock, int pshared)
{
	COMPILE_ERROR_ON(sizeof(os_spinlock_t) < sizeof(pthread_spinlock_t));
	return pthread_spin_init((pthread_spinlock_t *)lock, pshared);
}

/*
 * os_spin_destroy -- pthread_spin_destroy abstraction layer
 */
int
os_spin_destroy(os_spinlock_t *lock)
{
	return pthread_spin_destroy((pthread_spinlock_t *)lock);
}

/*
 * os_spin_lock -- pthread_spin_lock abstraction layer
 */
int
os_spin_lock(os_spinlock_t *lock)
{
	return pthread_spin_lock((pthread_spinlock_t *)lock);
}

/*
 * os_spin_unlock -- pthread_spin_unlock abstraction layer
 */
int
os_spin_unlock(os_spinlock_t *lock)
{
	return pthread_spin_unlock((pthread_spinlock_t *)lock);
}

/*
 * os_spin_trylock -- pthread_spin_trylock abstraction layer
 */

int
os_spin_trylock(os_spinlock_t *lock)
{
	return pthread_spin_trylock((pthread_spinlock_t *)lock);
}
/*
 * os_cond_init -- pthread_cond_init abstraction layer
 */
int
os_cond_init(os_cond_t *__restrict cond)
{
	COMPILE_ERROR_ON(sizeof(os_cond_t) < sizeof(pthread_cond_t));
	return pthread_cond_init((pthread_cond_t *)cond, NULL);
}

/*
 * os_cond_destroy -- pthread_cond_destroy abstraction layer
 */
int
os_cond_destroy(os_cond_t *__restrict cond)
{
	return pthread_cond_destroy((pthread_cond_t *)cond);
}

/*
 * os_cond_broadcast -- pthread_cond_broadcast abstraction layer
 */
int
os_cond_broadcast(os_cond_t *__restrict cond)
{
	return pthread_cond_broadcast((pthread_cond_t *)cond);
}

/*
 * os_cond_signal -- pthread_cond_signal abstraction layer
 */
int
os_cond_signal(os_cond_t *__restrict cond)
{
	return pthread_cond_signal((pthread_cond_t *)cond);
}

/*
 * os_cond_timedwait -- pthread_cond_timedwait abstraction layer
 */
int
os_cond_timedwait(os_cond_t *__restrict cond,
	os_mutex_t *__restrict mutex, const struct timespec *abstime)
{
	return pthread_cond_timedwait((pthread_cond_t *)cond,
		(pthread_mutex_t *)mutex, abstime);
}

/*
 * os_cond_wait -- pthread_cond_wait abstraction layer
 */
int
os_cond_wait(os_cond_t *__restrict cond,
	os_mutex_t *__restrict mutex)
{
	return pthread_cond_wait((pthread_cond_t *)cond,
		(pthread_mutex_t *)mutex);
}

/*
 * os_thread_create -- pthread_create abstraction layer
 */
int
os_thread_create(os_thread_t *thread, const os_thread_attr_t *attr,
		void *(*start_routine)(void *), void *arg)
{
	COMPILE_ERROR_ON(sizeof(os_thread_t) < sizeof(internal_os_thread_t));
	internal_os_thread_t *thread_info = (internal_os_thread_t *)thread;

	return pthread_create(&thread_info->thread, (pthread_attr_t *)attr,
		start_routine, arg);
}

/*
 * os_thread_join -- pthread_join abstraction layer
 */
int
os_thread_join(os_thread_t *thread, void **result)
{
	internal_os_thread_t *thread_info = (internal_os_thread_t *)thread;

	return pthread_join(thread_info->thread, result);
}

/*
 * os_thread_self -- pthread_self abstraction layer
 */
void
os_thread_self(os_thread_t *thread)
{
	internal_os_thread_t *thread_info = (internal_os_thread_t *)thread;

	thread_info->thread = pthread_self();
}

/*
 * os_thread_atfork -- pthread_atfork abstraction layer
 */
int
os_thread_atfork(void (*prepare)(void), void (*parent)(void),
	void (*child)(void))
{
	return pthread_atfork(prepare, parent, child);
}

/*
 * os_thread_setaffinity_np -- pthread_atfork abstraction layer
 */
int
os_thread_setaffinity_np(os_thread_t *thread, size_t set_size,
			const os_cpu_set_t *set)
{
	COMPILE_ERROR_ON(sizeof(os_cpu_set_t) < sizeof(cpu_set_t));
	internal_os_thread_t *thread_info = (internal_os_thread_t *)thread;

	return pthread_setaffinity_np(thread_info->thread, set_size,
		(cpu_set_t *)set);
}

/*
 * os_cpu_zero -- CP_ZERO abstraction layer
 */
void
os_cpu_zero(os_cpu_set_t *set)
{
	CPU_ZERO((cpu_set_t *)set);
}

/*
 * os_cpu_set -- CP_SET abstraction layer
 */
void
os_cpu_set(size_t cpu, os_cpu_set_t *set)
{
	CPU_SET(cpu, (cpu_set_t *)set);
}

/*
 * os_semaphore_init -- initializes semaphore instance
 */
int
os_semaphore_init(os_semaphore_t *sem, unsigned value)
{
	COMPILE_ERROR_ON(sizeof(os_semaphore_t) < sizeof(sem_t));
	return sem_init((sem_t *)sem, 0, value);
}

/*
 * os_semaphore_destroy -- destroys a semaphore instance
 */
int
os_semaphore_destroy(os_semaphore_t *sem)
{
	return sem_destroy((sem_t *)sem);
}

/*
 * os_semaphore_wait -- decreases the value of the semaphore
 */
int
os_semaphore_wait(os_semaphore_t *sem)
{
	return sem_wait((sem_t *)sem);
}

/*
 * os_semaphore_trywait -- tries to decrease the value of the semaphore
 */
int
os_semaphore_trywait(os_semaphore_t *sem)
{
	return sem_trywait((sem_t *)sem);
}

/*
 * os_semaphore_post -- increases the value of the semaphore
 */
int
os_semaphore_post(os_semaphore_t *sem)
{
	return sem_post((sem_t *)sem);
}
