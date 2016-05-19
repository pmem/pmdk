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
 * Compile time type check for make_persistent.
 */

#ifndef LIBPMEMOBJ_CHECK_PERSISTENT_PTR_ARRAY_HPP
#define LIBPMEMOBJ_CHECK_PERSISTENT_PTR_ARRAY_HPP

#include "libpmemobj/persistent_ptr.hpp"

namespace nvml
{

namespace detail
{

/*
 * Typedef checking if given type is not an array.
 */
template <typename T>
struct pp_if_not_array {
	typedef obj::persistent_ptr<T> type;
};

/*
 * Typedef checking if given type is not an array.
 */
template <typename T>
struct pp_if_not_array<T[]> {
};

/*
 * Typedef checking if given type is not an array.
 */
template <typename T, size_t N>
struct pp_if_not_array<T[N]> {
};

/*
 * Typedef checking if given type is an array.
 */
template <typename T>
struct pp_if_array;

/*
 * Typedef checking if given type is an array.
 */
template <typename T>
struct pp_if_array<T[]> {
	typedef obj::persistent_ptr<T[]> type;
};

/*
 * Typedef checking if given type is an array.
 */
template <typename T>
struct pp_if_size_array;

/*
 * Typedef checking if given type is an array.
 */
template <typename T, size_t N>
struct pp_if_size_array<T[N]> {
	typedef obj::persistent_ptr<T[N]> type;
};

} /* namespace detail */

} /* namespace nvml */

#endif /* LIBPMEMOBJ_CHECK_PERSISTENT_PTR_ARRAY_HPP */
