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

/*
 * obj_cpp_ptr.c -- cpp bindings test
 *
 */

#include "unittest.h"

#include <libpmemobj/make_persistent.hpp>
#include <libpmemobj/make_persistent_array_atomic.hpp>
#include <libpmemobj/make_persistent_atomic.hpp>
#include <libpmemobj/p.hpp>
#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/pool.hpp>
#include <libpmemobj/transaction.hpp>

#define LAYOUT "cpp"

namespace nvobj = nvml::obj;

namespace
{

/*
 * test_null_ptr -- verifies if the pointer correctly behaves like a NULL-value
 */
void
test_null_ptr(nvobj::persistent_ptr<int> &f)
{
	UT_ASSERT(OID_IS_NULL(f.raw()));
	UT_ASSERT((bool)f == false);
	UT_ASSERT(!f);
	UT_ASSERTeq(f.get(), NULL);
	UT_ASSERT(f == nullptr);
}

/*
 * get_temp -- returns a temporary persistent_ptr
 */
nvobj::persistent_ptr<int>
get_temp()
{
	nvobj::persistent_ptr<int> int_null = nullptr;

	return int_null;
}

/*
 * test_ptr_operators_null -- verifies various operations on NULL pointers
 */
void
test_ptr_operators_null()
{
	nvobj::persistent_ptr<int> int_default_null;
	test_null_ptr(int_default_null);

	nvobj::persistent_ptr<int> int_explicit_ptr_null = nullptr;
	test_null_ptr(int_explicit_ptr_null);

	nvobj::persistent_ptr<int> int_explicit_oid_null = OID_NULL;
	test_null_ptr(int_explicit_oid_null);

	nvobj::persistent_ptr<int> int_base = nullptr;
	nvobj::persistent_ptr<int> int_same = int_base;
	int_same = int_base;
	test_null_ptr(int_same);

	std::swap(int_base, int_same);

	auto temp_ptr = get_temp();
	test_null_ptr(temp_ptr);
}

const int TEST_INT = 10;
const int TEST_ARR_SIZE = 10;
const char TEST_CHAR = 'a';

struct foo {
	nvobj::p<int> bar;
	nvobj::p<char> arr[TEST_ARR_SIZE];
};

struct nested {
	nvobj::persistent_ptr<foo> inner;
};

struct root {
	nvobj::persistent_ptr<foo> pfoo;
	nvobj::persistent_ptr<nvobj::p<int>[TEST_ARR_SIZE]> parr;

	/* This variable is unused, but it's here to check if the persistent_ptr
	 * does not violate it's own restrictions.
	 */
	nvobj::persistent_ptr<nested> outer;
};

/*
 * test_ptr_atomic -- verifies the persistent ptr with the atomic C API
 */
void
test_ptr_atomic(nvobj::pool<root> &pop)
{
	nvobj::persistent_ptr<foo> pfoo;

	try {
		nvobj::make_persistent_atomic<foo>(pop, pfoo);
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERTne(pfoo.get(), NULL);

	(*pfoo).bar = TEST_INT;
	memset(&pfoo->arr, TEST_CHAR, sizeof(pfoo->arr));

	for (auto c : pfoo->arr) {
		UT_ASSERTeq(c, TEST_CHAR);
	}

	try {
		nvobj::delete_persistent_atomic<foo>(pfoo);
		pfoo = nullptr;
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERTeq(pfoo.get(), NULL);
}

/*
 * test_ptr_transactional -- verifies the persistent ptr with the tx C API
 */
void
test_ptr_transactional(nvobj::pool<root> &pop)
{
	auto r = pop.get_root();

	try {
		nvobj::transaction::exec_tx(pop, [&] {
			UT_ASSERT(r->pfoo == nullptr);

			r->pfoo = nvobj::make_persistent<foo>();
		});
	} catch (...) {
		UT_ASSERT(0);
	}

	auto pfoo = r->pfoo;

	try {
		nvobj::transaction::exec_tx(pop, [&] {
			pfoo->bar = TEST_INT;
			memset(&pfoo->arr, TEST_CHAR, sizeof(pfoo->arr));
		});
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERTeq(pfoo->bar, TEST_INT);
	for (auto c : pfoo->arr) {
		UT_ASSERTeq(c, TEST_CHAR);
	}

	bool exception_thrown = false;
	try {
		nvobj::transaction::exec_tx(pop, [&] {
			pfoo->bar = 0;
			nvobj::transaction::abort(-1);
		});
	} catch (nvml::manual_tx_abort &ma) {
		exception_thrown = true;
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(exception_thrown);
	UT_ASSERTeq(pfoo->bar, TEST_INT);

	try {
		nvobj::transaction::exec_tx(
			pop, [&] { nvobj::delete_persistent<foo>(r->pfoo); });
		r->pfoo = nullptr;
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(r->pfoo == nullptr);
	UT_ASSERT(pfoo != nullptr);
}

/*
 * test_ptr_array -- verifies the array specialization behavior
 */
void
test_ptr_array(nvobj::pool<root> &pop)
{
	nvobj::persistent_ptr<nvobj::p<int>[]> parr_vsize;

	try {
		nvobj::make_persistent_atomic<nvobj::p<int>[]>(pop, parr_vsize,
							       TEST_ARR_SIZE);
	} catch (...) {
		UT_ASSERT(0);
	}

	for (int i = 0; i < TEST_ARR_SIZE; ++i)
		parr_vsize[i] = i;

	for (int i = 0; i < TEST_ARR_SIZE; ++i)
		UT_ASSERTeq(parr_vsize[i], i);

	auto r = pop.get_root();

	try {
		nvobj::transaction::exec_tx(pop, [&] {
			r->parr = pmemobj_tx_zalloc(sizeof(int) * TEST_ARR_SIZE,
						    0);
		});
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(r->parr != nullptr);

	bool exception_thrown = false;
	try {
		nvobj::transaction::exec_tx(pop, [&] {
			for (int i = 0; i < TEST_ARR_SIZE; ++i)
				r->parr[i] = TEST_INT;

			nvobj::transaction::abort(-1);
		});
	} catch (nvml::manual_tx_abort &ma) {
		exception_thrown = true;
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(exception_thrown);

	exception_thrown = false;
	try {
		nvobj::transaction::exec_tx(pop, [&] {
			for (int i = 0; i < TEST_ARR_SIZE; ++i)
				r->parr[i] = TEST_INT;

			nvobj::transaction::abort(-1);
		});
	} catch (nvml::manual_tx_abort &ma) {
		exception_thrown = true;
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(exception_thrown);

	for (int i = 0; i < TEST_ARR_SIZE; ++i)
		UT_ASSERTeq(r->parr[i], 0);
}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_cpp_ptr");

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	nvobj::pool<root> pop;

	try {
		pop = nvobj::pool<struct root>::create(
			path, LAYOUT, PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR);
	} catch (nvml::pool_error &pe) {
		UT_FATAL("!pool::create: %s %s", pe.what(), path);
	}

	test_ptr_operators_null();
	test_ptr_atomic(pop);
	test_ptr_transactional(pop);
	test_ptr_array(pop);

	pop.close();

	DONE(NULL);
}
