/*
 * Copyright 2016-2017, Intel Corporation
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

#ifndef CONT_TEST_COMMON_HPP
#define CONT_TEST_COMMON_HPP

#include <unittest.h>
#include "libpmemobj++/p.hpp"

namespace nvobj = pmem::obj;

namespace {

const int Test_arr_size = 10;
const int Last_val = 0xFF;

struct foo {

	/*
	 * Default constructor.
	 */
	explicit foo(int tobar = 1) : bar(tobar)
	{
		for (int i = 0; i < Test_arr_size; ++i)
			arr[i] = i;
	}

	/*
	 * Copy constructible.
	 */
	foo(const foo &rhs) = default;
	foo &operator=(const foo &rhs) = default;

	bool
	operator<(const foo &rhs) const noexcept
	{
		return this->bar < rhs.bar;
	}

	/*
	 * Check foo values.
	 */
	void
	test_foo(int tobar = 1) const
	{
		UT_ASSERTeq(bar, tobar);
		for (int i = 0; i < Test_arr_size; ++i)
			UT_ASSERTeq(arr[i], i);
	}

	nvobj::p<int> bar;
	nvobj::p<char> arr[Test_arr_size];
};

struct hash : public std::unary_function<nvobj::p<size_t>, const foo &> {
	nvobj::p<size_t>
	operator()(const foo &key) const
	{
		return (nvobj::p<size_t>)key.bar;
	}
};

struct equal_to : public std::binary_function<bool, const foo &, const foo &> {
	bool
	operator()(const foo &lhs, const foo &rhs) const
	{
		return lhs.bar == rhs.bar;
	}
};

/*
 * test_container_val -- (internal) test container values
 */
template <typename T>
void
test_container_val(T &cont)
{
	auto iter = cont.rbegin();
	(iter++)->test_foo(Last_val);
	while (iter != cont.rend()) {
		(iter++)->test_foo();
	}
}

/*
 * loop_insert -- (internal) insert values in separate transactions
 */
template <typename T, typename Y, typename pool>
void
loop_insert(pool &pop, T &cont, const Y &val, int count)
{
	for (int i = 0; i < count; ++i) {
		try {
			nvobj::transaction::exec_tx(pop, [&] {
				cont.insert(cont.cbegin(), Y(val));
			});
		} catch (...) {
			UT_ASSERT(0);
		}
	}
}

}

#endif /* CONT_TEST_COMMON_HPP */
