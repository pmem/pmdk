/*
 * Copyright 2015-2017, Intel Corporation
 * Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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
 * os_thread.h -- (imperfect) POSIX-like threads for Windows
 *
 * Loosely inspired by:
 * http://locklessinc.com/articles/os_threads_on_windows/
 */

#ifndef OS_THREAD_H
#define OS_THREAD_H 1
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <synchapi.h>
/* XXX - dummy */
typedef long os_thread_once_t;
typedef DWORD os_thread_key_t;

#define OS_THREAD_ONCE_INIT 0

typedef struct {
	unsigned attr;
	CRITICAL_SECTION lock;
} os_thread_mutex_t;

typedef struct {
	unsigned attr;
	char is_write;
	SRWLOCK lock;
} os_thread_rwlock_t;

typedef struct {
	unsigned attr;
	CONDITION_VARIABLE cond;
} os_thread_cond_t;

typedef struct {
	HANDLE thread_handle;
	void *arg;
	void *(*start_routine)(void *);
	void *result;
} os_thread_info, *os_thread_t;

typedef void os_thread_attr_t;
typedef DWORD_PTR os_cpu_set_t;

/* XXX: spinlock notimplemented on windows */
typedef int os_thread_spinlock_t;

void OS_CPU_ZERO(os_cpu_set_t *set);
void OS_CPU_SET(int cpu, os_cpu_set_t *set);

#else
#include <pthread.h>
typedef pthread_once_t os_thread_once_t;
typedef pthread_key_t os_thread_key_t;

#define OS_THREAD_ONCE_INIT PTHREAD_ONCE_INIT

typedef pthread_mutex_t os_thread_mutex_t;
typedef pthread_rwlock_t os_thread_rwlock_t;
typedef pthread_cond_t os_thread_cond_t;
typedef pthread_attr_t os_thread_attr_t;
typedef pthread_spinlock_t os_thread_spinlock_t;
typedef pthread_t os_thread_t;
typedef cpu_set_t os_cpu_set_t;
#define _When_(...)

#define OS_CPU_ZERO(set) CPU_ZERO(set)
#define OS_CPU_SET(cpu, set) CPU_SET(cpu, set)
#endif
int os_thread_once(os_thread_once_t *o, void (*func)(void));

int os_thread_key_create(os_thread_key_t *key, void (*destructor)(void *));
int os_thread_key_delete(os_thread_key_t key);
int os_thread_setspecific(os_thread_key_t key, const void *value);
void *os_thread_getspecific(os_thread_key_t key);

int os_thread_mutex_init(os_thread_mutex_t *__restrict mutex);
int os_thread_mutex_destroy(os_thread_mutex_t *__restrict mutex);
_When_(return == 0, _Acquires_lock_(mutex->lock))
int os_thread_mutex_lock(os_thread_mutex_t *__restrict mutex);
_When_(return == 0, _Acquires_lock_(mutex->lock))
int os_thread_mutex_trylock(os_thread_mutex_t *__restrict mutex);
int os_thread_mutex_unlock(os_thread_mutex_t *__restrict mutex);

/* XXX - non POSIX */
int os_thread_mutex_timedlock(os_thread_mutex_t *__restrict mutex,
	const struct timespec *abstime);

int os_thread_rwlock_init(os_thread_rwlock_t *__restrict rwlock);
int os_thread_rwlock_destroy(os_thread_rwlock_t *__restrict rwlock);
int os_thread_rwlock_rdlock(os_thread_rwlock_t *__restrict rwlock);
int os_thread_rwlock_wrlock(os_thread_rwlock_t *__restrict rwlock);
int os_thread_rwlock_tryrdlock(os_thread_rwlock_t *__restrict rwlock);
_When_(return == 0, _Acquires_exclusive_lock_(rwlock->lock))
int os_thread_rwlock_trywrlock(os_thread_rwlock_t *__restrict rwlock);
_When_(rwlock->is_write != 0, _Requires_exclusive_lock_held_(rwlock->lock))
_When_(rwlock->is_write == 0, _Requires_shared_lock_held_(rwlock->lock))
int os_thread_rwlock_unlock(os_thread_rwlock_t *__restrict rwlock);
int os_thread_rwlock_timedrdlock(os_thread_rwlock_t *__restrict rwlock,
	const struct timespec *abstime);
int os_thread_rwlock_timedwrlock(os_thread_rwlock_t *__restrict rwlock,
	const struct timespec *abstime);

int os_thread_spin_init(os_thread_spinlock_t *lock, int pshared);
int os_thread_spin_destroy(os_thread_spinlock_t *lock);
int os_thread_spin_lock(os_thread_spinlock_t *lock);
int os_thread_spin_unlock(os_thread_spinlock_t *lock);
int os_thread_spin_trylock(os_thread_spinlock_t *lock);

int os_thread_cond_init(os_thread_cond_t *__restrict cond);
int os_thread_cond_destroy(os_thread_cond_t *__restrict cond);
int os_thread_cond_broadcast(os_thread_cond_t *__restrict cond);
int os_thread_cond_signal(os_thread_cond_t *__restrict cond);
int os_thread_cond_timedwait(os_thread_cond_t *__restrict cond,
	os_thread_mutex_t *__restrict mutex, const struct timespec *abstime);
int os_thread_cond_wait(os_thread_cond_t *__restrict cond,
	os_thread_mutex_t *__restrict mutex);


/* threading */

int os_thread_create(os_thread_t *thread, const os_thread_attr_t *attr,
	void *(*start_routine)(void *), void *arg);

int os_thread_join(os_thread_t thread, void **result);

/* thread affinity */

int os_thread_setaffinity_np(os_thread_t thread, size_t set_size,
	const os_cpu_set_t *set);


int os_thread_atfork(void (*prepare)(void), void (*parent)(void),
	void (*child)(void));


#ifdef __cplusplus
}
#endif
#endif /* OS_THREAD_H */
