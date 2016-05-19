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
 * Functions for destroying arrays.
 */

#ifndef LIBPMEMOBJ_DESTROYER_HPP
#define LIBPMEMOBJ_DESTROYER_HPP

#include "libpmemobj/detail/array_traits.hpp"

namespace nvml
{

namespace detail
{

/*
 * Template for checking if T is not an array.
 */
template <typename T>
struct if_not_array {
	typedef T type;
};

/*
 * Template for checking if T is not an array.
 */
template <typename T>
struct if_not_array<T[]>;

/*
 * Template for checking if T is not an array.
 */
template <typename T, size_t N>
struct if_not_array<T[N]>;

/*
 * Template for checking if T is an array.
 */
template <typename T>
struct if_size_array;

/*
 * Template for checking if T is an array.
 */
template <typename T>
struct if_size_array<T[]>;

/*
 * Template for checking if T is an array.
 */
template <typename T, size_t N>
struct if_size_array<T[N]> {
	typedef T type[N];
};

/*
 * Calls object's destructor.
 */
template <typename T>
void
destroy(typename if_not_array<T>::type &arg)
{
	arg.~T();
}

/*
 * Recursively calls array's elements' destructors.
 */
template <typename T>
void
destroy(typename if_size_array<T>::type &arg)
{
	typedef typename detail::pp_array_type<T>::type I;
	enum { N = pp_array_elems<T>::elems };

	for (std::size_t i = 0; i < N; ++i)
		destroy<I>(arg[N - 1 - i]);
}

} /* namespace detail */

} /* namespace nvml */

#endif /* LIBPMEMOBJ_DESTROYER_HPP */
