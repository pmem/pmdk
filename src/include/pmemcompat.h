/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2023, Intel Corporation */

/*
 * pmemcompat.h -- compatibility layer for libpmem* libraries
 */

#ifndef PMEMCOMPAT_H
#define PMEMCOMPAT_H

/* for backward compatibility */
#ifdef NVML_UTF8_API
#pragma message( "NVML_UTF8_API macro is obsolete, please use PMDK_UTF8_API instead." )
#ifndef PMDK_UTF8_API
#define PMDK_UTF8_API
#endif
#endif

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
