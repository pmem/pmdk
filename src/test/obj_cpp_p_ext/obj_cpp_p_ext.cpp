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
 * obj_cpp_p_ext.c -- cpp p<> property operators test
 *
 */

#include "unittest.h"

#include <libpmemobj/p.hpp>
#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/pext.hpp>
#include <libpmemobj/pool.hpp>
#include <libpmemobj/transaction.hpp>

#include <cmath>
#include <sstream>

#define LAYOUT "cpp"

namespace nvobj = nvml::obj;

namespace
{

struct foo {
	nvobj::p<int> pint;
	nvobj::p<long long> pllong;
	nvobj::p<unsigned char> puchar;
};

struct bar {
	nvobj::p<double> pdouble;
	nvobj::p<float> pfloat;
};

struct root {
	nvobj::persistent_ptr<bar> bar_ptr;
	nvobj::persistent_ptr<foo> foo_ptr;
};

/*
 * init_foobar -- (internal) initialize the root object with specific values
 */
nvobj::persistent_ptr<root>
init_foobar(nvobj::pool_base &pop)
{
	nvobj::pool<struct root> &root_pop =
		dynamic_cast<nvobj::pool<struct root> &>(pop);
	nvobj::persistent_ptr<root> r = root_pop.get_root();

	try {
		nvobj::transaction::exec_tx(pop, [&] {
			UT_ASSERT(r->bar_ptr == nullptr);
			UT_ASSERT(r->foo_ptr == nullptr);

			r->bar_ptr = pmemobj_tx_alloc(sizeof(bar), 0);
			r->foo_ptr = pmemobj_tx_alloc(sizeof(foo), 0);

			r->bar_ptr->pdouble = 1.0;
			r->bar_ptr->pfloat = 2.0;

			r->foo_ptr->puchar = 0;
			r->foo_ptr->pint = 1;
			r->foo_ptr->pllong = 2;
		});
	} catch (...) {
		UT_ASSERT(0);
	}

	return r;
}

/*
 * cleanup_foobar -- (internal) deallocate and zero out root fields
 */
void
cleanup_foobar(nvobj::pool_base &pop)
{
	nvobj::pool<struct root> &root_pop =
		dynamic_cast<nvobj::pool<struct root> &>(pop);
	nvobj::persistent_ptr<root> r = root_pop.get_root();

	try {
		nvobj::transaction::exec_tx(pop, [&] {
			UT_ASSERT(r->bar_ptr != nullptr);
			UT_ASSERT(r->foo_ptr != nullptr);

			pmemobj_tx_free(r->bar_ptr.raw());
			r->bar_ptr = nullptr;
			pmemobj_tx_free(r->foo_ptr.raw());
			r->foo_ptr = nullptr;
		});
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(r->bar_ptr == nullptr);
	UT_ASSERT(r->foo_ptr == nullptr);
}

/*
 * arithmetic_test -- (internal) perform basic arithmetic tests on p<>
 */
void
arithmetic_test(nvobj::pool_base &pop)
{
	nvobj::persistent_ptr<root> r = init_foobar(pop);

	/* operations test */
	try {
		nvobj::transaction::exec_tx(pop, [&] {
			/* addition */
			r->foo_ptr->puchar += r->foo_ptr->puchar;
			r->foo_ptr->puchar += r->foo_ptr->pint;
			r->foo_ptr->puchar += 2;
			UT_ASSERTeq(r->foo_ptr->puchar, 3);

			r->foo_ptr->pint =
				r->foo_ptr->pint + r->foo_ptr->puchar;
			r->foo_ptr->pint = r->foo_ptr->pint + r->foo_ptr->pint;
			r->foo_ptr->pint = r->foo_ptr->pllong + 8;
			UT_ASSERTeq(r->foo_ptr->pint, 10);

			/* for float assertions */
			float epsilon = 0.001;

			/* subtraction */
			r->bar_ptr->pdouble -= r->foo_ptr->puchar;
			r->bar_ptr->pfloat -= 2;
			UT_ASSERT(std::fabs(r->bar_ptr->pdouble + 2) < epsilon);
			UT_ASSERT(std::fabs(r->bar_ptr->pfloat) < epsilon);

			r->bar_ptr->pfloat =
				r->bar_ptr->pfloat - r->bar_ptr->pdouble;
			r->bar_ptr->pdouble =
				r->bar_ptr->pdouble - r->bar_ptr->pfloat;
			UT_ASSERT(std::fabs(r->bar_ptr->pfloat - 2) < epsilon);
			UT_ASSERT(std::fabs(r->bar_ptr->pdouble + 4) < epsilon);

			/* multiplication */
			r->foo_ptr->puchar *= r->foo_ptr->puchar;
			r->foo_ptr->puchar *= r->foo_ptr->pint;
			r->foo_ptr->puchar *= r->foo_ptr->pllong;
			UT_ASSERTeq(r->foo_ptr->puchar, 180);

			r->foo_ptr->pint =
				r->foo_ptr->pint * r->foo_ptr->puchar;
			r->foo_ptr->pint = r->foo_ptr->pint * r->foo_ptr->pint;
			r->foo_ptr->pint =
				r->foo_ptr->pllong * r->foo_ptr->pint;
			/* no assertions needed at this point */

			/* division */
			r->bar_ptr->pdouble /= r->foo_ptr->puchar;
			r->bar_ptr->pfloat /= r->foo_ptr->pllong;
			/* no assertions needed at this point */

			r->bar_ptr->pfloat =
				r->bar_ptr->pfloat / r->bar_ptr->pdouble;
			r->bar_ptr->pdouble =
				r->bar_ptr->pdouble / r->bar_ptr->pfloat;
			/* no assertions needed at this point */

			/* prefix */
			++r->foo_ptr->pllong;
			--r->foo_ptr->pllong;
			UT_ASSERTeq(r->foo_ptr->pllong, 2);

			/* postfix */
			r->foo_ptr->pllong++;
			r->foo_ptr->pllong--;
			UT_ASSERTeq(r->foo_ptr->pllong, 2);

			/* modulo */
			r->foo_ptr->pllong = 12;
			r->foo_ptr->pllong %= 7;
			UT_ASSERTeq(r->foo_ptr->pllong, 5);
			r->foo_ptr->pllong = r->foo_ptr->pllong % 3;
			UT_ASSERTeq(r->foo_ptr->pllong, 2);
			r->foo_ptr->pllong =
				r->foo_ptr->pllong % r->foo_ptr->pllong;
			UT_ASSERTeq(r->foo_ptr->pllong, 0);
		});
	} catch (...) {
		UT_ASSERT(0);
	}

	cleanup_foobar(pop);
}

/*
 * bitwise_test -- (internal) perform basic bitwise operator tests on p<>
 */
void
bitwise_test(nvobj::pool_base &pop)
{
	nvobj::persistent_ptr<root> r = init_foobar(pop);

	try {
		nvobj::transaction::exec_tx(pop, [&] {
			/* OR */
			r->foo_ptr->puchar |= r->foo_ptr->pllong;
			r->foo_ptr->puchar |= r->foo_ptr->pint;
			r->foo_ptr->puchar |= 4;
			UT_ASSERTeq(r->foo_ptr->puchar, 7);

			r->foo_ptr->pint =
				r->foo_ptr->pint | r->foo_ptr->puchar;
			r->foo_ptr->pint = r->foo_ptr->pint | r->foo_ptr->pint;
			r->foo_ptr->pint = r->foo_ptr->pllong | 0xF;
			UT_ASSERTeq(r->foo_ptr->pint, 15);

			/* AND */
			r->foo_ptr->puchar &= r->foo_ptr->puchar;
			r->foo_ptr->puchar &= r->foo_ptr->pint;
			r->foo_ptr->puchar &= 2;
			UT_ASSERTeq(r->foo_ptr->puchar, 2);

			r->foo_ptr->pint =
				r->foo_ptr->pint & r->foo_ptr->puchar;
			r->foo_ptr->pint = r->foo_ptr->pint & r->foo_ptr->pint;
			r->foo_ptr->pint = r->foo_ptr->pllong & 8;
			UT_ASSERTeq(r->foo_ptr->pint, 0);

			/* XOR */
			r->foo_ptr->puchar ^= r->foo_ptr->puchar;
			r->foo_ptr->puchar ^= r->foo_ptr->pint;
			r->foo_ptr->puchar ^= 2;
			UT_ASSERTeq(r->foo_ptr->puchar, 2);

			r->foo_ptr->pint =
				r->foo_ptr->pint ^ r->foo_ptr->puchar;
			r->foo_ptr->pint = r->foo_ptr->pint ^ r->foo_ptr->pint;
			r->foo_ptr->pint = r->foo_ptr->pllong ^ 8;
			UT_ASSERTeq(r->foo_ptr->pint, 10);

			/* RSHIFT */
			r->foo_ptr->puchar = 255;
			r->foo_ptr->puchar >>= 1;
			r->foo_ptr->puchar >>= r->foo_ptr->pllong;
			r->foo_ptr->puchar = r->foo_ptr->pllong >> 2;
			r->foo_ptr->puchar =
				r->foo_ptr->pllong >> r->foo_ptr->pllong;
			UT_ASSERTeq(r->foo_ptr->puchar, 0);

			/* LSHIFT */
			r->foo_ptr->puchar = 1;
			r->foo_ptr->puchar <<= 1;
			r->foo_ptr->puchar <<= r->foo_ptr->pllong;
			r->foo_ptr->puchar = r->foo_ptr->pllong << 2;
			r->foo_ptr->puchar = r->foo_ptr->pllong
				<< r->foo_ptr->pllong;
			UT_ASSERTeq(r->foo_ptr->puchar, 8);

			/* COMPLEMENT */
			r->foo_ptr->pint = 1;
			UT_ASSERTeq(~r->foo_ptr->pint, ~1);
		});
	} catch (...) {
		UT_ASSERT(0);
	}

	cleanup_foobar(pop);
}

/*
 * stream_test -- (internal) perform basic istream/ostream operator tests on p<>
 */
void
stream_test(nvobj::pool_base &pop)
{
	nvobj::persistent_ptr<root> r = init_foobar(pop);

	try {
		nvobj::transaction::exec_tx(pop, [&] {
			std::stringstream stream("12.4");
			stream >> r->bar_ptr->pdouble;
			/*
			 * clear the stream's EOF,
			 * we're ok with the buffer realloc
			 */
			stream.clear();
			stream.str("");
			r->bar_ptr->pdouble += 3.7;
			stream << r->bar_ptr->pdouble;
			stream >> r->foo_ptr->pint;
			UT_ASSERTeq(r->foo_ptr->pint, 16);
		});
	} catch (...) {
		UT_ASSERT(0);
	}

	cleanup_foobar(pop);
}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_cpp_p_ext");

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

	arithmetic_test(pop);
	bitwise_test(pop);
	stream_test(pop);

	pop.close();

	DONE(NULL);
}
