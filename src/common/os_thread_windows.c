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
#include <synchapi.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include "os_thread.h"
#include "util.h"
#include "out.h"


typedef struct {
	unsigned attr;
	CRITICAL_SECTION lock;
} internal_os_mutex_t;

typedef struct {
	unsigned attr;
	char is_write;
	SRWLOCK lock;
} internal_os_rwlock_t;

typedef struct {
	unsigned attr;
	CONDITION_VARIABLE cond;
} internal_os_cond_t;

typedef struct {
	HANDLE handle;
} internal_semaphore_t;

typedef union {
	GROUP_AFFINITY affinity;
} internal_os_cpu_set_t;


typedef struct {
	HANDLE thread_handle;
	void *arg;
	void *(*start_routine)(void *);
	void *result;
} internal_os_thread_info, *internal_os_thread_t;

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

/*
 * os_mutex_init -- initializes mutex
 */
int
os_mutex_init(os_mutex_t *__restrict mutex)
{
	COMPILE_ERROR_ON(sizeof(os_mutex_t) < sizeof(internal_os_mutex_t));
	internal_os_mutex_t *mutex_internal = (internal_os_mutex_t *)mutex;
	InitializeCriticalSection(&mutex_internal->lock);
	return 0;
}

/*
 * os_mutex_destroy -- destroys mutex
 */
int
os_mutex_destroy(os_mutex_t *__restrict mutex)
{
	internal_os_mutex_t *mutex_internal = (internal_os_mutex_t *)mutex;
	DeleteCriticalSection(&mutex_internal->lock);
	return 0;
}

/*
 * os_mutex_lock -- locks mutex
 */
_Use_decl_annotations_
int
os_mutex_lock(os_mutex_t *__restrict mutex)
{
	internal_os_mutex_t *mutex_internal = (internal_os_mutex_t *)mutex;
	EnterCriticalSection(&mutex_internal->lock);

	if (mutex_internal->lock.RecursionCount > 1) {
		LeaveCriticalSection(&mutex_internal->lock);
		FATAL("deadlock detected");
	}
	return 0;
}

/*
 * os_mutex_trylock -- tries lock mutex
 */
_Use_decl_annotations_
int
os_mutex_trylock(os_mutex_t *__restrict mutex)
{
	internal_os_mutex_t *mutex_internal = (internal_os_mutex_t *)mutex;
	if (TryEnterCriticalSection(&mutex_internal->lock) == FALSE)
		return EBUSY;

	if (mutex_internal->lock.RecursionCount > 1) {
		LeaveCriticalSection(&mutex_internal->lock);
		return EBUSY;
	}

	return 0;
}

/*
 * os_mutex_timedlock -- tries lock mutex with timeout
 */
int
os_mutex_timedlock(os_mutex_t *__restrict mutex,
	const struct timespec *abstime)
{
	TIMED_LOCK((os_mutex_trylock(mutex) == 0), abstime);
}

/*
 * os_mutex_unlock -- unlocks mutex
 */
int
os_mutex_unlock(os_mutex_t *__restrict mutex)
{
	internal_os_mutex_t *mutex_internal = (internal_os_mutex_t *)mutex;
	LeaveCriticalSection(&mutex_internal->lock);
	return 0;
}

/*
 * os_rwlock_init -- initializes rwlock
 */
int
os_rwlock_init(os_rwlock_t *__restrict rwlock)
{
	COMPILE_ERROR_ON(sizeof(os_rwlock_t) < sizeof(internal_os_rwlock_t));
	internal_os_rwlock_t *rwlock_internal = (internal_os_rwlock_t *)rwlock;
	InitializeSRWLock(&rwlock_internal->lock);
	return 0;
}

/*
 * os_rwlock_destroy -- destroys rwlock
 */
int
os_rwlock_destroy(os_rwlock_t *__restrict rwlock)
{
	/* do nothing */
	UNREFERENCED_PARAMETER(rwlock);

	return 0;
}

/*
 * os_rwlock_rdlock -- get shared lock
 */
