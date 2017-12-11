/*
 * Copyright 2017, Intel Corporation
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
 * obj_cpp_pool_win.c -- cpp pool implementation test
 */

#include "unittest.h"

#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>

namespace nvobj = pmem::obj;

namespace
{

size_t MB = ((size_t)1 << 20);

struct root {
	nvobj::p<int> val;
};

/*
 * pool_create -- (internal) test pool create
 */
void
pool_create(const wchar_t *path, const wchar_t *layout, size_t poolsize,
	    unsigned mode)
{
	std::unique_ptr<char> _path(ut_toUTF8(path));

	nvobj::pool<root> pop;

	try {
		pop = nvobj::pool<root>::create(path, layout, poolsize, mode);
		nvobj::persistent_ptr<root> root = pop.get_root();
		UT_ASSERT(root != nullptr);
	} catch (pmem::pool_error &) {
		UT_OUT("!%s: pool::create", _path.get());
		return;
	}

	os_stat_t stbuf;
	STATW(path, &stbuf);

	UT_OUT("%s: file size %zu mode 0%o", _path.get(), stbuf.st_size,
	       stbuf.st_mode & 0777);
	try {
		pop.close();
	} catch (std::logic_error &lr) {
		UT_OUT("%s: pool.close: %s", _path.get(), lr.what());
		return;
	}

	int result = nvobj::pool<root>::check(path, layout);

	if (result < 0)
		UT_OUT("!%s: pool::check", _path.get());
	else if (result == 0)
		UT_OUT("%s: pool::check: not consistent", _path.get());
}

/*
 * pool_open -- (internal) test pool open
 */
void
pool_open(const wchar_t *path, const wchar_t *layout)
{
	std::unique_ptr<char> _path(ut_toUTF8(path));
	nvobj::pool<root> pop;

	try {
		pop = nvobj::pool<root>::open(path, layout);
	} catch (pmem::pool_error &) {
		UT_OUT("!%s: pool::open", _path.get());
		return;
	}

	UT_OUT("%s: pool::open: Success", _path.get());

	try {
		pop.close();
	} catch (std::logic_error &lr) {
		UT_OUT("%s: pool.close: %s", _path.get(), lr.what());
	}
}

/*
 * double_close -- (internal) test double pool close
 */
void
double_close(const wchar_t *path, const wchar_t *layout, size_t poolsize,
	     unsigned mode)
{
	std::unique_ptr<char> _path(ut_toUTF8(path));
	nvobj::pool<root> pop;

	try {
		pop = nvobj::pool<root>::create(path, layout, poolsize, mode);
	} catch (pmem::pool_error &) {
		UT_OUT("!%s: pool::create", _path.get());
		return;
	}

	UT_OUT("%s: pool::create: Success", _path.get());

	try {
		pop.close();
		UT_OUT("%s: pool.close: Success", _path.get());
		pop.close();
	} catch (std::logic_error &lr) {
		UT_OUT("%s: pool.close: %s", _path.get(), lr.what());
	}
}

/*
 * get_root_closed -- (internal) test get_root on a closed pool
 */
void
get_root_closed()
{
	nvobj::pool<root> pop;

	try {
		pop.get_root();
	} catch (pmem::pool_error &pe) {
		UT_OUT("pool.get_root: %s", pe.what());
	}
}

} /* namespace */

int
wmain(int argc, wchar_t *argv[])
{
	STARTW(argc, argv, "obj_cpp_pool_win");

	if (argc < 4)
		UT_FATAL("usage: %s op path layout [poolsize mode]", ut_toUTF8(argv[0]));

	const wchar_t *layout = nullptr;
	size_t poolsize;
	unsigned mode;

	if (wcscmp(argv[3], L"EMPTY") == 0)
		layout = L"";
	else if (wcscmp(argv[3], L"NULL") != 0)
		layout = argv[3];

	switch (argv[1][0]) {
		case 'c':
			poolsize = wcstoul(argv[4], nullptr, 0) *
				MB; /* in megabytes */
			mode = wcstoul(argv[5], nullptr, 8);

			pool_create(argv[2], layout, poolsize, mode);
			break;
		case 'o':
			pool_open(argv[2], layout);
			break;
		case 'd':
			poolsize = wcstoul(argv[4], nullptr, 0) *
				MB; /* in megabytes */
			mode = wcstoul(argv[5], nullptr, 8);

			double_close(argv[2], layout, poolsize, mode);
			break;
		case 'i':
			get_root_closed();
			break;
		default:
			UT_FATAL("unknown operation");
	}

	DONEW(nullptr);
}
