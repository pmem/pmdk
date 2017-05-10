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
 * os_thread_windows.c -- (imperfect) POSIX-like threads for Windows
 *
 * Loosely inspired by:
 * http://locklessinc.com/articles/pthreads_on_windows/
 */

#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include "os_thread.h"
#include "util.h"
#include "out.h"

/* number of useconds between 1970-01-01T00:00:00Z and 1601-01-01T00:00:00Z */
#define DELTA_WIN2UNIX (11644473600000000ull)
#define TIMED_LOCK(action, ts) {\
	if ((action) == TRUE)\
		return 0;\
	unsigned long long et = (ts)->tv_sec * 1000000 + (ts)->tv_nsec / 1000;\
	while (1) {\
		FILETIME _t;\
		GetSystemTimeAsFileTime(&_t);\
		ULARGE_INTEGER _UI = {\
			.HighPart = _t.dwHighDateTime,\
			.LowPart = _t.dwLowDateTime,\
		};\
		if (_UI.QuadPart / 10 - DELTA_WIN2UNIX >= et)\
			return ETIMEDOUT;\
		if ((action) == TRUE)\
			return 0;\
		Sleep(1);\
	}\
	return ETIMEDOUT;\
}

int
os_thread_mutex_init(os_thread_mutex_t *__restrict mutex)
{
	InitializeCriticalSection(&mutex->lock);
	return 0;
}

int
os_thread_mutex_destroy(os_thread_mutex_t *__restrict mutex)
{
	DeleteCriticalSection(&mutex->lock);
	return 0;
}

_Use_decl_annotations_
int
os_thread_mutex_lock(os_thread_mutex_t *__restrict mutex)
{
	EnterCriticalSection(&mutex->lock);

	if (mutex->lock.RecursionCount > 1) {
		LeaveCriticalSection(&mutex->lock);
		/* XXX: fatal() */
		return EBUSY;
	}
	return 0;
}

_Use_decl_annotations_
int
os_thread_mutex_trylock(os_thread_mutex_t *__restrict mutex)
{
	if (TryEnterCriticalSection(&mutex->lock) == FALSE)
		return EBUSY;

	if (mutex->lock.RecursionCount > 1) {
		LeaveCriticalSection(&mutex->lock);
		/* XXX: fatal() */
		return EBUSY;
	}

	return 0;
}

/* XXX - non POSIX */
int
os_thread_mutex_timedlock(os_thread_mutex_t *__restrict mutex,
	const struct timespec *abstime)
{
	TIMED_LOCK((os_thread_mutex_trylock(mutex) == 0), abstime);
}

int
os_thread_mutex_unlock(os_thread_mutex_t *__restrict mutex)
{
	LeaveCriticalSection(&mutex->lock);
	return 0;
}

int
os_thread_rwlock_init(os_thread_rwlock_t *__restrict rwlock)
{
	InitializeSRWLock(&rwlock->lock);
	return 0;
}

int
os_thread_rwlock_destroy(os_thread_rwlock_t *__restrict rwlock)
{
	/* do nothing */
	(void) rwlock;

	return 0;
}

int
os_thread_rwlock_rdlock(os_thread_rwlock_t *__restrict rwlock)
{
	AcquireSRWLockShared(&rwlock->lock);
	rwlock->is_write = 0;
	return 0;
}

int
os_thread_rwlock_wrlock(os_thread_rwlock_t *__restrict rwlock)
{
	AcquireSRWLockExclusive(&rwlock->lock);
	rwlock->is_write = 1;
	return 0;
}

int
os_thread_rwlock_tryrdlock(os_thread_rwlock_t *__restrict rwlock)
{
	if (TryAcquireSRWLockShared(&rwlock->lock) == FALSE) {
		return EBUSY;
	} else {
		rwlock->is_write = 0;
		return 0;
	}
}

_Use_decl_annotations_
int
os_thread_rwlock_trywrlock(os_thread_rwlock_t *__restrict rwlock)
{
	if (TryAcquireSRWLockExclusive(&rwlock->lock) == FALSE) {
		return EBUSY;
	} else {
		rwlock->is_write = 1;
		return 0;
	}
}

int
os_thread_rwlock_timedrdlock(os_thread_rwlock_t *__restrict rwlock,
	const struct timespec *abstime)
{
	TIMED_LOCK((os_thread_rwlock_tryrdlock(rwlock) == 0), abstime);
}

int
os_thread_rwlock_timedwrlock(os_thread_rwlock_t *__restrict rwlock,
	const struct timespec *abstime)
{
	TIMED_LOCK((os_thread_rwlock_trywrlock(rwlock) == 0), abstime);
}

_Use_decl_annotations_
int
os_thread_rwlock_unlock(os_thread_rwlock_t *__restrict rwlock)
{
	if (rwlock->is_write)
		ReleaseSRWLockExclusive(&rwlock->lock);
	else
		ReleaseSRWLockShared(&rwlock->lock);
	return 0;
}

int
os_thread_cond_init(os_thread_cond_t *__restrict cond)
{
	InitializeConditionVariable(&cond->cond);
	return 0;
}

int
os_thread_cond_destroy(os_thread_cond_t *__restrict cond)
{
	/* do nothing */
	UNREFERENCED_PARAMETER(cond);

	return 0;
}

int
os_thread_cond_broadcast(os_thread_cond_t *__restrict cond)
{
	WakeAllConditionVariable(&cond->cond);
	return 0;
}