int
os_rwlock_rdlock(os_rwlock_t *__restrict rwlock)
{
	internal_os_rwlock_t *rwlock_internal = (internal_os_rwlock_t *)rwlock;
	AcquireSRWLockShared(&rwlock_internal->lock);
	rwlock_internal->is_write = 0;
	return 0;
}

/*
 * os_rwlock_wrlock -- get exclusive lock
 */
int
os_rwlock_wrlock(os_rwlock_t *__restrict rwlock)
{
	internal_os_rwlock_t *rwlock_internal = (internal_os_rwlock_t *)rwlock;
	AcquireSRWLockExclusive(&rwlock_internal->lock);
	rwlock_internal->is_write = 1;
	return 0;
}

/*
 * os_rwlock_tryrdlock -- tries get shared lock
 */
int
os_rwlock_tryrdlock(os_rwlock_t *__restrict rwlock)
{
	internal_os_rwlock_t *rwlock_internal = (internal_os_rwlock_t *)rwlock;
	if (TryAcquireSRWLockShared(&rwlock_internal->lock) == FALSE) {
		return EBUSY;
	} else {
		rwlock_internal->is_write = 0;
		return 0;
	}
}

/*
 * os_rwlock_trywrlock -- tries get exclusive lock
 */
_Use_decl_annotations_
int
os_rwlock_trywrlock(os_rwlock_t *__restrict rwlock)
{
	internal_os_rwlock_t *rwlock_internal = (internal_os_rwlock_t *)rwlock;
	if (TryAcquireSRWLockExclusive(&rwlock_internal->lock) == FALSE) {
		return EBUSY;
	} else {
		rwlock_internal->is_write = 1;
		return 0;
	}
}

/*
 * os_rwlock_timedrdlock -- gets shared lock with timeout
 */
int
os_rwlock_timedrdlock(os_rwlock_t *__restrict rwlock,
	const struct timespec *abstime)
{
	TIMED_LOCK((os_rwlock_tryrdlock(rwlock) == 0), abstime);
}

/*
 * os_rwlock_timedwrlock -- gets exclusive lock with timeout
 */
int
os_rwlock_timedwrlock(os_rwlock_t *__restrict rwlock,
	const struct timespec *abstime)
{
	TIMED_LOCK((os_rwlock_trywrlock(rwlock) == 0), abstime);
}

/*
 * os_rwlock_unlock -- unlocks rwlock
 */
_Use_decl_annotations_
int
os_rwlock_unlock(os_rwlock_t *__restrict rwlock)
{
	internal_os_rwlock_t *rwlock_internal = (internal_os_rwlock_t *)rwlock;
	if (rwlock_internal->is_write)
		ReleaseSRWLockExclusive(&rwlock_internal->lock);
	else
		ReleaseSRWLockShared(&rwlock_internal->lock);
	return 0;
}

/*
 * os_cond_init -- initializes condition variable
 */
int
os_cond_init(os_cond_t *__restrict cond)
{
	COMPILE_ERROR_ON(sizeof(os_cond_t) < sizeof(internal_os_cond_t));

	internal_os_cond_t *cond_internal = (internal_os_cond_t *)cond;
	InitializeConditionVariable(&cond_internal->cond);
	return 0;
}

/*
 * os_cond_destroy -- destroys condition variable
 */
int
os_cond_destroy(os_cond_t *__restrict cond)
{
	/* do nothing */
	UNREFERENCED_PARAMETER(cond);

	return 0;
}

/*
 * os_cond_broadcast -- broadcast condition variable
 */
int
os_cond_broadcast(os_cond_t *__restrict cond)
{
	internal_os_cond_t *cond_internal = (internal_os_cond_t *)cond;
	WakeAllConditionVariable(&cond_internal->cond);
	return 0;
}

/*
 * os_cond_wait -- signal condition variable
 */
int
os_cond_signal(os_cond_t *__restrict cond)
{
	internal_os_cond_t *cond_internal = (internal_os_cond_t *)cond;
	WakeConditionVariable(&cond_internal->cond);
	return 0;
}
/*
 * get_rel_wait -- (internal) convert timespec to windows timeout
 */
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

