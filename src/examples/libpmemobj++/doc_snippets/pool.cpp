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
 * pool.cpp -- C++ documentation snippets.
 */

//! [pool_example]
#include <fcntl.h>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>

using namespace pmem::obj;

void
pool_example()
{

	// pool root structure
	struct root {
		p<int> some_array[42];
		p<int> some_other_array[42];
		p<double> some_variable;
	};

	// create a pmemobj pool
	auto pop = pool<root>::create("poolfile", "layout", PMEMOBJ_MIN_POOL);

	// close a pmemobj pool
	pop.close();

	// or open a pmemobj pool
	pop = pool<root>::open("poolfile", "layout");

	// typical usage schemes
	auto root_obj = pop.get_root();

	// low-level memory manipulation
	root_obj->some_variable = 3.2;
	pop.persist(root_obj->some_variable);

	pop.memset_persist(root_obj->some_array, 2,
			   sizeof(root_obj->some_array));

	pop.memcpy_persist(root_obj->some_other_array, root_obj->some_array,
			   sizeof(root_obj->some_array));

	pop.close();

	// check pool consistency
	pool<root>::check("poolfile", "layout");
}
//! [pool_example]

//! [pool_base_example]
#include <fcntl.h>
#include <libpmemobj++/make_persistent_atomic.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/pool.hpp>

using namespace pmem::obj;

void
pool_base_example()
{

	struct some_struct {
		p<int> some_array[42];
		p<int> some_other_array[42];
		p<int> some_variable;
	};

	// create a pmemobj pool
	auto pop = pool_base::create("poolfile", "", PMEMOBJ_MIN_POOL);

	// close a pmemobj pool
	pop.close();

	// or open a pmemobj pool
	pop = pool_base::open("poolfile", "");

	// no "root" object available in pool_base
	persistent_ptr<some_struct> pval;
	make_persistent_atomic<some_struct>(pop, pval);

	// low-level memory manipulation
	pval->some_variable = 3;
	pop.persist(pval->some_variable);

	pop.memset_persist(pval->some_array, 2, sizeof(pval->some_array));

	pop.memcpy_persist(pval->some_other_array, pval->some_array,
			   sizeof(pval->some_array));

	pop.close();

	// check pool consistency
	pool_base::check("poolfile", "");
}
//! [pool_base_example]