int
os_thread_cond_signal(os_thread_cond_t *__restrict cond)
{
	WakeConditionVariable(&cond->cond);
	return 0;
}

static DWORD
get_rel_wait(const struct timespec *abstime)
{
	struct __timeb64 t;
	_ftime64_s(&t);
	time_t now_ms = t.time * 1000 + t.millitm;
	time_t ms = (time_t)(abstime->tv_sec * 1000 +
		abstime->tv_nsec / 1000000);

	DWORD rel_wait = (DWORD)(ms - now_ms);

	return rel_wait < 0 ? 0 : rel_wait;
}

int
os_thread_cond_timedwait(os_thread_cond_t *__restrict cond,
	os_thread_mutex_t *__restrict mutex, const struct timespec *abstime)
{
	BOOL ret;
	SetLastError(0);
	ret = SleepConditionVariableCS(&cond->cond, &mutex->lock,
			get_rel_wait(abstime));
	if (ret == FALSE)
		return (GetLastError() == ERROR_TIMEOUT) ? ETIMEDOUT : EINVAL;

	return 0;
}

int
os_thread_cond_wait(os_thread_cond_t *__restrict cond,
	os_thread_mutex_t *__restrict mutex)
{
	/* XXX - return error code based on GetLastError() */
	BOOL ret;
	ret = SleepConditionVariableCS(&cond->cond, &mutex->lock, INFINITE);
	return (ret == FALSE) ? EINVAL : 0;
}

int
os_thread_once(os_thread_once_t *once, void (*func)(void))
{
	if (!_InterlockedCompareExchange(once, 1, 0))
		func();
	return 0;
}

int
os_thread_key_create(os_thread_key_t *key, void (*destructor)(void *))
{
	/* XXX - destructor not supported */

	*key = TlsAlloc();
	if (*key == TLS_OUT_OF_INDEXES)
		return EAGAIN;
	if (!TlsSetValue(*key, NULL)) /* XXX - not needed? */
		return ENOMEM;
	return 0;
}

int
os_thread_key_delete(os_thread_key_t key)
{
	/* XXX - destructor not supported */

	if (!TlsFree(key))
		return EINVAL;
	return 0;
}

int
os_thread_setspecific(os_thread_key_t key, const void *value)
{
	if (!TlsSetValue(key, (LPVOID)value))
		return ENOENT;
	return 0;
}

void *
os_thread_getspecific(os_thread_key_t key)
{
	return TlsGetValue(key);
}

/* threading */
os_thread_once_t Pthread_self_index_initialized;
DWORD Pthread_self_index;

/*
 * os_thread_init is called once before the first POSIX thread is spawned i.e.
 * before the start_routine of the first POSIX thread is executed.  Here
 * we make sure:
 *
 *  - we have an entry allocated in thread local storage, where we will store
 *    the address of the os_thread_v structure for each thread
 */
void
os_thread_init(void)
{
	Pthread_self_index = TlsAlloc();
	if (Pthread_self_index == TLS_OUT_OF_INDEXES)
		abort();
}

/*
 * os_thread_start_routine_wrapper is a start routine for _beginthreadex() and
 * it helps:
 *
 *  - wrap the os_thread_create's start function
 *  - do the necessary initialization for POSIX threading implementation
 */
unsigned __stdcall
os_thread_start_routine_wrapper(void *arg)
{
	os_thread_t thread_info = (os_thread_t)arg;

	os_thread_once(&Pthread_self_index_initialized, os_thread_init);
	TlsSetValue(Pthread_self_index, thread_info);

	thread_info->result = thread_info->start_routine(thread_info->arg);

	return 0;
}

int
os_thread_create(os_thread_t *thread, const os_thread_attr_t *attr,
	void *(*start_routine)(void *), void *arg)
{
	os_thread_info *thread_info;

	ASSERT(attr == NULL);

	if ((thread_info = malloc(sizeof(os_thread_info))) == NULL)
		return EAGAIN;

	thread_info->start_routine = start_routine;
	thread_info->arg = arg;

	thread_info->thread_handle = (HANDLE)_beginthreadex(NULL, 0,
		os_thread_start_routine_wrapper, thread_info, CREATE_SUSPENDED,
		NULL);
	if (thread_info->thread_handle == 0) {
		free(thread_info);
		return errno;
	}

	if (ResumeThread(thread_info->thread_handle) == -1) {
		free(thread_info);
		return EAGAIN;
	}

	*thread = (os_thread_t)thread_info;

	return 0;
}

int
os_thread_join(os_thread_t thread, void **result)
{
	WaitForSingleObject(thread->thread_handle, INFINITE);
	CloseHandle(thread->thread_handle);

	if (result != NULL)
		*result = thread->result;

	free(thread);

	return 0;
}

void
OS_CPU_ZERO(os_cpu_set_t *set)
{
	memset(set, 0, sizeof(*set));
}

void
OS_CPU_SET(int cpu, os_cpu_set_t *set)
{
	ASSERT(cpu < sizeof(*set) * 8);
	*set |= 1LL << cpu;
}

int
os_thread_setaffinity_np(os_thread_t thread, size_t set_size,
	const os_cpu_set_t *set)
{
	DWORD_PTR result = SetThreadAffinityMask(thread->thread_handle, *set);
	return result != 0 ? 0 : EINVAL;
}
