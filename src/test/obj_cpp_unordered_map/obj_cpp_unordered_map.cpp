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
 * obj_cpp_unordered_map.c -- std persistent vector test
 *
 */

#include "unittest.h"

#include <libpmemobj++/allocator.hpp>
#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>

#include "obj_cpp_containers/cont_test_common.hpp"

#include <unordered_map>

#define LAYOUT "cpp"

namespace
{

const int Last_val_key = 23;

struct containers {

	explicit containers(nvobj::pool_base &pop)
	{
		try {
			nvobj::transaction::exec_tx(pop, [&] {
				foomap.insert(std::make_pair(1, foo()));
				foomap[12] = foo();
				foomap[2] = foo();
				foomap[14] = foo();
				foomap.erase(2);
				foomap[Last_val_key] = foo(Last_val);
			});
		} catch (...) {
			UT_ASSERT(0);
		}
	}

	std::unordered_map<int, foo, std::hash<int>, std::equal_to<int>,
			   nvobj::allocator<std::pair<const int, foo>>>
		foomap;
};

struct root {
	nvobj::persistent_ptr<containers> cons;
};

/*
 * test_map -- (internal) test map<foo> with the nvml allocator
 */
void
test_map(nvobj::pool<root> &pop, bool open)
{
	auto conp = pop.get_root()->cons;

	UT_ASSERT(conp != nullptr);

	auto lastval = conp->foomap.find(Last_val_key);
	UT_ASSERT(lastval != conp->foomap.end());
	lastval->second.test_foo(Last_val);

	auto iter = conp->foomap.cbegin();
	while (iter != conp->foomap.cend()) {
		if (iter == lastval)
			++iter;
		else
			(iter++)->second.test_foo();
	}

	if (open) {
		loop_insert(pop, conp->foomap, std::make_pair(rand(), foo()),
			    20);
		nvobj::transaction::manual tx(pop);

		conp->foomap.begin()->second = foo(234);

		auto it = conp->foomap.begin();
		std::advance(it, conp->foomap.size() / 2);
		conp->foomap.erase(it);

		nvobj::transaction::commit();
	}
}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_cpp_unordered_map");

	if (argc != 3 || strchr("co", argv[1][0]) == nullptr)
		UT_FATAL("usage: %s <c,o> file-name", argv[0]);

	const char *path = argv[2];

	nvobj::pool<root> pop;
	bool open = (argv[1][0] == 'o');

	try {
		if (open) {
			pop = nvobj::pool<root>::open(path, LAYOUT);

		} else {
			pop = nvobj::pool<root>::create(path, LAYOUT,
							PMEMOBJ_MIN_POOL * 2,
							S_IWUSR | S_IRUSR);
			nvobj::transaction::manual tx(pop);
			pop.get_root()->cons =
				nvobj::make_persistent<containers>(pop);
			nvobj::transaction::commit();
		}
	} catch (nvml::pool_error &pe) {
		UT_FATAL("!pool::create: %s %s", pe.what(), path);
	}

	test_map(pop, open);

	pop.close();

	DONE(nullptr);
}
