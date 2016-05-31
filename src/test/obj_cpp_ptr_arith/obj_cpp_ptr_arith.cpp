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
 * obj_cpp_ptr_arith.cpp -- cpp bindings test
 *
 */

#include "unittest.h"

#include <libpmemobj/make_persistent_atomic.hpp>
#include <libpmemobj/p.hpp>
#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/transaction.hpp>

#include <sstream>

#define LAYOUT "cpp"

namespace nvobj = nvml::obj;

namespace
{

const int TEST_ARR_SIZE = 10;

/*
 * prepare_array -- preallocate and fill a persistent array
 */
template <typename T>
nvobj::persistent_ptr<T>
prepare_array(nvobj::pool_base &pop)
{
	int ret;

	nvobj::persistent_ptr<T> parr_vsize;
	ret = pmemobj_alloc(pop.get_handle(), parr_vsize.raw_ptr(),
			    sizeof(T) * TEST_ARR_SIZE, 0, NULL, NULL);

	UT_ASSERTeq(ret, 0);

	T *parray = parr_vsize.get();

	try {
		nvobj::transaction::exec_tx(pop, [&] {
			for (int i = 0; i < TEST_ARR_SIZE; ++i) {
				parray[i] = i;
			}
		});
	} catch (...) {
		UT_FATAL("Transactional prepare_array aborted");
	}

	for (int i = 0; i < TEST_ARR_SIZE; ++i) {
		UT_ASSERTeq(parray[i], i);
	}

	return parr_vsize;
}

/*
 * test_arith -- test arithmetic operations on persistent pointers
 */
void
test_arith(nvobj::pool_base &pop)
{
	auto parr_vsize = prepare_array<nvobj::p<int>>(pop);

	/* test prefix postfix operators */
	for (int i = 0; i < TEST_ARR_SIZE; ++i) {
		UT_ASSERTeq(*parr_vsize, i);
		parr_vsize++;
	}

	for (int i = TEST_ARR_SIZE; i > 0; --i) {
		parr_vsize--;
		UT_ASSERTeq(*parr_vsize, i - 1);
	}

	for (int i = 0; i < TEST_ARR_SIZE; ++i) {
		UT_ASSERTeq(*parr_vsize, i);
		++parr_vsize;
	}

	for (int i = TEST_ARR_SIZE; i > 0; --i) {
		--parr_vsize;
		UT_ASSERTeq(*parr_vsize, i - 1);
	}

	/* test addition assignment and subtraction */
	parr_vsize += 2;
	UT_ASSERTeq(*parr_vsize, 2);

	parr_vsize -= 2;
	UT_ASSERTeq(*parr_vsize, 0);

	/* test strange invocations, parameter ignored */
	parr_vsize.operator++(5);
	UT_ASSERTeq(*parr_vsize, 1);

	parr_vsize.operator--(2);
	UT_ASSERTeq(*parr_vsize, 0);

	/* test subtraction and addition */
	for (int i = 0; i < TEST_ARR_SIZE; ++i)
		UT_ASSERTeq(*(parr_vsize + i), i);

	/* using STL one-pas-end style */
	auto parr_end = parr_vsize + TEST_ARR_SIZE;

	for (int i = TEST_ARR_SIZE; i > 0; --i)
		UT_ASSERTeq(*(parr_end - i), TEST_ARR_SIZE - i);

	UT_ASSERTeq(parr_end - parr_vsize, TEST_ARR_SIZE);

	/* check ostream operator */
	std::stringstream stream;
	stream << parr_vsize;
	UT_OUT("%s", stream.str().c_str());
}

/*
 * test_relational -- test relational operators on persistent pointers
 */
void
test_relational(nvobj::pool_base &pop)
{

	auto first_elem = prepare_array<nvobj::p<int>>(pop);
	nvobj::persistent_ptr<int[10][12]> parray;
	auto last_elem = first_elem + TEST_ARR_SIZE - 1;

	UT_ASSERT(first_elem != last_elem);
	UT_ASSERT(first_elem <= last_elem);
	UT_ASSERT(first_elem < last_elem);
	UT_ASSERT(last_elem > first_elem);
	UT_ASSERT(last_elem >= first_elem);
	UT_ASSERT(first_elem == first_elem);
	UT_ASSERT(first_elem >= first_elem);
	UT_ASSERT(first_elem <= first_elem);

	/* nullptr comparisons */
	UT_ASSERT(first_elem != nullptr);
	UT_ASSERT(nullptr != first_elem);
	UT_ASSERT(!(first_elem == nullptr));
	UT_ASSERT(!(nullptr == first_elem));

	UT_ASSERT(nullptr < first_elem);
	UT_ASSERT(!(first_elem < nullptr));
	UT_ASSERT(nullptr <= first_elem);
	UT_ASSERT(!(first_elem <= nullptr));

	UT_ASSERT(first_elem > nullptr);
	UT_ASSERT(!(nullptr > first_elem));
	UT_ASSERT(first_elem >= nullptr);
	UT_ASSERT(!(nullptr >= first_elem));

	/* pointer to array */
	UT_ASSERT(parray == nullptr);
	UT_ASSERT(nullptr == parray);
	UT_ASSERT(!(parray != nullptr));
	UT_ASSERT(!(nullptr != parray));

	UT_ASSERT(!(nullptr < parray));
	UT_ASSERT(!(parray < nullptr));
	UT_ASSERT(nullptr <= parray);
	UT_ASSERT(parray <= nullptr);

	UT_ASSERT(!(parray > nullptr));
	UT_ASSERT(!(nullptr > parray));
	UT_ASSERT(parray >= nullptr);
	UT_ASSERT(nullptr >= parray);

	auto different_array = prepare_array<nvobj::p<double>>(pop);

	/* only verify if this compiles */
	UT_ASSERT((first_elem < different_array) || true);
}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_cpp_ptr_arith");

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

	test_arith(pop);
	test_relational(pop);

	pop.close();

	DONE(NULL);
}