/*
 * os_cond_timedwait -- waits on condition variable with timeout
 */
int
os_cond_timedwait(os_cond_t *__restrict cond,
	os_mutex_t *__restrict mutex, const struct timespec *abstime)
{
	internal_os_cond_t *cond_internal = (internal_os_cond_t *)cond;
	internal_os_mutex_t *mutex_internal = (internal_os_mutex_t *)mutex;
	BOOL ret;
	SetLastError(0);
	ret = SleepConditionVariableCS(&cond_internal->cond,
		&mutex_internal->lock, get_rel_wait(abstime));
	if (ret == FALSE)
		return (GetLastError() == ERROR_TIMEOUT) ? ETIMEDOUT : EINVAL;

	return 0;
}

/*
 * os_cond_wait -- waits on condition variable
 */
int
os_cond_wait(os_cond_t *__restrict cond,
	os_mutex_t *__restrict mutex)
{
	internal_os_cond_t *cond_internal = (internal_os_cond_t *)cond;
	internal_os_mutex_t *mutex_internal = (internal_os_mutex_t *)mutex;
	/* XXX - return error code based on GetLastError() */
	BOOL ret;
	ret = SleepConditionVariableCS(&cond_internal->cond,
		&mutex_internal->lock, INFINITE);
	return (ret == FALSE) ? EINVAL : 0;
}

/*
 * os_once -- once-only function call
 */
int
os_once(os_once_t *once, void (*func)(void))
{
	os_once_t tmp;

	while ((tmp = *once) != 2) {
		if (tmp == 1)
			continue; /* another thread is already calling func() */

		/* try to be the first one... */
		if (!util_bool_compare_and_swap64(once, tmp, 1))
			continue; /* sorry, another thread was faster */

		func();

		if (!util_bool_compare_and_swap64(once, 1, 2)) {
			ERR("error setting once");
			return -1;
		}
	}

	return 0;
}

/*
 * According to MSDN, the maximum number of TLS indexes per process is 1088.
 */
#define TLS_KEYS_MAX 1088

struct key_dtor {
	os_tls_key_t key;
	void (*destructor)(void *);
};

/*
 * A list of all the TLS keys created by the process.
 * Actually it's a static array, as the number of keys is limited anyway.
 */
static struct key_dtor_list {
	size_t cnt;
	struct key_dtor keys[TLS_KEYS_MAX];
} Tls_keys;

/* TLS key list guard */
os_mutex_t Tls_lock;

/*
 * In case of static linking of NVM libraries, it may happen that
 * os_tls_init/os_tls_fini are called more than once per process
 * (i.e once from libpmem ctor/dtor and teh second time by the program itself).
 * This refcnt guarantees that only the last call to os_tls_fini would
 * actually destroy TLS data.
 */
static uint64_t Tls_refcnt;

os_once_t Tls_initialized;
os_once_t Tls_destroyed;

/*
 * tls_init -- initialize TLS key list
 *
 * Must be called only once.
 */
static void
tls_init(void)
{
	if (os_mutex_init(&Tls_lock) != 0)
		FATAL("TLS lock init");
}

/*
 * os_tls_init -- initialize TLS key list
 */
void
os_tls_init(void)
{
	os_once(&Tls_initialized, tls_init);
	util_fetch_and_add(&Tls_refcnt, 1);
}

/*
 * tls_fini -- destroy TLS key list
 *
 * Must be called only once.
 */
static void
tls_fini(void)
{
	if (os_mutex_destroy(&Tls_lock) != 0)
		FATAL("TLS lock destroy");
}

/*
 * os_tls_fini -- destroy TLS key list
 */
void
os_tls_fini(void)
{
	if (util_fetch_and_sub(&Tls_refcnt, 1) == 1)
		os_once(&Tls_destroyed, tls_fini); /* XXX */
}

/*
 * os_tls_thread_fini -- destroy TLS data
 *
 * Destroys all the TLS data by calling destructor for each key.
 */
