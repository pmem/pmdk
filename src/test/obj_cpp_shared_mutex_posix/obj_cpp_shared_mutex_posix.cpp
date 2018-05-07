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
 * obj_cpp_shared_mutex_posix.cpp -- cpp shared mutex test
 *
 */

#include "unittest.h"

#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/shared_mutex.hpp>

#include <mutex>

#define LAYOUT "cpp"

namespace nvobj = pmem::obj;

namespace
{

/* pool root structure */
struct root {
	nvobj::shared_mutex pmutex;
	int counter;
};

/* number of ops per thread */
const int num_ops = 200;

/* the number of threads */
const int num_threads = 30;

/*
 * writer -- (internal) bump up the counter by 2
 */
void *
writer(void *arg)
{
	auto proot = static_cast<nvobj::persistent_ptr<root> *>(arg);

	for (int i = 0; i < num_ops; ++i) {
		std::lock_guard<nvobj::shared_mutex> lock((*proot)->pmutex);
		++((*proot)->counter);
		++((*proot)->counter);
	}
	return nullptr;
}

/*
 * reader -- (internal) verify if the counter is even
 */
void *
reader(void *arg)
{
	auto proot = static_cast<nvobj::persistent_ptr<root> *>(arg);
	for (int i = 0; i < num_ops; ++i) {
		(*proot)->pmutex.lock_shared();
		UT_ASSERTeq((*proot)->counter % 2, 0);
		(*proot)->pmutex.unlock_shared();
	}
	return nullptr;
}

/*
 * writer_trylock -- (internal) trylock bump the counter by 2
 */
void *
writer_trylock(void *arg)
{
	auto proot = static_cast<nvobj::persistent_ptr<root> *>(arg);
	for (;;) {
		if ((*proot)->pmutex.try_lock()) {
			--((*proot)->counter);
			--((*proot)->counter);
			(*proot)->pmutex.unlock();
			break;
		}
	}
	return nullptr;
}

/*
 * reader_trylock -- (internal) trylock verify that the counter is even
 */
void *
reader_trylock(void *arg)
{
	auto proot = static_cast<nvobj::persistent_ptr<root> *>(arg);
	for (;;) {
		if ((*proot)->pmutex.try_lock_shared()) {
			UT_ASSERTeq((*proot)->counter % 2, 0);
			(*proot)->pmutex.unlock_shared();
			break;
		}
	}
	return nullptr;
}

/*
 * mutex_zero_test -- (internal) test the zeroing constructor
 */
void
mutex_zero_test(nvobj::pool<struct root> &pop)
{
	PMEMoid raw_mutex;

	pmemobj_alloc(pop.get_handle(), &raw_mutex, sizeof(PMEMrwlock), 1,
		      [](PMEMobjpool *pop, void *ptr, void *arg) -> int {
			      PMEMrwlock *mtx = static_cast<PMEMrwlock *>(ptr);
			      pmemobj_memset_persist(pop, mtx, 1, sizeof(*mtx));
			      return 0;
		      },
		      nullptr);

	nvobj::shared_mutex *placed_mtx =
		new (pmemobj_direct(raw_mutex)) nvobj::shared_mutex;
	std::unique_lock<nvobj::shared_mutex> lck(*placed_mtx);
}

/*
 * mutex_test -- (internal) launch worker threads to test the pshared_mutex
 */
template <typename Worker>
void
mutex_test(nvobj::pool<root> &pop, Worker writer, Worker reader)
{
	const int total_threads = num_threads * 2;
	os_thread_t threads[total_threads];

	auto proot = pop.get_root();

	for (int i = 0; i < total_threads; i += 2) {
		PTHREAD_CREATE(&threads[i], nullptr, writer, &proot);
		PTHREAD_CREATE(&threads[i + 1], nullptr, reader, &proot);
	}

	for (int i = 0; i < total_threads; ++i)
		PTHREAD_JOIN(&threads[i], nullptr);
}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_cpp_shared_mutex_posix");

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	nvobj::pool<root> pop;

	try {
		pop = nvobj::pool<root>::create(path, LAYOUT, PMEMOBJ_MIN_POOL,
						S_IWUSR | S_IRUSR);
	} catch (pmem::pool_error &pe) {
		UT_FATAL("!pool::create: %s %s", pe.what(), path);
	}

	mutex_zero_test(pop);

	int expected = num_threads * num_ops * 2;
	mutex_test(pop, writer, reader);
	UT_ASSERTeq(pop.get_root()->counter, expected);

	/* trylocks are not tested as exhaustively */
	expected -= num_threads * 2;
	mutex_test(pop, writer_trylock, reader_trylock);
	UT_ASSERTeq(pop.get_root()->counter, expected);

	/* pmemcheck related persist */
	pmemobj_persist(pop.get_handle(), &(pop.get_root()->counter),
			sizeof(pop.get_root()->counter));

	pop.close();

	DONE(nullptr);
}
