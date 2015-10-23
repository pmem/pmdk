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
 * pthread.c -- fake pthread locks
 */

#include <pthread.h>
#include <time.h>

#include <sys/types.h>
#include <sys/timeb.h>


#define	TIMED_LOCK(action, ts) {\
	if ((action) == TRUE)\
		return 0;\
	long long et = (ts)->tv_sec * 1000 + (ts)->tv_nsec / 1000000;\
	while (1) {\
		struct __timeb64 t;\
		_ftime64(&t);\
		if (t.time * 1000 + t.millitm >= et)\
			return ETIMEDOUT;\
		if ((action) == TRUE)\
			return 0;\
		Sleep(1);\
	}\
	return ETIMEDOUT;\
}


#ifdef USE_WIN_MUTEX

int
pthread_mutex_init(pthread_mutex_t *restrict mutex,
	const pthread_mutexattr_t *restrict attr)
{
	/* XXX - errno */
	*mutex = CreateMutex(NULL, FALSE, NULL);
	return *mutex == NULL;
}

int
pthread_mutex_destroy(pthread_mutex_t *restrict mutex)
{
	/* XXX - errno */
	return 0;
}

int
pthread_mutex_lock(pthread_mutex_t *restrict mutex)
{
	/* XXX - errno */
	return WaitForSingleObject(*mutex, INFINITE) == WAIT_FAILED;
}

int
pthread_mutex_trylock(pthread_mutex_t *restrict mutex)
{
	/* XXX - errno */
	return WaitForSingleObject(*mutex, 0) == WAIT_FAILED;
}

int
pthread_mutex_unlock(pthread_mutex_t *restrict mutex)
{
	/* XXX - errno */
	return ReleaseMutex(*mutex) == FALSE;
}

#else

int
pthread_mutex_init(pthread_mutex_t *restrict mutex,
	const pthread_mutexattr_t *restrict attr)
{
	(void) attr;

	InitializeCriticalSection(mutex);
	return 0;
}

int
pthread_mutex_destroy(pthread_mutex_t *restrict mutex)
{
	DeleteCriticalSection(mutex);
	return 0;
}

int
pthread_mutex_lock(pthread_mutex_t *restrict mutex)
{
	EnterCriticalSection(mutex);
	return 0;
}

int
pthread_mutex_trylock(pthread_mutex_t *restrict mutex)
{
	return (TryEnterCriticalSection(mutex) == FALSE) ? 0 : EBUSY;
}

/* XXX - non POSIX */
int
pthread_mutex_timedlock(pthread_mutex_t *restrict mutex,
	const struct timespec *abstime)
{
	TIMED_LOCK(TryEnterCriticalSection(mutex), abstime);
}

int
pthread_mutex_unlock(pthread_mutex_t *restrict mutex)
{
	LeaveCriticalSection(mutex);
	return 0;
}

#endif

#ifdef USE_WIN_SRWLOCK

int
pthread_rwlock_init(pthread_rwlock_t *restrict rwlock,
	const pthread_rwlockattr_t *restrict attr)
{
	(void) attr;

	InitializeSRWLock(rwlock);
	return 0;
}

int
pthread_rwlock_destroy(pthread_rwlock_t *restrict rwlock)
{
	/* do nothing */
	(void) rwlock;

	return 0;
}

int
pthread_rwlock_rdlock(pthread_rwlock_t *restrict rwlock)
{
	AcquireSRWLockShared(rwlock);
	return 0;
}

int
pthread_rwlock_wrlock(pthread_rwlock_t *restrict rwlock)
{
	AcquireSRWLockExclusive(rwlock);
	return 0;
}

int
pthread_rwlock_tryrdlock(pthread_rwlock_t *restrict rwlock)
{
	return (TryAcquireSRWLockShared(rwlock) == FALSE) ? 0 : EBUSY;
}

int
pthread_rwlock_trywrlock(pthread_rwlock_t *restrict rwlock)
{
	return (TryAcquireSRWLockExclusive(rwlock) == FALSE) ? 0 : EBUSY;
}

int
pthread_rwlock_timedrdlock(pthread_rwlock_t *restrict rwlock,
	const struct timespec *abstime)
{
	TIMED_LOCK(TryAcquireSRWLockShared(rwlock), abstime);
}

int
pthread_rwlock_timedwrlock(pthread_rwlock_t *restrict rwlock,
	const struct timespec *abstime)
{
	TIMED_LOCK(TryAcquireSRWLockExclusive(rwlock), abstime);
}

int
pthread_rwlock_unlock(pthread_rwlock_t *restrict rwlock)
{
	/* XXX - distinquish between shared/exclusive lock */
	ReleaseSRWLockExclusive(rwlock); /* ReleaseSRWLockShared(rwlock); */
	return 0;
}

#endif


int
pthread_cond_init(pthread_cond_t *restrict cond,
	const pthread_condattr_t *restrict attr)
{
	(void) attr;

	InitializeConditionVariable(cond);
	return 0;
}

int
pthread_cond_destroy(pthread_cond_t *restrict cond)
{
	/* do nothing */
	(void) cond;

	return 0;
}

int
pthread_cond_broadcast(pthread_cond_t *restrict cond)
{
	WakeAllConditionVariable(cond);
	return 0;
}

int
pthread_cond_signal(pthread_cond_t *restrict cond)
{
	WakeConditionVariable(cond);
	return 0;
}

int
pthread_cond_timedwait(pthread_cond_t *restrict cond,
	pthread_mutex_t *restrict mutex, const struct timespec *abstime)
{
	DWORD ms = (DWORD)(abstime->tv_sec * 1000 +
					abstime->tv_nsec / 1000000);
	return (SleepConditionVariableCS(cond, mutex, ms) == FALSE)
							? 0 : ETIMEDOUT;
}

int
pthread_cond_wait(pthread_cond_t *restrict cond,
	pthread_mutex_t *restrict mutex)
{
	return SleepConditionVariableCS(cond, mutex, INFINITE) == FALSE;
}
