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

#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/p.hpp>

#include <sstream>

#define LAYOUT "cpp"

using namespace nvml::obj;

namespace {

const int TEST_ARR_SIZE = 10;

/*
 * prepare_array -- preallocate and fill a persistent array
 */
template<typename T>
persistent_ptr<T>
prepare_array(PMEMobjpool *pop)
{
	int ret;

	persistent_ptr<T> parr_vsize;
	ret = pmemobj_alloc(pop, parr_vsize.raw_ptr(),
		sizeof (T) * TEST_ARR_SIZE,
		0, NULL, NULL);
	ASSERTeq(ret, 0);

	T *parray = parr_vsize.get();

	TX_BEGIN(pop) {
		for (int i = 0; i < TEST_ARR_SIZE; ++i) {
			parray[i] = i;
		}
	} TX_ONABORT {
		FATAL("Transactional prepare_array aborted");
	} TX_END;

	for (int i = 0; i < TEST_ARR_SIZE; ++i) {
		ASSERTeq(parray[i], i);
	}

	return parr_vsize;
}

/*
 * test_arith -- test arithmetic operations on persistent pointers
 */
void
test_arith(PMEMobjpool *pop)
{
	persistent_ptr<p<int>> parr_vsize = prepare_array<p<int>>(pop);

	/* test prefix postfix operators */
	for (int i = 0; i < TEST_ARR_SIZE; ++i) {
		ASSERTeq(*parr_vsize, i);
		parr_vsize++;
	}

	for (int i = TEST_ARR_SIZE; i > 0; --i) {
		parr_vsize--;
		ASSERTeq(*parr_vsize, i - 1);
	}

	for (int i = 0; i < TEST_ARR_SIZE; ++i) {
		ASSERTeq(*parr_vsize, i);
		++parr_vsize;
	}

	for (int i = TEST_ARR_SIZE; i > 0; --i) {
		--parr_vsize;
		ASSERTeq(*parr_vsize, i - 1);
	}

	/* test addition assignment and subtraction */
	parr_vsize += 2;
	ASSERTeq(*parr_vsize, 2);

	parr_vsize -= 2;
	ASSERTeq(*parr_vsize, 0);

	/* test strange invocations, parameter ignored */
	parr_vsize.operator++(5);
	ASSERTeq(*parr_vsize, 1);

	parr_vsize.operator--(2);
	ASSERTeq(*parr_vsize, 0);

	/* test subtraction and addition */
	for (int i = 0; i < TEST_ARR_SIZE; ++i)
		ASSERTeq(*(parr_vsize + i), i);

	/* using STL one-pas-end style */
	persistent_ptr<p<int>> parr_end = parr_vsize + TEST_ARR_SIZE;

	for (int i = TEST_ARR_SIZE; i > 0; --i)
		ASSERTeq(*(parr_end - i), TEST_ARR_SIZE - i);

	ASSERTeq(parr_end - parr_vsize, TEST_ARR_SIZE);

	/* check ostream operator */
	std::stringstream stream;
	stream << parr_vsize;
	OUT("%s", stream.str().c_str());
}

/*
 * test_relational -- test relational operators on persistent pointers
 */
void
test_relational(PMEMobjpool *pop)
{

	persistent_ptr<p<int>> first_elem = prepare_array<p<int>>(pop);
	persistent_ptr<p<int>> last_elem = first_elem + TEST_ARR_SIZE - 1;

	ASSERT(first_elem != last_elem);
	ASSERT(first_elem <= last_elem);
	ASSERT(first_elem < last_elem);
	ASSERT(last_elem > first_elem );
	ASSERT(last_elem >= first_elem );
	ASSERT(first_elem == first_elem);
	ASSERT(first_elem >= first_elem);
	ASSERT(first_elem <= first_elem);

	/* nullptr comparisons */
	ASSERT(first_elem != nullptr);
	ASSERT(nullptr != first_elem);
	ASSERT(!(first_elem == nullptr));
	ASSERT(!(nullptr == first_elem));

	ASSERT(nullptr < first_elem);
	ASSERT(!(first_elem < nullptr));
	ASSERT(nullptr <= first_elem);
	ASSERT(!(first_elem <= nullptr));

	ASSERT(first_elem > nullptr);
	ASSERT(!(nullptr > first_elem));
	ASSERT(first_elem >= nullptr);
	ASSERT(!(nullptr >= first_elem));

	persistent_ptr<p<double>> different_array =
			prepare_array<p<double>>(pop);

	/* only verify if this compiles */
	ASSERT((first_elem < different_array) || true);
}

}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_cpp_ptr_arith");

	if (argc != 2)
		FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop = NULL;

	if ((pop = pmemobj_create(path, LAYOUT, PMEMOBJ_MIN_POOL,
			S_IWUSR | S_IRUSR)) == NULL)
		FATAL("!pmemobj_create: %s", path);

	test_arith(pop);
	test_relational(pop);

	pmemobj_close(pop);

	DONE(NULL);
}
