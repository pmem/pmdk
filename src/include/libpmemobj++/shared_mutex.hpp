/*
 * Copyright 2016, Intel Corporation
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

/**
 * @file
 * Pmem-resident shared mutex.
 */

#ifndef PMEMOBJ_SHARED_MUTEX_HPP
#define PMEMOBJ_SHARED_MUTEX_HPP

#include "libpmemobj.h"

namespace nvml
{

namespace obj
{

/**
 * Persistent memory resident shared_mutex implementation.
 *
 * This class is an implementation of a PMEM-resident share_mutex
 * which mimics in behavior the C++11 std::mutex. This class
 * satisfies all requirements of the SharedMutex and StandardLayoutType
 * concepts. The typical usage would be:
 * @snippet doc_snippets/mutex.cpp shared_mutex_example
 */
class shared_mutex {
public:
	/** Implementation defined handle to the native type. */
	typedef PMEMrwlock *native_handle_type;

	/**
	 * Defaulted constructor.
	 */
	shared_mutex() noexcept = default;

	/**
	 * Defaulted destructor.
	 */
	~shared_mutex() = default;

	/**
	 * Lock the mutex for exclusive access.
	 *
	 * If a different thread already locked this mutex, the calling
	 * thread will block. If the same thread tries to lock a mutex
	 * it already owns, either in exclusive or shared mode,
	 * the behavior is undefined.
	 *
	 * @throw lock_error when an error occurs, this includes all
	 * system related errors with the underlying implementation of
	 * the mutex.
	 */
	void
	lock()
	{
		PMEMobjpool *pop = pmemobj_pool_by_ptr(this);
		if (int ret = pmemobj_rwlock_wrlock(pop, &this->plock))
			throw lock_error(ret, std::system_category(),
					 "Failed to lock a "
					 "shared mutex.");
	}

	/**
	 * Lock the mutex for shared access.
	 *
	 * If a different thread already locked this mutex for exclusive
	 * access, the calling thread will block. If it was locked for
	 * shared access by a different thread, the lock will succeed.
	 *
	 * The mutex can be locked for shared access multiple times
	 * by the same thread. If so, the same number of unlocks must be
	 * made to unlock the mutex.
	 *
	 * @throw lock_error when an error occurs, this includes all
	 * system related errors with the underlying implementation of
	 * the mutex.
	 */
	void
	lock_shared()
	{
		PMEMobjpool *pop = pmemobj_pool_by_ptr(this);
		if (int ret = pmemobj_rwlock_rdlock(pop, &this->plock))
			throw lock_error(ret, std::system_category(),
					 "Failed to shared lock a "
					 "shared mutex.");
	}

	/**
	 * Try to lock the mutex for exclusive access, returns
	 * regardless if the lock succeeds.
	 *
	 * If a different thread already locked this mutex, the calling
	 * thread will return `false`. If the same thread tries to lock
	 * a mutex it already owns, either in exclusive or shared mode,
	 * the behavior is undefined.
	 *
	 * @throw lock_error when an error occurs, this includes all
	 * system related errors with the underlying implementation of
	 * the mutex.
	 */
	bool
	try_lock()
	{
		PMEMobjpool *pop = pmemobj_pool_by_ptr(this);
		int ret = pmemobj_rwlock_trywrlock(pop, &this->plock);

		if (ret == 0)
			return true;
		else if (ret == EBUSY)
			return false;
		else
			throw lock_error(ret, std::system_category(),
					 "Failed to lock a"
					 " shared mutex.");
	}

	/**
	 * Try to lock the mutex for shared access, returns
	 * regardless if the lock succeeds.
	 *
	 * The mutex can be locked for shared access multiple times
	 * by the same thread. If so, the same number of unlocks must be
	 * made to unlock the mutex.
	 *
	 * @return `false` if a different thread already locked the
	 * mutex for exclusive access, `true` otherwise.
	 *
	 * @throw lock_error when an error occurs, this includes all
	 * system related errors with the underlying implementation of
	 * the mutex.
	 */
	bool
	try_lock_shared()
	{
		PMEMobjpool *pop = pmemobj_pool_by_ptr(this);
		int ret = pmemobj_rwlock_tryrdlock(pop, &this->plock);

		if (ret == 0)
			return true;
		else if (ret == EBUSY)
			return false;
		else
			throw lock_error(ret, std::system_category(),
					 "Failed to lock a"
					 " shared mutex.");
	}

	/**
	 * Unlocks the mutex.
	 *
	 * The mutex must be locked for exclusive access by the calling
	 * thread, otherwise results in undefined behavior.
	 *
	 * @throw lock_error when an error occurs, this includes all
	 * system related errors with the underlying implementation of
	 * the mutex.
	 */
	void
	unlock()
	{
		PMEMobjpool *pop = pmemobj_pool_by_ptr(this);
		if (int ret = pmemobj_rwlock_unlock(pop, &this->plock))
			throw lock_error(ret, std::system_category(),
					 "Failed to unlock a"
					 " shared mutex.");
	}

	/**
	 * Unlocks the mutex.
	 *
	 * The mutex must be locked for shared access by the calling
	 * thread, otherwise results in undefined behavior.
	 *
	 * @throw lock_error when an error occurs, this includes all
	 * system related errors with the underlying implementation of
	 * the mutex.
	 */
	void
	unlock_shared()
	{
		this->unlock();
	}

	/**
	 * Access a native handle to this shared mutex.
	 *
	 * @return a pointer to PMEMmutex.
	 */
	native_handle_type
	native_handle() noexcept
	{
		return &this->plock;
	}

	/**
	 * The type of lock needed for the transaction API.
	 *
	 * @return TX_LOCK_RWLOCK
	 */
	enum pobj_tx_lock
	lock_type() const noexcept
	{
		return TX_LOCK_RWLOCK;
	}

	/**
	 * Deleted assignment operator.
	 */
	shared_mutex &operator=(const shared_mutex &) = delete;

	/**
	 * Deleted copy constructor.
	 */
	shared_mutex(const shared_mutex &) = delete;

private:
	/** A POSIX style PMEM-resident shared_mutex.*/
	PMEMrwlock plock;
};

} /* namespace obj */

} /* namespace nvml */

#endif /* PMEMOBJ_SHARED_MUTEX_HPP */
