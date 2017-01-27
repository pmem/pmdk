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

/*
 * obj_cpp_allocator.c -- cpp bindings test
 *
 */

#include "unittest.h"

#include <libpmemobj++/allocator.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>

#define LAYOUT "cpp"

namespace nvobj = nvml::obj;

namespace
{

const int TEST_ARR_SIZE = 10;

struct foo {

	/*
	 * Default constructor.
	 */
	foo()
	{
		bar = 1;
		for (int i = 0; i < TEST_ARR_SIZE; ++i)
			arr[i] = i;
	}

	/*
	 * Copy constructible.
	 */
	foo(const foo &rhs) = default;

	/*
	 * Check foo values.
	 */
	void
	test_foo()
	{
		UT_ASSERTeq(bar, 1);
		for (int i = 0; i < TEST_ARR_SIZE; ++i)
			UT_ASSERTeq(arr[i], i);
	}

	nvobj::p<int> bar;
	nvobj::p<char> arr[TEST_ARR_SIZE];
};

/*
 * test_alloc_valid -- (internal) test an allocation within a transaction
 */
void
test_alloc_valid(nvobj::pool_base &pop)
{
	nvobj::allocator<foo> al;

	try {
		nvobj::transaction::exec_tx(pop, [&] {
			auto fooptr = al.allocate(1);
			UT_ASSERT(pmemobj_alloc_usable_size(fooptr.raw()) >=
				  sizeof(foo));
			al.construct(fooptr, foo());
			fooptr->test_foo();
			al.destroy(fooptr);
			al.deallocate(fooptr);
		});
	} catch (...) {
		UT_ASSERT(0);
	}
}

/*
 * test_alloc_invalid -- (internal) test an allocation outside of a transaction
 */
void
test_alloc_invalid()
{
	nvobj::allocator<foo> al;
	bool thrown = false;
	try {
		auto fooptr = al.allocate(1);
		al.construct(fooptr, foo());
	} catch (nvml::transaction_scope_error &) {
		thrown = true;
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(thrown);
}

/*
 * test_alloc_equal -- (internal) test allocator equality/inequality operators
 */
void
test_alloc_equal()
{
	nvobj::allocator<foo> fooal;
	nvobj::allocator<int> intal;
	std::allocator<foo> stdfooal;
	std::allocator<int> stdintal;
	std::allocator<double> stddblal;

	UT_ASSERT(fooal == fooal);
	UT_ASSERT(intal == fooal);
	UT_ASSERT(!(fooal != fooal));
	UT_ASSERT(!(intal != fooal));
	UT_ASSERT(fooal != stdfooal);
	UT_ASSERT(fooal != stdintal);
	UT_ASSERT(fooal != stddblal);
	UT_ASSERT(intal != stdfooal);
	UT_ASSERT(intal != stdintal);
	UT_ASSERT(intal != stddblal);
	UT_ASSERT(!(fooal == stdfooal));
	UT_ASSERT(!(fooal == stdintal));
	UT_ASSERT(!(fooal == stddblal));
	UT_ASSERT(!(intal == stdfooal));
	UT_ASSERT(!(intal == stdintal));
	UT_ASSERT(!(intal == stddblal));
}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_cpp_allocator");

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	nvobj::pool_base pop;

	try {
		pop = nvobj::pool_base::create(path, LAYOUT, PMEMOBJ_MIN_POOL,
					       S_IWUSR | S_IRUSR);
	} catch (nvml::pool_error &pe) {
		UT_FATAL("!pool::create: %s %s", pe.what(), path);
	}

	test_alloc_valid(pop);
	test_alloc_invalid();
	test_alloc_equal();

	pop.close();

	DONE(nullptr);
}
