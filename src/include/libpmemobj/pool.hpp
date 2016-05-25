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
 * C++ pmemobj pool.
 */

#ifndef PMEMOBJ_POOL_HPP
#define PMEMOBJ_POOL_HPP

#include <sys/stat.h>

#include "libpmemobj.h"
#include "libpmemobj/detail/pexceptions.hpp"
#include "libpmemobj/p.hpp"

namespace nvml
{

namespace obj
{
template <typename T>
class persistent_ptr;

/**
 * The non-template pool base class.
 *
 * This class is a non-template version of pool. It is useful for places
 * where providing pool template argument is undesirable. The typical usage
 * example would be:
 * @snippet doc_snippets/pool.cpp pool_base_example
 */
class pool_base {
public:
	/**
	 * Defaulted constructor.
	 */
	pool_base() noexcept : pop(nullptr)
	{
	}

	/**
	 * Explicit constructor.
	 *
	 * Create pool_base object based on C-style pool handle.
	 *
	 * @param cpop C-style pool handle.
	 */
	explicit pool_base(pmemobjpool *cpop) noexcept : pop(cpop)
	{
	}

	/**
	 * Defaulted copy constructor.
	 */
	pool_base(const pool_base &) noexcept = default;

	/**
	 * Defaulted move constructor.
	 */
	pool_base(pool_base &&) noexcept = default;

	/**
	 * Defaulted copy assignment operator.
	 */
	pool_base &operator=(const pool_base &) noexcept = default;

	/**
	 * Defaulted move assignment operator.
	 */
	pool_base &operator=(pool_base &&) noexcept = default;

	/**
	 * Default virtual destructor.
	 */
	virtual ~pool_base() noexcept = default;

	/**
	 * Opens an existing object store memory pool.
	 *
	 * @param path System path to the file containing the memory
	 *	pool or a pool set.
	 * @param layout Unique identifier of the pool as specified at
	 *	pool creation time.
	 *
	 * @return handle to the opened pool.
	 *
	 * @throw nvml::pool_error when an error during opening occurs.
	 */
	static pool_base
	open(const std::string &path, const std::string &layout)
	{
		pmemobjpool *pop = pmemobj_open(path.c_str(), layout.c_str());
		if (pop == nullptr)
			throw pool_error("Failed opening pool");

		return pool_base(pop);
	}

	/**
	 * Creates a new transactional object store pool.
	 *
	 * @param path System path to the file to be created. If exists
	 *	the pool can be created in-place depending on the size
	 *	parameter. Existing file must be zeroed.
	 * @param layout Unique identifier of the pool, can be a
	 *	NULL-terminated string.
	 * @param size Size of the pool in bytes. If zero and the file
	 *	exists the pool is created in-place.
	 * @param mode File mode for the new file.
	 *
	 * @return handle to the created pool.
	 *
	 * @throw nvml::pool_error when an error during creation occurs.
	 */
	static pool_base
	create(const std::string &path, const std::string &layout,
	       std::size_t size = PMEMOBJ_MIN_POOL,
	       mode_t mode = S_IWUSR | S_IRUSR)
	{
		pmemobjpool *pop = pmemobj_create(path.c_str(), layout.c_str(),
						  size, mode);
		if (pop == nullptr)
			throw pool_error("Failed creating pool");

		return pool_base(pop);
	}

	/**
	 * Checks if a given pool is consistent.
	 *
	 * @param path System path to the file containing the memory
	 *	pool or a pool set.
	 * @param layout Unique identifier of the pool as specified at
	 *	pool creation time.
	 *
	 * @return -1 on error, 1 if file is consistent, 0 otherwise.
	 */
	static int
	check(const std::string &path, const std::string &layout) noexcept
	{
		return pmemobj_check(path.c_str(), layout.c_str());
	}

	/**
	 * Closes the pool.
	 *
	 * @throw std::logic_error if the pool has already been closed.
	 */
	void
	close()
	{
		if (this->pop == nullptr)
			throw std::logic_error("Pool already closed");

		pmemobj_close(this->pop);
		this->pop = nullptr;
	}

	/**
	 * Performs persist operation on a given chunk of memory.
	 *
	 * @param[in] addr address of memory chunk
	 * @param[in] len size of memory chunk
	 */
	void
	persist(const void *addr, size_t len) noexcept
	{
		pmemobj_persist(this->pop, addr, len);
	}

	/**
	 * Performs persist operation on a given pmem property.
	 *
	 * @param[in] prop Resides on pmem property
	 */
	template <typename Y>
	void
	persist(const p<Y> &prop) noexcept
	{
		pmemobj_persist(this->pop, &prop, sizeof(Y));
	}

	/**
	 * Performs persist operation on a given persistent object.
	 *
	 * @param[in] ptr Persistent pointer to object
	 */
	template <typename Y>
	void
	persist(const persistent_ptr<Y> &ptr) noexcept
	{
		pmemobj_persist(this->pop, &ptr, sizeof(ptr));
	}

	/**
	 * Performs flush operation on a given chunk of memory.
	 *
	 * @param[in] addr address of memory chunk
	 * @param[in] len size of memory chunk
	 */
	void
	flush(const void *addr, size_t len) noexcept
	{
		pmemobj_flush(this->pop, addr, len);
	}

