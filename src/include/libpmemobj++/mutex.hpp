/*
 * Copyright 2016-2017, Intel Corporation
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
 * Pmem-resident mutex.
 */

#ifndef PMEMOBJ_MUTEX_HPP
#define PMEMOBJ_MUTEX_HPP

#include "libpmemobj++/detail/pexceptions.hpp"
#include "libpmemobj/thread.h"
#include "libpmemobj/tx_base.h"

namespace pmem
{

namespace obj
{

/**
 * Persistent memory resident mutex implementation.
 *
 * This class is an implementation of a PMEM-resident mutex
 * which mimics in behavior the C++11 std::mutex. This class
 * satisfies all requirements of the Mutex and StandardLayoutType
 * concepts. The typical usage example would be:
 * @snippet doc_snippets/mutex.cpp unique_guard_example
 */
class mutex {
public:
	/** Implementation defined handle to the native type. */
	typedef PMEMmutex *native_handle_type;

	/**
	 * Default constructor.
	 *
	 * @throw lock_error when the mutex is not from persistent memory.
	 */
	mutex()
	{
		PMEMobjpool *pop;
		if ((pop = pmemobj_pool_by_ptr(&plock)) == nullptr)
			throw lock_error(1, std::generic_category(),
					 "Persistent mutex not from persistent"
					 "memory.");

		pmemobj_mutex_zero(pop, &plock);
	}

	/**
	 * Defaulted destructor.
	 */
	~mutex() = default;

	/**
	 * Locks the mutex, blocks if already locked.
	 *
	 * If a different thread already locked this mutex, the calling
	 * thread will block. If the same thread tries to lock a mutex
	 * it already owns, the behavior is undefined.
	 *
	 * @throw lock_error when an error occurs, this includes all
	 * system related errors with the underlying implementation of
	 * the mutex.
	 */
	void
	lock()
	{
		PMEMobjpool *pop = pmemobj_pool_by_ptr(this);
		if (int ret = pmemobj_mutex_lock(pop, &this->plock))
			throw lock_error(ret, std::system_category(),
					 "Failed to lock a mutex.");
	}

	/**
	 * Tries to lock the mutex, returns regardless if the lock
	 * succeeds.
	 *
	 * If the same thread tries to lock a mutex it already owns,
	 * the behavior is undefined.
	 *
	 * @return `true` on successful lock acquisition, `false`
	 * otherwise.
	 *
	 * @throw lock_error when an error occurs, this includes all
	 * system related errors with the underlying implementation of
	 * the mutex.
	 */
	bool
	try_lock()
	{
		PMEMobjpool *pop = pmemobj_pool_by_ptr(this);
		int ret = pmemobj_mutex_trylock(pop, &this->plock);

		if (ret == 0)
			return true;
		else if (ret == EBUSY)
			return false;
		else
			throw lock_error(ret, std::system_category(),
					 "Failed to lock a mutex.");
	}

	/**
	 * Unlocks a previously locked mutex.
	 *
	 * Unlocking a mutex that has not been locked by the current
	 * thread results in undefined behavior. Unlocking a mutex that
	 * has not been lock also results in undefined behavior.
	 */
	void
	unlock()
	{
		PMEMobjpool *pop = pmemobj_pool_by_ptr(this);
		(void)pmemobj_mutex_unlock(pop, &this->plock);
	}

	/**
	 * Access a native handle to this condition variable.
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
	 * @return TX_PARAM_MUTEX
	 */
	enum pobj_tx_param
	lock_type() const noexcept
	{
		return TX_PARAM_MUTEX;
	}

	/**
	 * Deleted assignment operator.
	 */
	mutex &operator=(const mutex &) = delete;

	/**
	 * Deleted copy constructor.
	 */
	mutex(const mutex &) = delete;

private:
	/** A POSIX style PMEM-resident mutex.*/
	PMEMmutex plock;
};

} /* namespace obj */

} /* namespace pmem */

#endif /* PMEMOBJ_MUTEX_HPP */
