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

#ifndef LIBPMEMOBJ_INTEGER_SEQUENCE_HPP
#define LIBPMEMOBJ_INTEGER_SEQUENCE_HPP

/**
 * @file
 * Create c++14 style index sequence.
 */

namespace nvml
{

namespace detail
{

/*
 * Base index type template.
 */
template <typename T, T...>
struct integer_sequence {
};

/*
 * Size_t specialization of the integer sequence.
 */
template <size_t... Indices>
using index_sequence = integer_sequence<size_t, Indices...>;

/*
 * Empty base class.
 *
 * Subject of empty base optimization.
 */
template <typename T, T I, typename... Types>
struct make_integer_seq_impl;

/*
 * Class ending recursive variadic template peeling.
 */
template <typename T, T I, T... Indices>
struct make_integer_seq_impl<T, I, integer_sequence<T, Indices...>> {
	typedef integer_sequence<T, Indices...> type;
};

/*
 * Recursively create index while peeling off the types.
 */
template <typename N, N I, N... Indices, typename T, typename... Types>
struct make_integer_seq_impl<N, I, integer_sequence<N, Indices...>, T,
			     Types...> {
	typedef typename make_integer_seq_impl<
		N, I + 1, integer_sequence<N, Indices..., I>, Types...>::type
		type;
};

/*
 * Make index sequence entry point.
 */
template <typename... Types>
using make_index_sequence =
	make_integer_seq_impl<size_t, 0, integer_sequence<size_t>, Types...>;

} /* namespace detail */

} /* namespace nvml */

#endif /* LIBPMEMOBJ_INTEGER_SEQUENCE_HPP */
