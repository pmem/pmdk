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
 * obj_cpp_mutex_posix.cpp -- cpp mutex test
 */

#include "unittest.h"

#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/pool.hpp>
#include <libpmemobj/timed_mutex.hpp>

#include <mutex>

#define LAYOUT "cpp"

namespace nvobj = nvml::obj;

namespace
{

/* pool root structure */
struct root {
	nvobj::timed_mutex pmutex;
	int counter;
};

/* number of ops per thread */
const int num_ops = 200;

/* the number of threads */
const int num_threads = 30;

/* timeout for try_lock_for and try_lock_until methods */
const auto timeout = std::chrono::milliseconds(100);

/*
 * increment_pint -- (internal) test the mutex with an std::lock_guard
 */
static void *
increment_pint(void *arg)
{
	auto proot = static_cast<nvobj::persistent_ptr<struct root> *>(arg);

	for (int i = 0; i < num_ops; ++i) {
		std::lock_guard<nvobj::timed_mutex> lock((*proot)->pmutex);
		((*proot)->counter)++;
	}
	return nullptr;
}

/*
 * decrement_pint -- (internal) test the mutex with an std::unique_lock
 */
static void *
decrement_pint(void *arg)
{
	auto proot = static_cast<nvobj::persistent_ptr<struct root> *>(arg);

	std::unique_lock<nvobj::timed_mutex> lock((*proot)->pmutex);
	for (int i = 0; i < num_ops; ++i)
		--((*proot)->counter);

	lock.unlock();
	return nullptr;
}

/*
 * trylock_test -- (internal) test the trylock implementation
 */
static void *
trylock_test(void *arg)
{
	auto proot = static_cast<nvobj::persistent_ptr<struct root> *>(arg);
	for (;;) {
		if ((*proot)->pmutex.try_lock()) {
			((*proot)->counter)++;
			(*proot)->pmutex.unlock();
			break;
		}
	}
	return nullptr;
}

/*
 * trylock_for_test -- (internal) test the try_lock_for implementation
 */
static void *
trylock_for_test(void *arg)
{
	using clk = std::chrono::steady_clock;
	auto proot = static_cast<nvobj::persistent_ptr<struct root> *>(arg);

	auto t1 = clk::now();
	if ((*proot)->pmutex.try_lock_for(timeout)) {
		((*proot)->counter)++;
		(*proot)->pmutex.unlock();
	} else {
		auto t2 = clk::now();
		auto t_diff = t2 - t1;
		UT_ASSERT(t_diff >= timeout);
	}

	return nullptr;
}

/*
 * trylock_until_test -- (internal) test the try_lock_until implementation
 */
static void *
trylock_until_test(void *arg)
{
	using clk = std::chrono::steady_clock;
	auto proot = static_cast<nvobj::persistent_ptr<struct root> *>(arg);

	auto t1 = clk::now();
	if ((*proot)->pmutex.try_lock_until(t1 + timeout)) {
		--((*proot)->counter);
		(*proot)->pmutex.unlock();
	} else {
		auto t2 = clk::now();
		auto t_diff = t2 - t1;
		UT_ASSERT(t_diff >= timeout);
	}

	return nullptr;
}

/*
 * mutex_test -- (internal) launch worker threads to test the pmutex
 */
template <typename Worker>
void
timed_mtx_test(nvobj::pool<struct root> &pop, Worker function)
{
	pthread_t threads[num_threads];

	auto proot = pop.get_root();

	for (int i = 0; i < num_threads; ++i)
		PTHREAD_CREATE(&threads[i], nullptr, function, &proot);

	for (int i = 0; i < num_threads; ++i)
		PTHREAD_JOIN(threads[i], nullptr);
}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_cpp_mutex_posix");

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

	timed_mtx_test(pop, increment_pint);
	UT_ASSERTeq(pop.get_root()->counter, num_threads * num_ops);

	timed_mtx_test(pop, decrement_pint);
	UT_ASSERTeq(pop.get_root()->counter, 0);

	timed_mtx_test(pop, trylock_test);
	UT_ASSERTeq(pop.get_root()->counter, num_threads);

	timed_mtx_test(pop, trylock_until_test);
	UT_ASSERTeq(pop.get_root()->counter, 0);

	timed_mtx_test(pop, trylock_for_test);
	UT_ASSERTeq(pop.get_root()->counter, num_threads);

	pop.get_root()->pmutex.lock();

	timed_mtx_test(pop, trylock_until_test);
	UT_ASSERTeq(pop.get_root()->counter, num_threads);

	timed_mtx_test(pop, trylock_for_test);
	UT_ASSERTeq(pop.get_root()->counter, num_threads);

	pop.get_root()->pmutex.unlock();

	/* pmemcheck related persist */
	pmemobj_persist(pop.get_handle(), &(pop.get_root()->counter),
			sizeof(pop.get_root()->counter));

	pop.close();

	DONE(NULL);
}
