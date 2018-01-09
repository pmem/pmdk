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
 * make_persistent.cpp -- C++ documentation snippets.
 */

//! [make_example]
#include <fcntl.h>
#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>

using namespace pmem::obj;

void
make_persistent_example()
{

	struct compound_type {

		compound_type(int val, double dval)
		    : some_variable(val), some_other_variable(dval)
		{
		}

		void
		set_some_variable(int val)
		{
			some_variable = val;
		}

		p<int> some_variable;
		p<double> some_other_variable;
	};

	// pool root structure
	struct root {
		persistent_ptr<compound_type> comp; //
	};

	// create a pmemobj pool
	auto pop = pool<root>::create("poolfile", "layout", PMEMOBJ_MIN_POOL);
	auto proot = pop.get_root();

	// typical usage schemes
	transaction::exec_tx(pop, [&] {
		// allocation with constructor argument passing
		proot->comp = make_persistent<compound_type>(1, 2.0);

		// transactionally delete the object, ~compound_type() is called
		delete_persistent<compound_type>(proot->comp);
	});

	// throws an transaction_scope_error exception
	auto arr1 = make_persistent<compound_type>(2, 15.0);
	delete_persistent<compound_type>(arr1);
}
//! [make_example]

//! [make_array_example]
#include <fcntl.h>
#include <libpmemobj++/make_persistent_array.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>

using namespace pmem::obj;

void
make_persistent_array_example()
{

	struct compound_type {

		compound_type() : some_variable(0), some_other_variable(0)
		{
		}

		void
		set_some_variable(int val)
		{
			some_variable = val;
		}

		p<int> some_variable;
		p<double> some_other_variable;
	};

	// pool root structure
	struct root {
		persistent_ptr<compound_type[]> comp; //
	};

	// create a pmemobj pool
	auto pop = pool<root>::create("poolfile", "layout", PMEMOBJ_MIN_POOL);
	auto proot = pop.get_root();

	// typical usage schemes
	transaction::exec_tx(pop, [&] {
		// allocate an array of 20 objects - compound_type must be
		// default constructible
		proot->comp = make_persistent<compound_type[]>(20);
		// another allocation method
		auto arr1 = make_persistent<compound_type[3]>();

		// transactionally delete arrays , ~compound_type() is called
		delete_persistent<compound_type[]>(proot->comp, 20);
		delete_persistent<compound_type[3]>(arr1);
	});

	// throws an transaction_scope_error exception
	auto arr1 = make_persistent<compound_type[3]>();
	delete_persistent<compound_type[3]>(arr1);
}
//! [make_array_example]

//! [make_atomic_example]
#include <fcntl.h>
#include <libpmemobj++/make_persistent_atomic.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>

using namespace pmem::obj;

void
make_persistent_atomic_example()
{

	struct compound_type {

		compound_type(int val, double dval)
		    : some_variable(val), some_other_variable(dval)
		{
		}

		void
		set_some_variable(int val)
		{
			some_variable = val;
		}

		p<int> some_variable;
		p<double> some_other_variable;
	};

	// pool root structure
	struct root {
		persistent_ptr<compound_type> comp; //
	};

	// create a pmemobj pool
	auto pop = pool<root>::create("poolfile", "layout", PMEMOBJ_MIN_POOL);
	auto proot = pop.get_root();

	// typical usage schemes

	// atomic allocation and construction with arguments passing
	make_persistent_atomic<compound_type>(pop, proot->comp, 1, 2.0);

	// atomic object deallocation, ~compound_type() is not called
	delete_persistent<compound_type>(proot->comp);

	// error prone cases
	transaction::exec_tx(pop, [&] {
		// possible invalid state in case of transaction abort
		make_persistent_atomic<compound_type>(pop, proot->comp, 1, 1.3);
		delete_persistent_atomic<compound_type>(proot->comp);
	});
}
//! [make_atomic_example]

//! [make_array_atomic_example]
#include <fcntl.h>
#include <libpmemobj++/make_persistent_array_atomic.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>

using namespace pmem::obj;

void
make_persistent_array_atomic_example()
{

	struct compound_type {

		compound_type() : some_variable(0), some_other_variable(0)
		{
		}

		void
		set_some_variable(int val)
		{
			some_variable = val;
		}

		p<int> some_variable;
		p<double> some_other_variable;
	};

	// pool root structure
	struct root {
		persistent_ptr<compound_type[]> comp; //
	};

	// create a pmemobj pool
	auto pop = pool<root>::create("poolfile", "layout", PMEMOBJ_MIN_POOL);
	auto proot = pop.get_root();

	// typical usage schemes

	// atomic array allocation and construction - the compound_type has to
	// be default constructible
	make_persistent_atomic<compound_type[]>(pop, proot->comp, 20);

	persistent_ptr<compound_type[42]> arr;
	make_persistent_atomic<compound_type[42]>(pop, arr);

	// atomic array deallocation, no destructor being called
	delete_persistent_atomic<compound_type[]>(proot->comp, 20);
	delete_persistent_atomic<compound_type[42]>(arr);

	// error prone cases
	transaction::exec_tx(pop, [&] {
		// possible invalid state in case of transaction abort
		make_persistent_atomic<compound_type[]>(pop, proot->comp, 30);
		delete_persistent_atomic<compound_type[]>(proot->comp, 30);
	});
}
//! [make_array_atomic_example]
