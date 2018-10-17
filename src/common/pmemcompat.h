/*
 * Copyright 2016-2018, Intel Corporation
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
 * pmemcompat.h -- compatibility layer for libpmem* libraries
 */

#ifndef PMEMCOMPAT_H
#define PMEMCOMPAT_H
#include <windows.h>

struct iovec {
	void  *iov_base;
	size_t iov_len;
};

typedef int mode_t;
/*
 * XXX: this code will not work on windows if our library is included in
 * an extern block.
 */
#if defined(__cplusplus) && defined(_MSC_VER) && !defined(__typeof__)
#include <type_traits>
/*
 * These templates are used to remove a type reference(T&) which, in some
 * cases, is returned by decltype
 */
namespace pmem {

namespace detail {

template<typename T>
struct get_type {
	using type = T;
};

template<typename T>
struct get_type<T*> {
	using type = T*;
};

template<typename T>
struct get_type<T&> {
	using type = T;
};

} /* namespace detail */

} /* namespace pmem */

#define __typeof__(p) pmem::detail::get_type<decltype(p)>::type

#endif

#endif
