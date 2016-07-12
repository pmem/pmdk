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
 * Common array traits.
 */

#ifndef LIBPMEMOBJ_ARRAY_TRAITS_HPP
#define LIBPMEMOBJ_ARRAY_TRAITS_HPP

namespace nvml
{

namespace detail
{

/*
 * Returns the number of array elements.
 */
template <typename T>
struct pp_array_elems {
	enum { elems = 1 };
};

/*
 * Returns the number of array elements.
 */
template <typename T, size_t N>
struct pp_array_elems<T[N]> {
	enum { elems = N };
};

/*
 * Returns the type of elements in an array.
 */
template <typename T>
struct pp_array_type;

/*
 * Returns the type of elements in an array.
 */
template <typename T>
struct pp_array_type<T[]> {
	typedef T type;
};

/*
 * Returns the type of elements in an array.
 */
template <typename T, size_t N>
struct pp_array_type<T[N]> {
	typedef T type;
};

} /* namespace detail */

} /* namespace nvml */

#endif /* LIBPMEMOBJ_ARRAY_TRAITS_HPP */
