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
 * Persistent_ptr allocation functions for arrays. The typical usage examples
 * would be:
 * @snippet doc_snippets/make_persistent.cpp make_array_example
 */

#ifndef PMEMOBJ_MAKE_PERSISTENT_ARRAY_HPP
#define PMEMOBJ_MAKE_PERSISTENT_ARRAY_HPP

#include "libpmemobj.h"
#include "libpmemobj/detail/array_traits.hpp"
#include "libpmemobj/detail/check_persistent_ptr_array.hpp"
#include "libpmemobj/detail/common.hpp"
#include "libpmemobj/detail/destroyer.hpp"
#include "libpmemobj/detail/pexceptions.hpp"

namespace nvml
{

namespace obj
{

/**
 * Transactionally allocate and construct an array of objects of type T.
 *
 * This function can be used to *transactionally* allocate an array.
 * Cannot be used for simple objects.
 *
 * @param[in] N the number of array elements.
 *
 * @return persistent_ptr<T[]> on success
 *
 * @throw transaction_scope_error if called outside of an active
 * transaction
 * @throw transaction_alloc_error on transactional allocation failure.
 */
template <typename T>
typename detail::pp_if_array<T>::type
make_persistent(std::size_t N)
{
	typedef typename detail::pp_array_type<T>::type I;

	if (pmemobj_tx_stage() != TX_STAGE_WORK)
		throw transaction_scope_error(
			"refusing to allocate "
			"memory outside of transaction scope");

	persistent_ptr<T> ptr =
		pmemobj_tx_alloc(sizeof(I) * N, detail::type_num<I>());

	if (ptr == nullptr)
		throw transaction_alloc_error("failed to allocate "
					      "persistent memory array");

	std::size_t i;
	try {
		for (i = 0; i < N; ++i)
			::new (ptr.get() + i) I();
	} catch (...) {
		for (std::size_t j = 1; j <= i; ++j)
			detail::destroy<I>(ptr[i - j]);
		pmemobj_tx_free(*ptr.raw_ptr());
		throw;
	}

	return ptr;
}

/**
 * Transactionally allocate and construct an array of objects of type T.
 *
 * This function can be used to *transactionally* allocate an array.
 * Cannot be used for simple objects.
 *
 * @return persistent_ptr<T[N]> on success
 *
 * @throw transaction_scope_error if called outside of an active
 * transaction
 * @throw transaction_alloc_error on transactional allocation failure.
 */
template <typename T>
typename detail::pp_if_size_array<T>::type
make_persistent()
{
	typedef typename detail::pp_array_type<T>::type I;
	enum { N = detail::pp_array_elems<T>::elems };

	if (pmemobj_tx_stage() != TX_STAGE_WORK)
		throw transaction_scope_error(
			"refusing to allocate "
			"memory outside of transaction scope");

	persistent_ptr<T> ptr =
		pmemobj_tx_alloc(sizeof(I) * N, detail::type_num<I>());

	if (ptr == nullptr)
		throw transaction_alloc_error("failed to allocate "
					      "persistent memory array");

	std::size_t i;
	try {
		for (i = 0; i < N; ++i)
			::new (ptr.get() + i) I();
	} catch (...) {
		for (std::size_t j = 1; j <= i; ++j)
			detail::destroy<I>(ptr[i - j]);
		pmemobj_tx_free(*ptr.raw_ptr());
		throw;
	}

	return ptr;
}

/**
 * Transactionally free an array of objects of type T held
 * in a persitent_ptr.
 *
 * This function can be used to *transactionally* free an array of
 * objects. Calls the objects' destructors before freeing memory.
 * Cannot be used for single objects.
 *
 * @param[in,out] ptr persistent pointer to an array of objects.
 * @param[in] N the size of the array.
 *
 * @throw transaction_scope_error if called outside of an active
 * transaction
 * @throw transaction_alloc_error on transactional free failure.
 */
template <typename T>
void
delete_persistent(typename detail::pp_if_array<T>::type ptr, std::size_t N)
{
	typedef typename detail::pp_array_type<T>::type I;

	if (pmemobj_tx_stage() != TX_STAGE_WORK)
		throw transaction_scope_error(
			"refusing to free "
			"memory outside of transaction scope");

	if (ptr == nullptr)
		return;

	for (std::size_t i = 0; i < N; ++i)
		detail::destroy<I>(ptr[N - 1 - i]);

	if (pmemobj_tx_free(*ptr.raw_ptr()) != 0)
		throw transaction_alloc_error("failed to delete "
					      "persistent memory object");
}

/**
 * Transactionally free an array of objects of type T held
 * in a persitent_ptr.
 *
 * This function can be used to *transactionally* free an array of
 * objects. Calls the objects' destructors before freeing memory.
 * Cannot be used for single objects.
 *
 * @param[in,out] ptr persistent pointer to an array of objects.
 *
 * @throw transaction_scope_error if called outside of an active
 * transaction
 * @throw transaction_alloc_error on transactional free failure.
 */
template <typename T>
void
delete_persistent(typename detail::pp_if_size_array<T>::type ptr)
{
	typedef typename detail::pp_array_type<T>::type I;
	enum { N = detail::pp_array_elems<T>::elems };

	if (pmemobj_tx_stage() != TX_STAGE_WORK)
		throw transaction_scope_error(
			"refusing to free "
			"memory outside of transaction scope");

	if (ptr == nullptr)
		return;

	for (std::size_t i = 0; i < N; ++i)
		detail::destroy<I>(ptr[N - 1 - i]);

	if (pmemobj_tx_free(*ptr.raw_ptr()) != 0)
		throw transaction_alloc_error("failed to delete "
					      "persistent memory object");
}

} /* namespace obj */

} /* namespace nvml */

#endif /* PMEMOBJ_MAKE_PERSISTENT_ARRAY_HPP */
