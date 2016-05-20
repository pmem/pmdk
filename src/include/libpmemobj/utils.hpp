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
 * Libpmemobj C++ utils.
 */
#ifndef LIBPMEMOBJ_UTILS_HPP
#define LIBPMEMOBJ_UTILS_HPP

#include "libpmemobj.h"
#include "libpmemobj/detail/pexceptions.hpp"
#include "libpmemobj/persistent_ptr.hpp"

namespace nvml
{

namespace obj
{

/**
 * Retrieve pool handle for the given pointer.
 *
 * @param[in] that pointer to an object from a persistent memory pool.
 *
 * @return handle to the pool containing the object.
 *
 * @throw `pool_error` if the given pointer does not belong to an open pool.
 */
template <typename T>
inline pool_base
pool_by_vptr(const T *that)
{
	auto pop = pmemobj_pool_by_ptr(that);
	if (!pop)
		throw pool_error("Object not in an open pool.");

	return pool_base(pop);
}

/**
 * Retrieve pool handle for the given persistent_ptr.
 *
 * @param[in] ptr pointer to an object from a persistent memory pool.
 *
 * @return handle to the pool containing the object.
 *
 * @throw `pool_error` if the given pointer does not belong to an open pool.
 */
template <typename T>
inline pool_base
pool_by_pptr(const persistent_ptr<T> ptr)
{
	auto pop = pmemobj_pool_by_oid(ptr.raw());
	if (!pop)
		throw pool_error("Object not in an open pool.");

	return pool_base(pop);
}

} /* namespace obj */

} /* namespace nvml */

#endif /* LIBPMEMOBJ_UTILS_HPP */
