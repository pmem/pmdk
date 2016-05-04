/*
 * Copyright 2015-2016, Intel Corporation
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
 * pthread.h -- (imperfect) POSIX-like threads for Windows
 *
 * Loosely inspired by:
 * http://locklessinc.com/articles/pthreads_on_windows/
 */

/*
 * XXX - The initial approach to NVML for Windows port was to minimize the
 * amount of changes required in the core part of the library, and to avoid
 * preprocessor conditionals, if possible.  For that reason, some of the
 * Linux system calls that have no equivalents on Windows have been emulated
 * using Windows API.
 * Note that it was not a goal to fully emulate POSIX-compliant behavior
 * of mentioned functions.  They are used only internally, so current
 * implementation is just good enough to satisfy NVML needs and to make it
 * work on Windows.
 *
 * This is a subject for change in the future.  Likely, all these functions
 * will be replaced with "util_xxx" wrappers with OS-specific implementation
 * for Linux and Windows.
 */

#ifndef PTHREAD_H
#define PTHREAD_H 1

#include <stdint.h>

/* XXX - dummy */
typedef int pthread_t;
typedef pthread_attr_t;
typedef long pthread_once_t;
typedef DWORD pthread_key_t;

#define PTHREAD_ONCE_INIT 0

int pthread_once(pthread_once_t *o, void (*func)(void));

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
int pthread_key_delete(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void *value);
void *pthread_getspecific(pthread_key_t key);

typedef struct {
	unsigned attr;
	CRITICAL_SECTION lock;
} pthread_mutex_t;

typedef struct {
	unsigned attr;
	SRWLOCK lock;
} pthread_rwlock_t;

typedef struct {
	unsigned attr;
	CONDITION_VARIABLE cond;
} pthread_cond_t;

typedef int pthread_mutexattr_t;
typedef int pthread_rwlockattr_t;
typedef int pthread_condattr_t;

/* Mutex types */
enum
{
	PTHREAD_MUTEX_NORMAL = 0,
	PTHREAD_MUTEX_RECURSIVE = 1,
	PTHREAD_MUTEX_ERRORCHECK = 2,
	PTHREAD_MUTEX_DEFAULT = PTHREAD_MUTEX_NORMAL
};

/* RWlock types */
enum
{
	PTHREAD_RWLOCK_PREFER_READER = 0,
	PTHREAD_RWLOCK_PREFER_WRITER = 1,
	PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE = 2,
	PTHREAD_RWLOCK_DEFAULT = PTHREAD_RWLOCK_PREFER_READER
};

int pthread_mutexattr_init(pthread_mutexattr_t *attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);

int pthread_mutexattr_gettype(const pthread_mutexattr_t *__restrict attr,
	int *__restrict type);
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);

int pthread_mutex_init(pthread_mutex_t *__restrict mutex,
	const pthread_mutexattr_t *__restrict attr);
int pthread_mutex_destroy(pthread_mutex_t *__restrict mutex);
int pthread_mutex_lock(pthread_mutex_t *__restrict mutex);
int pthread_mutex_trylock(pthread_mutex_t *__restrict mutex);
int pthread_mutex_unlock(pthread_mutex_t *__restrict mutex);

/* XXX - non POSIX */
int pthread_mutex_timedlock(pthread_mutex_t *__restrict mutex,
	const struct timespec *abstime);

int pthread_rwlock_init(pthread_rwlock_t *__restrict rwlock,
	const pthread_rwlockattr_t *__restrict attr);
int pthread_rwlock_destroy(pthread_rwlock_t *__restrict rwlock);
int pthread_rwlock_rdlock(pthread_rwlock_t *__restrict rwlock);
int pthread_rwlock_wrlock(pthread_rwlock_t *__restrict rwlock);
int pthread_rwlock_tryrdlock(pthread_rwlock_t *__restrict rwlock);
int pthread_rwlock_trywrlock(pthread_rwlock_t *__restrict rwlock);
int pthread_rwlock_unlock(pthread_rwlock_t *__restrict rwlock);
int pthread_rwlock_timedrdlock(pthread_rwlock_t *__restrict rwlock,
	const struct timespec *abstime);
int pthread_rwlock_timedwrlock(pthread_rwlock_t *__restrict rwlock,
	const struct timespec *abstime);

int pthread_cond_init(pthread_cond_t *__restrict cond,
	const pthread_condattr_t *__restrict attr);
int pthread_cond_destroy(pthread_cond_t *__restrict cond);
int pthread_cond_broadcast(pthread_cond_t *__restrict cond);
int pthread_cond_signal(pthread_cond_t *__restrict cond);
int pthread_cond_timedwait(pthread_cond_t *__restrict cond,
	pthread_mutex_t *__restrict mutex, const struct timespec *abstime);
int pthread_cond_wait(pthread_cond_t *__restrict cond,
	pthread_mutex_t *__restrict mutex);

#endif /* PTHREAD_H */
