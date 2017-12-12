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
 * obj_cpp_make_persistent.cpp -- cpp make_persistent test for objects
 */

#include "unittest.h"

#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>

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

	foo(int val) : bar(val)
	{
		for (int i = 0; i < TEST_ARR_SIZE; ++i)
			this->arr[i] = val;
	}

	foo(int val, char arr_val) : bar(val)
	{
		for (int i = 0; i < TEST_ARR_SIZE; ++i)
			this->arr[i] = arr_val;
	}

	/*
	 * Assert values of foo.
	 */
	void
	check_foo(int val, char arr_val)
	{
		UT_ASSERTeq(val, this->bar);
		for (int i = 0; i < TEST_ARR_SIZE; ++i)
			UT_ASSERTeq(arr_val, this->arr[i]);
	}

	nvobj::p<int> bar;
	nvobj::p<char> arr[TEST_ARR_SIZE];
};

struct root {
	nvobj::persistent_ptr<foo> pfoo;
};

/*
 * test_make_no_args -- (internal) test make_persitent without arguments
 */
void
test_make_no_args(nvobj::pool<struct root> &pop)
{
	nvobj::persistent_ptr<root> r = pop.get_root();

	try {
		nvobj::transaction::exec_tx(pop, [&] {
			UT_ASSERT(r->pfoo == nullptr);

			r->pfoo = nvobj::make_persistent<foo>();
			r->pfoo->check_foo(1, 1);

			nvobj::delete_persistent<foo>(r->pfoo);
			r->pfoo = nullptr;
		});
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(r->pfoo == nullptr);
}

/*
 * test_make_args -- (internal) test make_persitent with arguments
 */
void
test_make_args(nvobj::pool<struct root> &pop)
{
	nvobj::persistent_ptr<root> r = pop.get_root();

	try {
		nvobj::transaction::exec_tx(pop, [&] {
			UT_ASSERT(r->pfoo == nullptr);

			r->pfoo = nvobj::make_persistent<foo>(2);
			r->pfoo->check_foo(2, 2);

			nvobj::delete_persistent<foo>(r->pfoo);

			r->pfoo = nvobj::make_persistent<foo>(3, 4);
			r->pfoo->check_foo(3, 4);

			nvobj::delete_persistent<foo>(r->pfoo);
			r->pfoo = nullptr;
		});
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(r->pfoo == nullptr);
}

/*
 * test_additional_delete -- (internal) test double delete and delete rollback
 */
void
test_additional_delete(nvobj::pool<struct root> &pop)
{
	nvobj::persistent_ptr<root> r = pop.get_root();

	try {
		nvobj::transaction::exec_tx(pop, [&] {

			UT_ASSERT(r->pfoo == nullptr);

			r->pfoo = nvobj::make_persistent<foo>();
			r->pfoo->check_foo(1, 1);
		});
	} catch (...) {
		UT_ASSERT(0);
	}

	bool exception_thrown = false;
	try {
		nvobj::transaction::exec_tx(pop, [&] {
			UT_ASSERT(r->pfoo != nullptr);
			nvobj::delete_persistent<foo>(r->pfoo);
			r->pfoo = nullptr;
			nvobj::delete_persistent<foo>(r->pfoo);

			nvobj::transaction::abort(EINVAL);
		});
	} catch (nvml::manual_tx_abort &) {
		exception_thrown = true;
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(exception_thrown);
	UT_ASSERT(r->pfoo != nullptr);
	r->pfoo->check_foo(1, 1);

	try {
		nvobj::transaction::exec_tx(pop, [&] {
			UT_ASSERT(r->pfoo != nullptr);
			nvobj::delete_persistent<foo>(r->pfoo);
			r->pfoo = nullptr;
		});
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(r->pfoo == nullptr);
}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_cpp_make_persistent");

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	nvobj::pool<struct root> pop;

	try {
		pop = nvobj::pool<root>::create(path, LAYOUT, PMEMOBJ_MIN_POOL,
						S_IWUSR | S_IRUSR);
	} catch (nvml::pool_error &pe) {
		UT_FATAL("!pool::create: %s %s", pe.what(), path);
	}

	test_make_no_args(pop);
	test_make_args(pop);
	test_additional_delete(pop);

	pop.close();

	DONE(nullptr);
}
