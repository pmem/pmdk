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

/*
 * pool.hpp -- cpp pmemobj pool implementation
 */

#ifndef PMEMOBJ_POOL_HPP
#define PMEMOBJ_POOL_HPP

#include <sys/stat.h>

#include "libpmemobj.h"
#include "libpmemobj/detail/pexceptions.hpp"
#include "libpmemobj/persistent_ptr.hpp"

namespace nvml
{

namespace obj
{

	/*
	 * The non-template pool base class.
	 */
	class pool_base {
	public:
		/*
		 * Default virtual destructor.
		 */
		virtual ~pool_base() noexcept = default;

	protected:

		/*
		 * Protected constructor
		 *
		 * Prohibits base class object initialization.
		 */
		pool_base() = default;
	};

	/*
	 * Pmemobj pool class
	 *
	 * This class is the pmemobj pool handler. It provides basic primitives
	 * for operations on pmemobj pools. The template parameter defines the
	 * type of the root object within the pool.
	 */
	template<typename T>
	class pool : public pool_base
	{
	public:
		/*
		 * Default constructor.
		 */
		pool() : pop(nullptr) {}

		/*
		 * Default copy constructor.
		 */
		pool(const pool&) = default;

		/*
		 * Default move constructor.
		 */
		pool(pool&&) = default;

		/*
		 * Default copy assignment operator.
		 */
		pool &operator=(const pool&) = default;

		/*
		 * Default move assignment operator.
		 */
		pool &operator=(pool&&) = default;

		/*
		 * Retrieve pool's root object.
		 *
		 * Returns a persistent pointer to the root object.
		 */
		persistent_ptr<T> get_root()
		{
			persistent_ptr<T> root = pmemobj_root(this->pop,
					sizeof (T));
			return root;
		}

		/*
		 * Open an existing pool/pool set.
		 *
		 * Returns a handle to the opened pool.
		 *
		 * Throws a pool_error on any pool open error.
		 */
		static pool<T> open(const std::string &path,
				const std::string &layout)
		{
			pmemobjpool *pop = pmemobj_open(path.c_str(),
					layout.c_str());
			if (pop == nullptr)
				throw pool_error("Failed opening pool");

			return pool(pop);
		}

		/*
		 * Create a new pool/pool set.
		 *
		 * Returns a handle to the created pool.
		 *
		 * Throws a pool_error on any pool create error.
		 */
		static pool<T> create(const std::string &path,
				const std::string &layout,
				std::size_t size = PMEMOBJ_MIN_POOL,
				mode_t mode = S_IWUSR | S_IRUSR)
		{
			pmemobjpool *pop = pmemobj_create(path.c_str(),
					layout.c_str(), size, mode);
			if (pop == nullptr)
				throw pool_error("Failed creating pool");

			return pool(pop);
		}

		/*
		 * Check if a given pool is consistent.
		 *
		 * Returns -1 on error, 1 if file is consistent, 0 otherwise.
		 */
		static int check(const std::string &path,
				const std::string &layout)
		{
			return pmemobj_check(path.c_str(), layout.c_str());
		}

		/*
		 * Closes the pool.
		 *
		 * Throws std::logic_error if the pool has already been closed.
		 */
		void close()
		{
			if (this->pop == nullptr)
				throw std::logic_error("Pool already closed");

			pmemobj_close(this->pop);
			this->pop = nullptr;
		}

		/*
		 * Get the C style handle to the pool.
		 *
		 * Necessary to be able to use the pool with the C API.
		 *
		 * Returns the pools opaque handle.
		 */
		PMEMobjpool *get_handle() noexcept
		{
			return this->pop;
		}

	private:
		/*
		 * Private constructor.
		 *
		 * Enforce using factory methods for object creation.
		 */
		pool(pmemobjpool *_pop) : pop(_pop)
		{
		}

		/* The pool opaque handle */
		PMEMobjpool *pop;
	};

} /* namespace obj */

} /* namespace nvml */

#endif /* PMEMOBJ_POOL_HPP */
