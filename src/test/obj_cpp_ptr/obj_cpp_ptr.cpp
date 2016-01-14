/*
 * Copyright (c) 2015-2016, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/p.hpp>

#define LAYOUT "cpp"

using namespace nvml::obj;

namespace {

/*
 * test_null_ptr -- verifies if the pointer correctly behaves like a NULL-value
 */
void
test_null_ptr(persistent_ptr<int> &f)
{
	ASSERT(OID_IS_NULL(f.raw()));
	ASSERT((bool)f == false);
	ASSERT(!f);
	ASSERTeq(f.get(), NULL);
	ASSERT(f == nullptr);
}

/*
 * get_temp -- returns a temporary persistent_ptr
 */
persistent_ptr<int>
get_temp()
{
	persistent_ptr<int> int_null = nullptr;

	return int_null;
}

/*
 * test_ptr_operators_null -- verifies various operations on NULL pointers
 */
void
test_ptr_operators_null()
{
	persistent_ptr<int> int_explicit_ptr_null = nullptr;
	test_null_ptr(int_explicit_ptr_null);

	persistent_ptr<int> int_explicit_oid_null = OID_NULL;
	test_null_ptr(int_explicit_oid_null);

	persistent_ptr<float> float_base = nullptr;
	persistent_ptr<int> int_converted = float_base;
	int_converted = float_base;
	test_null_ptr(int_converted);

	persistent_ptr<int> int_base = nullptr;
	persistent_ptr<int> int_same = int_base;
	int_same = int_base;
	test_null_ptr(int_same);

	std::swap(int_base, int_same);

	auto temp_ptr = get_temp();
	test_null_ptr(temp_ptr);
}

#define TEST_INT 10
#define TEST_ARR_SIZE 10
#define TEST_CHAR 'a'

struct foo {
	p<int> bar;
	p<char> arr[TEST_ARR_SIZE];
};

struct nested {
	persistent_ptr<foo> inner;
};

struct root {
	persistent_ptr<foo> pfoo;
	persistent_ptr<p<int>[TEST_ARR_SIZE]> parr;

	/* This variable is unused, but it's here to check if the persistent_ptr
	 * does not violate it's own restrictions.
	 */
	persistent_ptr<nested> outer;
};

/*
 * test_ptr_atomic -- verifies the persistent ptr with the atomic C API
 */
void
test_ptr_atomic(PMEMobjpool *pop)
{
	int ret;

	persistent_ptr<foo> pfoo;

	ret = pmemobj_alloc(pop, pfoo.raw_ptr(), sizeof (foo),
		0, NULL, NULL);

	ASSERTeq(ret, 0);
	ASSERTne(pfoo.get(), NULL);

	(*pfoo).bar = TEST_INT;
	memset(&pfoo->arr, TEST_CHAR, sizeof (pfoo->arr));

	for (auto c : pfoo->arr) {
		ASSERTeq(c, TEST_CHAR);
	}

	pmemobj_free(pfoo.raw_ptr());

	ASSERTeq(ret, 0);
	ASSERTeq(pfoo.get(), NULL);
}

/*
 * test_ptr_transactional -- verifies the persistent ptr with the tx C API
 */
void
test_ptr_transactional(PMEMobjpool *pop)
{
	persistent_ptr<root> r = pmemobj_root(pop, sizeof (root));

	TX_BEGIN(pop) {
		ASSERT(r->pfoo == nullptr);

		r->pfoo = pmemobj_tx_alloc(sizeof (foo), 0);

	} TX_ONABORT {
		ASSERT(0);
	} TX_END

	persistent_ptr<foo> pfoo = r->pfoo;

	TX_BEGIN(pop) {
		pfoo->bar = TEST_INT;
		memset(&pfoo->arr, TEST_CHAR, sizeof (pfoo->arr));
	} TX_ONABORT {
		ASSERT(0);
	} TX_END

	ASSERTeq(pfoo->bar, TEST_INT);
	for (auto c : pfoo->arr) {
		ASSERTeq(c, TEST_CHAR);
	}

	TX_BEGIN(pop) {
		pfoo->bar = 0;
		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		ASSERT(0);
	} TX_END

	ASSERTeq(pfoo->bar, TEST_INT);

	TX_BEGIN(pop) {
		pmemobj_tx_free(pfoo.raw());
		r->pfoo = nullptr;
	} TX_ONABORT {
		ASSERT(0);
	} TX_END

	ASSERT(r->pfoo == nullptr);
}

/*
 * test_ptr_array -- verifies the array specialization behavior
 */
void
test_ptr_array(PMEMobjpool *pop)
{
	int ret;

	persistent_ptr<p<int>[]> parr_vsize;
	ret = pmemobj_alloc(pop, parr_vsize.raw_ptr(),
		sizeof (int) * TEST_ARR_SIZE,
		0, NULL, NULL);
	ASSERTeq(ret, 0);

	for (int i = 0; i < TEST_ARR_SIZE; ++i)
		parr_vsize[i] = i;

	for (int i = 0; i < TEST_ARR_SIZE; ++i)
		ASSERTeq(parr_vsize[i], i);

	persistent_ptr<root> r = pmemobj_root(pop, sizeof (root));

	TX_BEGIN(pop) {
		r->parr = pmemobj_tx_zalloc(sizeof (int) * TEST_ARR_SIZE, 0);
	} TX_ONABORT {
		ASSERT(0);
	} TX_END

	ASSERT(r->parr != nullptr);

	TX_BEGIN(pop) {
		for (int i = 0; i < TEST_ARR_SIZE; ++i)
			r->parr[i] = TEST_INT;

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		ASSERT(0);
	} TX_END

	for (int i = 0; i < TEST_ARR_SIZE; ++i)
		ASSERTeq(r->parr[i], 0);
}

}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_cpp_ptr");

	if (argc != 2)
		FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop = NULL;

	if ((pop = pmemobj_create(path, LAYOUT, PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR)) == NULL)
		FATAL("!pmemobj_create: %s", path);

	test_ptr_operators_null();
	test_ptr_atomic(pop);
	test_ptr_transactional(pop);
	test_ptr_array(pop);

	pmemobj_close(pop);

	DONE(NULL);
}
