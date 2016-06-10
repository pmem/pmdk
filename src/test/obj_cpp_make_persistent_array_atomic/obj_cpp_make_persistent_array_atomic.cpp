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

/*
 * obj_cpp_make_persistent_array_atomic.cpp -- cpp make_persistent test for
 * arrays
 */

#include "unittest.h"

#include <libpmemobj/make_persistent_array_atomic.hpp>
#include <libpmemobj/p.hpp>
#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/pool.hpp>

#define LAYOUT "cpp"

namespace nvobj = nvml::obj;

namespace
{

const int TEST_ARR_SIZE = 10;

class foo {
public:
	foo() : bar(1)
	{
		for (int i = 0; i < TEST_ARR_SIZE; ++i)
			this->arr[i] = 1;
	}

	/*
	 * Assert values of foo.
	 */
	void
	check_foo()
	{
		UT_ASSERTeq(1, this->bar);
		for (int i = 0; i < TEST_ARR_SIZE; ++i)
			UT_ASSERTeq(1, this->arr[i]);
	}

	~foo() = default;

	nvobj::p<int> bar;
	nvobj::p<char> arr[TEST_ARR_SIZE];
};

struct root {
	nvobj::persistent_ptr<foo[]> pfoo;
};

class bar {
public:
	bar()
	{
		/* throw any exception */
		throw 1;
	}
};

/*
 * test_make_one_d -- (internal) test make_persitent of a 1d array
 */
void
test_make_one_d(nvobj::pool_base &pop)
{
	nvobj::persistent_ptr<foo[]> pfoo;
	nvobj::make_persistent_atomic<foo[]>(pop, pfoo, 5);
	for (int i = 0; i < 5; ++i)
		pfoo[i].check_foo();

	nvobj::delete_persistent_atomic<foo[]>(pfoo, 5);

	nvobj::make_persistent_atomic<foo[]>(pop, pfoo, 6);
	for (int i = 0; i < 6; ++i)
		pfoo[i].check_foo();

	nvobj::delete_persistent_atomic<foo[]>(pfoo, 6);

	nvobj::persistent_ptr<foo[5]> pfooN;
	nvobj::make_persistent_atomic<foo[5]>(pop, pfooN);
	for (int i = 0; i < 5; ++i)
		pfooN[i].check_foo();

	nvobj::delete_persistent_atomic<foo[5]>(pfooN);
}

/*
 * test_make_two_d -- (internal) test make_persitent of a 2d array
 */
void
test_make_two_d(nvobj::pool_base &pop)
{
	nvobj::persistent_ptr<foo[][2]> pfoo;
	nvobj::make_persistent_atomic<foo[][2]>(pop, pfoo, 5);
	for (int i = 0; i < 5; ++i)
		for (int j = 0; j < 2; j++)
			pfoo[i][j].check_foo();

	nvobj::delete_persistent_atomic<foo[][2]>(pfoo, 5);

	nvobj::persistent_ptr<foo[][3]> pfoo2;
	nvobj::make_persistent_atomic<foo[][3]>(pop, pfoo2, 6);
	for (int i = 0; i < 6; ++i)
		for (int j = 0; j < 3; j++)
			pfoo2[i][j].check_foo();

	nvobj::delete_persistent_atomic<foo[][3]>(pfoo2, 6);

	nvobj::persistent_ptr<foo[5][2]> pfooN;
	nvobj::make_persistent_atomic<foo[5][2]>(pop, pfooN);
	for (int i = 0; i < 5; ++i)
		for (int j = 0; j < 2; j++)
			pfooN[i][j].check_foo();

	nvobj::delete_persistent_atomic<foo[5][2]>(pfooN);
}

/*
 * test_constructor_exception -- (internal) test exceptions thrown in
 * constructors
 */
void
test_constructor_exception(nvobj::pool_base &pop)
{
	nvobj::persistent_ptr<bar[]> pfoo;
	bool except = false;
	try {
		nvobj::make_persistent_atomic<bar[]>(pop, pfoo, 5);
	} catch (std::bad_alloc &ba) {
		except = true;
	}

	UT_ASSERT(except);
}

/*
 * test_delete_null -- (internal) test atomic delete nullptr
 */
void
test_delete_null(nvobj::pool<struct root> &pop)
{
	nvobj::persistent_ptr<foo[]> pfoo;
	nvobj::persistent_ptr<bar[3]> pbar;

	UT_ASSERT(pfoo == nullptr);
	UT_ASSERT(pbar == nullptr);

	try {
		nvobj::delete_persistent_atomic<foo[]>(pfoo, 2);
		nvobj::delete_persistent_atomic<bar[3]>(pbar);
	} catch (...) {
		UT_ASSERT(0);
	}
}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_cpp_make_persistent_array_atomic");

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	nvobj::pool<struct root> pop;

	try {
		pop = nvobj::pool<struct root>::create(
			path, LAYOUT, PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR);
	} catch (nvml::pool_error &pe) {
		UT_FATAL("!pool::create: %s %s", pe.what(), path);
	}

	test_make_one_d(pop);
	test_make_two_d(pop);
	test_constructor_exception(pop);
	test_delete_null(pop);

	pop.close();

	DONE(NULL);
}
