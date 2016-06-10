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
 * Atomic persistent_ptr allocation functions for arrays. The typical usage
 * examples would be:
 * @snippet doc_snippets/make_persistent.cpp make_array_atomic_example
 */

#ifndef PMEMOBJ_MAKE_PERSISTENT_ARRAY_ATOMIC_HPP
#define PMEMOBJ_MAKE_PERSISTENT_ARRAY_ATOMIC_HPP

#include "libpmemobj.h"
#include "libpmemobj/detail/array_traits.hpp"
#include "libpmemobj/detail/check_persistent_ptr_array.hpp"
#include "libpmemobj/detail/common.hpp"
#include "libpmemobj/detail/make_atomic_impl.hpp"
#include "libpmemobj/detail/pexceptions.hpp"

namespace nvml
{

namespace obj
{

/**
 * Atomically allocate an array of objects.
 *
 * This function can be used to atomically allocate an array of objects.
 * Cannot be used for simple objects. Do *NOT* use this inside transactions, as
 * it might lead to undefined behavior in the presence of transaction aborts.
 *
 * @param[in,out] pool the pool from which the object will be allocated.
 * @param[in,out] ptr the persistent pointer to which the allocation
 *	will take place.
 * @param[in] N the number of array elements.
 *
 * @throw std::bad_alloc on allocation failure.
 */
template <typename T>
void
make_persistent_atomic(pool_base &pool,
		       typename detail::pp_if_array<T>::type &ptr,
		       std::size_t N)
{
	typedef typename detail::pp_array_type<T>::type I;

	auto ret = pmemobj_alloc(pool.get_handle(), ptr.raw_ptr(),
				 sizeof(I) * N, detail::type_num<I>(),
				 &detail::array_constructor<I>,
				 static_cast<void *>(&N));

	if (ret != 0)
		throw std::bad_alloc();
}

/**
 * Atomically allocate an array of objects.
 *
 * This function can be used to atomically allocate an array of objects.
 * Cannot be used for simple objects. Do *NOT* use this inside transactions, as
 * it might lead to undefined behavior in the presence of transaction aborts.
 *
 * @param[in,out] pool the pool from which the object will be allocated.
 * @param[in,out] ptr the persistent pointer to which the allocation
 *	will take place.
 *
 * @throw std::bad_alloc on allocation failure.
 */
template <typename T>
void
make_persistent_atomic(pool_base &pool,
		       typename detail::pp_if_size_array<T>::type &ptr)
{
	typedef typename detail::pp_array_type<T>::type I;
	std::size_t N = detail::pp_array_elems<T>::elems;

	auto ret = pmemobj_alloc(pool.get_handle(), ptr.raw_ptr(),
				 sizeof(I) * N, detail::type_num<I>(),
				 &detail::array_constructor<I>,
				 static_cast<void *>(&N));

	if (ret != 0)
		throw std::bad_alloc();
}

/**
 * Atomically deallocate an array of objects.
 *
 * There is no way to atomically destroy an object. Any object specific
 * cleanup must be performed elsewhere. Do *NOT* use this inside transactions,
 * as it might lead to undefined behavior in the presence of transaction aborts.
 *
 * param[in,out] ptr the persistent_ptr whose pointee is to be
 * deallocated.
 */
template <typename T>
void
delete_persistent_atomic(typename detail::pp_if_array<T>::type &ptr,
			 std::size_t N)
{
	if (ptr == nullptr)
		return;

	/* we CAN'T call destructor */
	pmemobj_free(ptr.raw_ptr());
}

/**
 * Atomically deallocate an array of objects.
 *
 * There is no way to atomically destroy an object. Any object specific
 * cleanup must be performed elsewhere. Do *NOT* use this inside transactions,
 * as it might lead to undefined behavior in the presence of transaction aborts.
 *
 * param[in,out] ptr the persistent_ptr whose pointee is to be deallocated.
 */
template <typename T>
void
delete_persistent_atomic(typename detail::pp_if_size_array<T>::type &ptr)
{
	if (ptr == nullptr)
		return;

	/* we CAN'T call destructor */
	pmemobj_free(ptr.raw_ptr());
}

} /* namespace obj */

} /* namespace nvml */

#endif /* PMEMOBJ_MAKE_PERSISTENT_ARRAY_ATOMIC_HPP */
