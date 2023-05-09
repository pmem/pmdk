/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2023, Intel Corporation */
/*
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
 * os_thread.h -- os thread abstraction layer
 */

#ifndef OS_THREAD_H
#define OS_THREAD_H 1

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef union {
	long long align;
	char padding[44]; /* XXX linux: 40 windows: 44 */
} os_mutex_t;

typedef union {
	long long align;
	char padding[56];
} os_rwlock_t;

typedef union {
	long long align;
	char padding[48];
} os_cond_t;

typedef union {
	long long align;
	char padding[32]; /* XXX linux: 8 windows: 32 */
} os_thread_t;

typedef union {
	long long align;  /* linux: long FreeBSD: 12 */
	char padding[16]; /* 16 to be safe */
} os_once_t;

#define OS_ONCE_INIT { .padding = {0} }

typedef unsigned os_tls_key_t;

typedef union {
	long long align;
	char padding[56];
} os_semaphore_t;

typedef union {
	long long align;
	char padding[56];
} os_thread_attr_t;

typedef union {
	long long align;
	char padding[512];
} os_cpu_set_t;

#ifdef __FreeBSD__
#define cpu_set_t cpuset_t
typedef uintptr_t os_spinlock_t;
#else
typedef volatile int os_spinlock_t;
#endif

void os_cpu_zero(os_cpu_set_t *set);
void os_cpu_set(size_t cpu, os_cpu_set_t *set);

#define _When_(...)

int os_once(os_once_t *o, void (*func)(void));

int os_tls_key_create(os_tls_key_t *key, void (*destructor)(void *));
int os_tls_key_delete(os_tls_key_t key);
int os_tls_set(os_tls_key_t key, const void *value);
void *os_tls_get(os_tls_key_t key);

int os_mutex_init(os_mutex_t *__restrict mutex);
int os_mutex_destroy(os_mutex_t *__restrict mutex);
_When_(return == 0, _Acquires_lock_(mutex->lock))
int os_mutex_lock(os_mutex_t *__restrict mutex);
_When_(return == 0, _Acquires_lock_(mutex->lock))
int os_mutex_trylock(os_mutex_t *__restrict mutex);
int os_mutex_unlock(os_mutex_t *__restrict mutex);

/* XXX - non POSIX */
int os_mutex_timedlock(os_mutex_t *__restrict mutex,
	const struct timespec *abstime);

int os_rwlock_init(os_rwlock_t *__restrict rwlock);
int os_rwlock_destroy(os_rwlock_t *__restrict rwlock);
int os_rwlock_rdlock(os_rwlock_t *__restrict rwlock);
int os_rwlock_wrlock(os_rwlock_t *__restrict rwlock);
int os_rwlock_tryrdlock(os_rwlock_t *__restrict rwlock);
_When_(return == 0, _Acquires_exclusive_lock_(rwlock->lock))
int os_rwlock_trywrlock(os_rwlock_t *__restrict rwlock);
_When_(rwlock->is_write != 0, _Requires_exclusive_lock_held_(rwlock->lock))
_When_(rwlock->is_write == 0, _Requires_shared_lock_held_(rwlock->lock))
int os_rwlock_unlock(os_rwlock_t *__restrict rwlock);
int os_rwlock_timedrdlock(os_rwlock_t *__restrict rwlock,
	const struct timespec *abstime);
int os_rwlock_timedwrlock(os_rwlock_t *__restrict rwlock,
	const struct timespec *abstime);

int os_spin_init(os_spinlock_t *lock, int pshared);
int os_spin_destroy(os_spinlock_t *lock);
int os_spin_lock(os_spinlock_t *lock);
int os_spin_unlock(os_spinlock_t *lock);
int os_spin_trylock(os_spinlock_t *lock);

int os_cond_init(os_cond_t *__restrict cond);
int os_cond_destroy(os_cond_t *__restrict cond);
int os_cond_broadcast(os_cond_t *__restrict cond);
int os_cond_signal(os_cond_t *__restrict cond);
int os_cond_timedwait(os_cond_t *__restrict cond,
	os_mutex_t *__restrict mutex, const struct timespec *abstime);
int os_cond_wait(os_cond_t *__restrict cond,
	os_mutex_t *__restrict mutex);

/* threading */

int os_thread_create(os_thread_t *thread, const os_thread_attr_t *attr,
	void *(*start_routine)(void *), void *arg);

int os_thread_join(os_thread_t *thread, void **result);

void os_thread_self(os_thread_t *thread);

/* thread affinity */

int os_thread_setaffinity_np(os_thread_t *thread, size_t set_size,
	const os_cpu_set_t *set);

int os_thread_atfork(void (*prepare)(void), void (*parent)(void),
	void (*child)(void));

int os_semaphore_init(os_semaphore_t *sem, unsigned value);
int os_semaphore_destroy(os_semaphore_t *sem);
int os_semaphore_wait(os_semaphore_t *sem);
int os_semaphore_trywait(os_semaphore_t *sem);
int os_semaphore_post(os_semaphore_t *sem);

#ifdef __cplusplus
}
#endif
#endif /* OS_THREAD_H */
