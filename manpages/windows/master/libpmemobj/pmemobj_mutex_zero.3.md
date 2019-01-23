---
layout: manual
Content-Style: 'text/css'
title: PMEMOBJ_MUTEX_ZERO
collection: libpmemobj
header: PMDK
date: pmemobj API version 2.3
...

[comment]: <> (Copyright 2017-2018, Intel Corporation)

[comment]: <> (Redistribution and use in source and binary forms, with or without)
[comment]: <> (modification, are permitted provided that the following conditions)
[comment]: <> (are met:)
[comment]: <> (    * Redistributions of source code must retain the above copyright)
[comment]: <> (      notice, this list of conditions and the following disclaimer.)
[comment]: <> (    * Redistributions in binary form must reproduce the above copyright)
[comment]: <> (      notice, this list of conditions and the following disclaimer in)
[comment]: <> (      the documentation and/or other materials provided with the)
[comment]: <> (      distribution.)
[comment]: <> (    * Neither the name of the copyright holder nor the names of its)
[comment]: <> (      contributors may be used to endorse or promote products derived)
[comment]: <> (      from this software without specific prior written permission.)

[comment]: <> (THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS)
[comment]: <> ("AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT)
[comment]: <> (LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR)
[comment]: <> (A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT)
[comment]: <> (OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,)
[comment]: <> (SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT)
[comment]: <> (LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,)
[comment]: <> (DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY)
[comment]: <> (THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT)
[comment]: <> ((INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE)
[comment]: <> (OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.)

[comment]: <> (pmemobj_mutex_zero.3 -- man page for locking functions from libpmemobj library)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmemobj_mutex_zero**(), **pmemobj_mutex_lock**(), **pmemobj_mutex_timedlock**(),
**pmemobj_mutex_trylock**(), **pmemobj_mutex_unlock**(),

**pmemobj_rwlock_zero**(), **pmemobj_rwlock_rdlock**(), **pmemobj_rwlock_wrlock**(),
**pmemobj_rwlock_timedrdlock**(), **pmemobj_rwlock_timedwrlock**(), **pmemobj_rwlock_tryrdlock**(),
**pmemobj_rwlock_trywrlock**(), **pmemobj_rwlock_unlock**(),

**pmemobj_cond_zero**(), **pmemobj_cond_broadcast**(), **pmemobj_cond_signal**(),
**pmemobj_cond_timedwait**(), **pmemobj_cond_wait**()
- pmemobj synchronization primitives


# SYNOPSIS #

```c
#include <libpmemobj.h>

void pmemobj_mutex_zero(PMEMobjpool *pop, PMEMmutex *mutexp);
int pmemobj_mutex_lock(PMEMobjpool *pop, PMEMmutex *mutexp);
int pmemobj_mutex_timedlock(PMEMobjpool *pop, PMEMmutex *restrict mutexp,
	const struct timespec *restrict abs_timeout);
int pmemobj_mutex_trylock(PMEMobjpool *pop, PMEMmutex *mutexp);
int pmemobj_mutex_unlock(PMEMobjpool *pop, PMEMmutex *mutexp);

void pmemobj_rwlock_zero(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_rdlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_wrlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_timedrdlock(PMEMobjpool *pop, PMEMrwlock *restrict rwlockp,
	const struct timespec *restrict abs_timeout);
int pmemobj_rwlock_timedwrlock(PMEMobjpool *pop, PMEMrwlock *restrict rwlockp,
	const struct timespec *restrict abs_timeout);
int pmemobj_rwlock_tryrdlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_trywrlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_unlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);

void pmemobj_cond_zero(PMEMobjpool *pop, PMEMcond *condp);
int pmemobj_cond_broadcast(PMEMobjpool *pop, PMEMcond *condp);
int pmemobj_cond_signal(PMEMobjpool *pop, PMEMcond *condp);
int pmemobj_cond_timedwait(PMEMobjpool *pop, PMEMcond *restrict condp,
	PMEMmutex *restrict mutexp, const struct timespec *restrict abs_timeout);
int pmemobj_cond_wait(PMEMobjpool *pop, PMEMcond *restrict condp,
	PMEMmutex *restrict mutexp);
```


# DESCRIPTION #

**libpmemobj**(7) provides several types of synchronization primitives
designed to be used with persistent memory. The pmem-aware lock implementation
is based on the standard POSIX Threads Library, as described in
**pthread_mutex_init**(3), **pthread_rwlock_init**(3) and
**pthread_cond_init**(3). Pmem-aware locks provide semantics similar to
standard **pthread** locks, except that they are embedded in pmem-resident
objects and are considered initialized by zeroing them. Therefore, locks
allocated with **pmemobj_zalloc**(3) or **pmemobj_tx_zalloc**(3) do not require
another initialization step. For performance reasons, they are also padded up
to 64 bytes (cache line size).

On FreeBSD, since all **pthread** locks are dynamically
allocated, while the lock object is still padded up to 64 bytes
for consistency with Linux, only the pointer to the lock is embedded in the
pmem-resident object. **libpmemobj**(7) transparently manages freeing of the
locks when the pool is closed.

The fundamental property of pmem-aware locks is their automatic
reinitialization every time the persistent object store pool is opened. Thus,
all the pmem-aware locks may be considered initialized (unlocked) immediately
after the pool is opened, regardless of their state at the time the pool was
closed for the last time.

Pmem-aware mutexes, read/write locks and condition variables must be declared
with the *PMEMmutex*, *PMEMrwlock*, or *PMEMcond* type, respectively.

The **pmemobj_mutex_zero**() function explicitly initializes the pmem-aware
mutex *mutexp* by zeroing it. Initialization is not necessary if the object
containing the mutex has been allocated using **pmemobj_zalloc**(3) or
**pmemobj_tx_zalloc**(3).

The **pmemobj_mutex_lock**() function locks the pmem-aware mutex *mutexp*.
If the mutex is already locked, the calling thread will block until the mutex
becomes available. If this is the first use of the mutex since the opening of
the pool *pop*, the mutex is automatically reinitialized and then locked.

**pmemobj_mutex_timedlock**() performs the same action as
**pmemobj_mutex_lock**(), but will not wait beyond *abs_timeout* to obtain the
lock before returning.

The **pmemobj_mutex_trylock**() function locks pmem-aware mutex *mutexp*.
If the mutex is already locked, **pthread_mutex_trylock**() will not block
waiting for the mutex, but will return an error. If this is the first
use of the mutex since the opening of the pool *pop*, the mutex is
automatically reinitialized and then locked.

The **pmemobj_mutex_unlock**() function unlocks the pmem-aware mutex
*mutexp*. Undefined behavior follows if a thread tries to unlock a
mutex that has not been locked by it, or if a thread tries to release a mutex
that is already unlocked or has not been initialized.

The **pmemobj_rwlock_zero**() function is used to explicitly initialize the
pmem-aware read/write lock *rwlockp* by zeroing it. Initialization is not
necessary if the object containing the lock has been allocated using
**pmemobj_zalloc**(3) or **pmemobj_tx_zalloc**(3).

The **pmemobj_rwlock_rdlock**() function acquires a read lock on *rwlockp*,
provided that the lock is not presently held for writing and no writer threads
are presently blocked on the lock. If the read lock cannot be acquired
immediately, the calling thread blocks until it can acquire the lock. If this
is the first use of the lock since the opening of the pool *pop*, the lock is
automatically reinitialized and then acquired.

**pmemobj_rwlock_timedrdlock**() performs the same action as
**pmemobj_rwlock_rdlock**(), but will not wait beyond *abs_timeout* to obtain
the lock before returning. A thread may hold multiple concurrent read locks.
If so, **pmemobj_rwlock_unlock**() must be called once for each lock obtained.
The results of acquiring a read lock while the calling thread holds a write
lock are undefined.

The **pmemobj_rwlock_wrlock**() function blocks until a write lock can be
acquired against read/write lock *rwlockp*. If this is the first use of the
lock since the opening of the pool *pop*, the lock is automatically
reinitialized and then acquired.

**pmemobj_rwlock_timedwrlock**() performs the same action, but will not wait
beyond *abs_timeout* to obtain the lock before returning.

The **pmemobj_rwlock_tryrdlock**() function performs the same action as
**pmemobj_rwlock_rdlock**(), but does not block if the lock cannot be
immediately obtained. The results are undefined if the calling thread already
holds the lock at the time the call is made.

The **pmemobj_rwlock_trywrlock**() function performs the same action as
**pmemobj_rwlock_wrlock**(), but does not block if the lock cannot be immediately
obtained. The results are undefined if the calling thread already holds the lock
at the time the call is made.

The **pmemobj_rwlock_unlock**() function is used to release the read/write
lock previously obtained by **pmemobj_rwlock_rdlock**(),
**pmemobj_rwlock_wrlock**(), **pthread_rwlock_tryrdlock**(), or
**pmemobj_rwlock_trywrlock**().

The **pmemobj_cond_zero**() function explicitly initializes the pmem-aware
condition variable *condp* by zeroing it. Initialization is not necessary if
the object containing the condition variable has been allocated using
**pmemobj_zalloc**(3) or **pmemobj_tx_zalloc**(3).

The difference between **pmemobj_cond_broadcast**() and
**pmemobj_cond_signal**() is that the former unblocks all threads waiting
for the condition variable, whereas the latter blocks only one waiting thread.
If no threads are waiting on *condp*, neither function has any effect. If more
than one thread is blocked on a condition variable, the used scheduling policy
determines the order in which threads are unblocked. The same mutex used for
waiting must be held while calling either function. Although neither function
strictly enforces this requirement, undefined behavior may follow if the mutex
is not held.

The **pmemobj_cond_timedwait**() and **pmemobj_cond_wait**() functions block
on a condition variable. They must be called with mutex *mutexp* locked by
the calling thread, or undefined behavior results. These functions atomically
release mutex *mutexp* and cause the calling thread to block on the condition
variable *condp*; atomically here means "atomically with respect to access by
another thread to the mutex and then the condition variable". That is, if
another thread is able to acquire the mutex after the about-to-block thread
has released it, then a subsequent call to **pmemobj_cond_broadcast**() or
**pmemobj_cond_signal**() in that thread will behave as if it were issued
after the about-to-block thread has blocked. Upon successful return, the mutex
will be locked and owned by the calling thread.


# RETURN VALUE #

The **pmemobj_mutex_zero**(), **pmemobj_rwlock_zero**()
and **pmemobj_cond_zero**() functions return no value.

Other locking functions return 0 on success.  Otherwise, an error
number will be returned to indicate the error.


# SEE ALSO #

**pmemobj_tx_zalloc**(3), **pmemobj_zalloc**(3), **pthread_cond_init**(3),
**pthread_mutex_init**(3), **pthread_rwlock_init**(3), **libpmem**(7),
**libpmemobj**(7) and **<http://pmem.io>**
