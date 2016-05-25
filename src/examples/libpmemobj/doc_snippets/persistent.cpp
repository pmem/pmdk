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
 * persistent.cpp -- C++ documentation snippets.
 */

//! [p_property_example]
#include <fcntl.h>
#include <libpmemobj/p.hpp>
#include <libpmemobj/pool.hpp>
#include <libpmemobj/transaction.hpp>

using namespace nvml::obj;

void
p_property_example()
{

	struct compound_type {

		void
		set_some_variable(int val)
		{
			some_variable = val;
		}

		int some_variable;
		double some_other_variable;
	};

	// pool root structure
	static struct root {
		p<int> counter;		 // this is OK
		p<compound_type> whoops; // this is hard to use
	} proot;

	// create a pmemobj pool
	auto pop = pool<root>::create("poolfile", "layout", PMEMOBJ_MIN_POOL,
				      S_IWUSR | S_IRUSR);

	// typical usage schemes
	transaction::exec_tx(pop, [&] {
		proot.counter = 12; // atomic
		// one way to change `whoops`
		proot.whoops.get_rw().set_some_variable(2);
		proot.whoops.get_rw().some_other_variable = 3.0;
	});

	// Changing a p<> variable outside of a transaction is a volatile
	// modification. No way to ensure persistence in case of power failure.
	proot.counter = 12;
}
//! [p_property_example]

//! [persistent_ptr_example]
#include <fcntl.h>
#include <libpmemobj/make_persistent.hpp>
#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/pool.hpp>
#include <libpmemobj/transaction.hpp>

using namespace nvml::obj;

void
persistent_ptr_example()
{

	struct compound_type {

		void
		set_some_variable(int val)
		{
			some_variable = val;
		}

		int some_variable;
		double some_other_variable;
	};

	// pool root structure
	struct root {
		persistent_ptr<compound_type> comp;
	} proot;

	// create a pmemobj pool
	auto pop = pool<root>::create("poolfile", "layout", PMEMOBJ_MIN_POOL,
				      S_IWUSR | S_IRUSR);

	// typical usage schemes
	transaction::exec_tx(pop, [&] {
		proot.comp = make_persistent<compound_type>(); // allocation
		proot.comp->set_some_variable(12);	     // call function
		proot.comp->some_other_variable = 2.3;	 // set variable
	});

	// reading from the persistent_ptr
	compound_type tmp = *proot.comp;
	(void)tmp;

	// Changing a persistent_ptr<> variable outside of a transaction is a
	// volatile modification. No way to ensure persistence in case of power
	// failure.
	proot.comp->some_variable = 12;
}
//! [persistent_ptr_example]