void
os_tls_thread_fini(void)
{
	if (util_bool_compare_and_swap64(&Tls_refcnt, 0, 0))
		/* TLS data has been already destroyed */
		return;

	os_mutex_lock(&Tls_lock);

	for (int i = 0; i < Tls_keys.cnt; i++) {
		void *key = os_tls_get(Tls_keys.keys[i].key);
		if (key != NULL && Tls_keys.keys[i].destructor != NULL)
			Tls_keys.keys[i].destructor(key);
	}

	os_mutex_unlock(&Tls_lock);
}

/*
 * os_tls_key_insert -- (internal) insert a key to the list of tls keys
 */
static int
os_tls_key_insert(os_tls_key_t *key, void (*destructor)(void *))
{
	int ret = 0;

	if (util_bool_compare_and_swap64(&Tls_refcnt, 0, 0))
		/* TLS data has been already destroyed */
		return 0;

	os_mutex_lock(&Tls_lock);

	if (Tls_keys.cnt == TLS_KEYS_MAX) {
		ret = ENOMEM;
		goto exit;
	}

	Tls_keys.keys[Tls_keys.cnt].key = *key;
	Tls_keys.keys[Tls_keys.cnt].destructor = destructor;
	Tls_keys.cnt++;

exit:
	os_mutex_unlock(&Tls_lock);
	return ret;
}

/*
 * os_tls_key_remove -- (internal) removes a key from the list of tls keys
 */
static void
os_tls_key_remove(os_tls_key_t key)
{
	if (util_bool_compare_and_swap64(&Tls_refcnt, 0, 0))
		/* TLS data has been already destroyed */
		return;

	os_mutex_lock(&Tls_lock);

	for (int i = 0; i < Tls_keys.cnt; i++) {
		if (Tls_keys.keys[i].key == key) {
			/* move the list up */
			memmove(&Tls_keys.keys[i], &Tls_keys.keys[i + 1],
				sizeof(struct key_dtor) * Tls_keys.cnt - i - 1);
			Tls_keys.cnt--;
			os_mutex_unlock(&Tls_lock);
			return;
		}
	}

	os_mutex_unlock(&Tls_lock);
}

/*
 * os_tls_key_create -- creates a new tls key
 */
int
os_tls_key_create(os_tls_key_t *key, void (*destructor)(void *))
{
	*key = TlsAlloc();
	if (*key == TLS_OUT_OF_INDEXES)
		return EAGAIN;

	int ret = os_tls_key_insert(key, destructor);
	if (ret != 0) {
		(void) TlsFree(*key);
		return ret;
	}

	return 0;
}

/*
 * os_tls_key_delete -- deletes key from tls
 */
int
os_tls_key_delete(os_tls_key_t key)
{
	os_tls_key_remove(key);

	if (!TlsFree(key))
		return EINVAL;
	return 0;
}

/*
 * os_tls_set -- sets a value in tls
 */
int
os_tls_set(os_tls_key_t key, const void *value)
{
	if (!TlsSetValue(key, (LPVOID)value))
		return ENOENT;
	return 0;
}

/*
 * os_tls_get -- gets a value from tls
 */
void *
os_tls_get(os_tls_key_t key)
{
	return TlsGetValue(key);
}

/* threading */

/*
 * os_thread_start_routine_wrapper is a start routine for _beginthreadex() and
 * it helps:
 *
 *  - wrap the os_thread_create's start function
 */
static unsigned __stdcall
os_thread_start_routine_wrapper(void *arg)
{
	internal_os_thread_t thread_info = (internal_os_thread_t)arg;

	thread_info->result = thread_info->start_routine(thread_info->arg);

	return 0;
}

/*
 * os_thread_create -- starts a new thread
 */
