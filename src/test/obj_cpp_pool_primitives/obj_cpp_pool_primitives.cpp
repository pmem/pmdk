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
 * obj_cpp_pool_primitives.c -- cpp pool implementation test for:
 * - persist
 * - flush
 * - drain
 * - memcpy_persist
 * - memset_persist
 */

#include "unittest.h"

#include <libpmemobj/p.hpp>
#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/pool.hpp>

namespace nvobj = nvml::obj;

namespace
{

int TEST_VAL = 1;
size_t MB = ((size_t)1 << 20);

struct root {
	nvobj::p<int> val;
	nvobj::persistent_ptr<root> me;
};

/*
 * pool_test_memset -- (internal) test memset_persist primitive
 */
void
pool_test_memset(nvobj::pool<root> &pop)
{
	nvobj::persistent_ptr<root> root = pop.get_root();
	UT_ASSERT(root != nullptr);

	void *ret = pop.memset_persist(&root->val, TEST_VAL, sizeof(root->val));
	UT_ASSERTeq(ret, &root->val);

	int c;
	memset(&c, TEST_VAL, sizeof(c));
	UT_ASSERTeq(root->val, c);
}

/*
 * pool_test_memcpy -- (internal) test memcpy_persist primitive
 */
void
pool_test_memcpy(nvobj::pool<root> &pop)
{
	nvobj::persistent_ptr<root> root = pop.get_root();
	UT_ASSERT(root != nullptr);

	int v = TEST_VAL;
	void *ret = pop.memcpy_persist(&root->val, &v, sizeof(root->val));
	UT_ASSERTeq(ret, &root->val);
	UT_ASSERTeq(root->val, v);
}

/*
 * pool_test_drain -- (internal) test drain primitive
 */
void
pool_test_drain(nvobj::pool<root> &pop)
{
	pop.drain();
}

/*
 * pool_test_flush -- (internal) test flush primitive
 */
void
pool_test_flush(nvobj::pool<root> &pop)
{
	nvobj::persistent_ptr<root> root = pop.get_root();
	UT_ASSERT(root != nullptr);

	root->val = TEST_VAL;

	pop.flush(&root->val, sizeof(root->val));
}

/*
 * pool_test_flush_p -- (internal) test flush primitive on pmem property
 */
void
pool_test_flush_p(nvobj::pool<root> &pop)
{
	nvobj::persistent_ptr<root> root = pop.get_root();
	UT_ASSERT(root != nullptr);

	root->val = TEST_VAL;

	pop.flush(root->val);
}

/*
 * pool_test_flush_ptr -- (internal) test flush primitive on pmem pointer
 */
void
pool_test_flush_ptr(nvobj::pool<root> &pop)
{
	nvobj::persistent_ptr<root> root = pop.get_root();
	UT_ASSERT(root != nullptr);

	root->me = root;

	pop.flush(root->me);
}

/*
 * pool_test_flush_ptr_obj -- (internal) test flush primitive on pmem pointer
 * object
 */
void
pool_test_flush_ptr_obj(nvobj::pool<root> &pop)
{
	nvobj::persistent_ptr<root> root = pop.get_root();
	UT_ASSERT(root != nullptr);

	root->me = root;
	root->val = TEST_VAL;

	root.flush(pop);
}

/*
 * pool_test_flush_ptr_obj_no_pop -- (internal) test flush primitive on
 * pmem pointer object, without using pop
 */
void
pool_test_flush_ptr_obj_no_pop(nvobj::pool<root> &pop)
{
	nvobj::persistent_ptr<root> root = pop.get_root();
	UT_ASSERT(root != nullptr);

	root->me = root;
	root->val = TEST_VAL;

	root.flush();
}

/*
 * pool_test_persist -- (internal) test persist primitive
 */
void
pool_test_persist(nvobj::pool<root> &pop)
{
	nvobj::persistent_ptr<root> root = pop.get_root();
	UT_ASSERT(root != nullptr);

	root->val = TEST_VAL;

	pop.persist(&root->val, sizeof(root->val));
}

/*
 * pool_test_persist_p -- (internal) test persist primitive on pmem property
 */
void
pool_test_persist_p(nvobj::pool<root> &pop)
{
	nvobj::persistent_ptr<root> root = pop.get_root();
	UT_ASSERT(root != nullptr);

	root->val = TEST_VAL;

	pop.persist(root->val);
}

/*
 * pool_test_persist_ptr -- (internal) test persist primitive on pmem pointer
 */
void
pool_test_persist_ptr(nvobj::pool<root> &pop)
{
	nvobj::persistent_ptr<root> root = pop.get_root();
	UT_ASSERT(root != nullptr);

	root->me = root;

	pop.persist(root->me);
}

/*
 * pool_test_persist_ptr_obj -- (internal) test persist primitive on pmem
 * pointer object
 */
void
pool_test_persist_ptr_obj(nvobj::pool<root> &pop)
{
	nvobj::persistent_ptr<root> root = pop.get_root();
	UT_ASSERT(root != nullptr);

	root->me = root;
	root->val = TEST_VAL;

	root.persist(pop);
}

/*
 * pool_test_persist_ptr_obj_no_pop -- (internal) test persist primitive on
 * pmem pointer object, without using pop
 */
void
pool_test_persist_ptr_obj_no_pop(nvobj::pool<root> &pop)
{
	nvobj::persistent_ptr<root> root = pop.get_root();
	UT_ASSERT(root != nullptr);

	root->me = root;
	root->val = TEST_VAL;

	root.persist();
}

/*
 * pool_create -- (internal) test pool create
 */
nvobj::pool<root>
pool_create(const char *path, const char *layout, size_t poolsize,
	    unsigned mode)
{
	nvobj::pool<root> pop =
		nvobj::pool<root>::create(path, layout, poolsize, mode);
	nvobj::persistent_ptr<root> root = pop.get_root();
	UT_ASSERT(root != nullptr);

	return pop;
}

} /* namespace */

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_cpp_pool_primitives");

	if (argc != 2)
		UT_FATAL("usage: %s path", argv[0]);

	nvobj::pool<root> pop = pool_create(argv[1], "layout", 32 * MB, 0666);

	pool_test_persist(pop);
	pool_test_persist_p(pop);
	pool_test_persist_ptr(pop);
	pool_test_persist_ptr_obj(pop);
	pool_test_persist_ptr_obj_no_pop(pop);
	pool_test_flush(pop);
	pool_test_flush_p(pop);
	pool_test_flush_ptr(pop);
	pool_test_flush_ptr_obj(pop);
	pool_test_flush_ptr_obj_no_pop(pop);
	pool_test_drain(pop);
	pool_test_memcpy(pop);
	pool_test_memset(pop);

	DONE(NULL);
}
