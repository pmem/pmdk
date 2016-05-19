/*
 * Copyright 2015-2016, Intel Corporation
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
 * Helper template for persistent ptr specialization.
 *
 * Based on Boost library smart_ptr implementation.
 */

#ifndef PMEMOBJ_SPECIALIZATION_HPP
#define PMEMOBJ_SPECIALIZATION_HPP

#include <memory>

namespace nvml
{

namespace detail
{
/* smart pointer specialization */

template <typename T>
struct sp_element {
	typedef T type;
};

template <typename T>
struct sp_element<T[]> {
	typedef T type;
};

template <typename T, std::size_t N>
struct sp_element<T[N]> {
	typedef T type;
};

/* sp_dereference is a return type of operator* */

template <typename T>
struct sp_dereference {
	typedef T &type;
};

template <>
struct sp_dereference<void> {
	typedef void type;
};

template <>
struct sp_dereference<void const> {
	typedef void type;
};

template <>
struct sp_dereference<void volatile> {
	typedef void type;
};

template <>
struct sp_dereference<void const volatile> {
	typedef void type;
};

template <typename T>
struct sp_dereference<T[]> {
	typedef void type;
};

template <typename T, std::size_t N>
struct sp_dereference<T[N]> {
	typedef void type;
};

/* sp_member_access is a return type of operator-> */

template <typename T>
struct sp_member_access {
	typedef T *type;
};

template <typename T>
struct sp_member_access<T[]> {
	typedef void type;
};

template <typename T, std::size_t N>
struct sp_member_access<T[N]> {
	typedef void type;
};

/* sp_array_access is a return type of operator[] */

template <typename T>
struct sp_array_access {
	typedef T &type;
};

template <typename T>
struct sp_array_access<T[]> {
	typedef T &type;
};

template <typename T, std::size_t N>
struct sp_array_access<T[N]> {
	typedef T &type;
};

/* sp_extent is used for operator[] index checking */

template <typename T>
struct sp_extent {
	enum _vt { value = 0 };
};

template <typename T, std::size_t N>
struct sp_extent<T[N]> {
	enum _vt { value = N };
};

} /* namespace detail */

} /* namespace nvml */

#endif /* PMEMOBJ_SPECIALIZATION_HPP */