int
os_thread_create(os_thread_t *thread, const os_thread_attr_t *attr,
	void *(*start_routine)(void *), void *arg)
{
	internal_os_thread_info *thread_info;

	ASSERT(attr == NULL);

	if ((thread_info = malloc(sizeof(internal_os_thread_info))) == NULL)
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

/*
 * os_thread_join -- joins a thread
 */
int
os_thread_join(os_thread_t thread, void **result)
{
	internal_os_thread_t internal_thread = (internal_os_thread_t)thread;
	WaitForSingleObject(internal_thread->thread_handle, INFINITE);
	CloseHandle(internal_thread->thread_handle);

	if (result != NULL)
		*result = internal_thread->result;

	free(internal_thread);

	return 0;
}

/*
 * os_cpu_zero -- clears cpu set
 */
void
os_cpu_zero(os_cpu_set_t *set)
{
	internal_os_cpu_set_t *internal_set = (internal_os_cpu_set_t *)set;

	memset(&internal_set->affinity, 0, sizeof(internal_set->affinity));
}

/*
 * os_cpu_set -- adds cpu to set
 */
void
os_cpu_set(size_t cpu, os_cpu_set_t *set)
{
	internal_os_cpu_set_t *internal_set = (internal_os_cpu_set_t *)set;
	int sum = 0;
	int group_max = GetActiveProcessorGroupCount();
	int group = 0;
	while (group < group_max) {
		sum += GetActiveProcessorCount(group);
		if (sum > cpu) {
			/*
			 * XXX: can't set affinity to two diffrent cpu groups
			 */
			if (internal_set->affinity.Group != group) {
				internal_set->affinity.Mask = 0;
				internal_set->affinity.Group = group;
			}

			cpu -= sum - GetActiveProcessorCount(group);
			internal_set->affinity.Mask |= 1LL << cpu;
			return;
		}

		group++;
	}
	FATAL("os_cpu_set cpu out of bounds");
}

/*
 * os_thread_setaffinity_np -- sets affinity of the thread
 */
int
os_thread_setaffinity_np(os_thread_t thread, size_t set_size,
	const os_cpu_set_t *set)
{
	internal_os_cpu_set_t *internal_set = (internal_os_cpu_set_t *)set;
	internal_os_thread_t internal_thread = (internal_os_thread_t)thread;

	int ret = SetThreadGroupAffinity(internal_thread->thread_handle,
			&internal_set->affinity, NULL);
	return ret != 0 ? 0 : EINVAL;
}

/*
 * os_semaphore_init -- initializes a new semaphore instance
 */
int
os_semaphore_init(os_semaphore_t *sem, unsigned value)
{
	internal_semaphore_t *internal_sem = (internal_semaphore_t *)sem;
	internal_sem->handle = CreateSemaphore(NULL,
		value, LONG_MAX, NULL);

	return internal_sem->handle != 0 ? 0 : -1;
}

/*
 * os_semaphore_destroy -- destroys a semaphore instance
 */
int
os_semaphore_destroy(os_semaphore_t *sem)
{
	internal_semaphore_t *internal_sem = (internal_semaphore_t *)sem;
	BOOL ret = CloseHandle(internal_sem->handle);
	return ret ? 0 : -1;
}

/*
 * os_semaphore_wait -- decreases the value of the semaphore
 */
int
os_semaphore_wait(os_semaphore_t *sem)
{
	internal_semaphore_t *internal_sem = (internal_semaphore_t *)sem;
	DWORD ret = WaitForSingleObject(internal_sem->handle, INFINITE);
	return ret == WAIT_OBJECT_0 ? 0 : -1;
}

/*
 * os_semaphore_trywait -- tries to decrease the value of the semaphore
 */
int
os_semaphore_trywait(os_semaphore_t *sem)
{
	internal_semaphore_t *internal_sem = (internal_semaphore_t *)sem;
	DWORD ret = WaitForSingleObject(internal_sem->handle, 0);

	if (ret == WAIT_TIMEOUT)
		errno = EAGAIN;

	return ret == WAIT_OBJECT_0 ? 0 : -1;
}

/*
 * os_semaphore_post -- increases the value of the semaphore
 */
int
os_semaphore_post(os_semaphore_t *sem)
{
	internal_semaphore_t *internal_sem = (internal_semaphore_t *)sem;
	BOOL ret = ReleaseSemaphore(internal_sem->handle, 1, NULL);
	return ret ? 0 : -1;
}