	/**
	 * Performs flush operation on a given pmem property.
	 *
	 * @param[in] prop Resides on pmem property
	 */
	template <typename Y>
	void
	flush(const p<Y> &prop) noexcept
	{
		pmemobj_flush(this->pop, &prop, sizeof(Y));
	}

	/**
	 * Performs flush operation on a given persistent object.
	 *
	 * @param[in] ptr Persistent pointer to object
	 */
	template <typename Y>
	void
	flush(const persistent_ptr<Y> &ptr) noexcept
	{
		pmemobj_flush(this->pop, &ptr, sizeof(ptr));
	}

	/**
	 * Performs drain operation.
	 */
	void
	drain(void) noexcept
	{
		pmemobj_drain(this->pop);
	}

	/**
	 * Performs memcpy and persist operation on a given chunk of
	 * memory.
	 *
	 * @param[in] dest destination memory address
	 * @param[in] src source memory address
	 * @param[in] len size of memory chunk
	 *
	 * @return A pointer to dest
	 */
	void *
	memcpy_persist(void *dest, const void *src, size_t len) noexcept
	{
		return pmemobj_memcpy_persist(this->pop, dest, src, len);
	}

	/**
	 * Performs memset and persist operation on a given chunk of
	 * memory.
	 *
	 * @param[in] dest destination memory address
	 * @param[in] c constant value to fill the memory
	 * @param[in] len size of memory chunk
	 *
	 * @return A pointer to dest
	 */
	void *
	memset_persist(void *dest, int c, size_t len) noexcept
	{
		return pmemobj_memset_persist(this->pop, dest, c, len);
	}

	/*
	 * Gets the C style handle to the pool.
	 *
	 * Necessary to be able to use the pool with the C API.
	 *
	 * @return pool opaque handle.
	 */
	PMEMobjpool *
	get_handle() noexcept
	{
		return this->pop;
	}

protected:
	/* The pool opaque handle */
	PMEMobjpool *pop;
};

/**
 * PMEMobj pool class
 *
 * This class is the pmemobj pool handler. It provides basic primitives
 * for operations on pmemobj pools. The template parameter defines the
 * type of the root object within the pool. The typical usage example would be:
 * @snippet doc_snippets/pool.cpp pool_example
 */
template <typename T>
class pool : public pool_base {
public:
	/**
	 * Defaulted constructor.
	 */
	pool() noexcept = default;

	/**
	 * Defaulted copy constructor.
	 */
	pool(const pool &) noexcept = default;

	/**
	 * Defaulted move constructor.
	 */
	pool(pool &&) noexcept = default;

	/**
	 * Defaulted copy assignment operator.
	 */
	pool &operator=(const pool &) noexcept = default;

	/**
	 * Defaulted move assignment operator.
	 */
	pool &operator=(pool &&) noexcept = default;

	/**
	 * Default destructor.
	 */
	~pool() noexcept = default;

	/**
	 * Defaulted copy constructor.
	 */
	explicit pool(const pool_base &pb) noexcept : pool_base(pb)
	{
	}

	/**
	 * Defaulted move constructor.
	 */
	explicit pool(pool_base &&pb) noexcept : pool_base(pb)
	{
	}

	/**
	 * Retrieves pool's root object.
	 *
	 * @return persistent pointer to the root object.
	 */
	persistent_ptr<T>
	get_root()
	{
		if (pop == nullptr)
			throw pool_error("Invalid pool handle");

		persistent_ptr<T> root = pmemobj_root(this->pop, sizeof(T));
		return root;
	}

	/**
	 * Opens an existing object store memory pool.
	 *
	 * @param path System path to the file containing the memory
	 *	pool or a pool set.
	 * @param layout Unique identifier of the pool as specified at
	 *	pool creation time.
	 *
	 * @return handle to the opened pool.
	 *
	 * @throw nvml::pool_error when an error during opening occurs.
	 */
	static pool<T>
	open(const std::string &path, const std::string &layout)
	{
		return pool<T>(pool_base::open(path, layout));
	}

	/**
	 * Creates a new transactional object store pool.
	 *
	 * @param path System path to the file to be created. If exists
	 *	the pool can be created in-place depending on the size
	 *	parameter. Existing file must be zeroed.
	 * @param layout Unique identifier of the pool, can be a
	 *	NULL-terminated string.
	 * @param size Size of the pool in bytes. If zero and the file
	 *	exists the pool is created in-place.
	 * @param mode File mode for the new file.
	 *
	 * @return handle to the created pool.
	 *
	 * @throw nvml::pool_error when an error during creation occurs.
	 */
	static pool<T>
	create(const std::string &path, const std::string &layout,
	       std::size_t size = PMEMOBJ_MIN_POOL,
	       mode_t mode = S_IWUSR | S_IRUSR)
	{
		return pool<T>(pool_base::create(path, layout, size, mode));
	}

	/**
	 * Checks if a given pool is consistent.
	 *
	 * @param path System path to the file containing the memory
	 *	pool or a pool set.
	 * @param layout Unique identifier of the pool as specified at
	 *	pool creation time.
	 *
	 * @return -1 on error, 1 if file is consistent, 0 otherwise.
	 */
	static int
	check(const std::string &path, const std::string &layout)
	{
		return pool_base::check(path, layout);
	}
};

} /* namespace obj */

} /* namespace nvml */

#endif /* PMEMOBJ_POOL_HPP */
