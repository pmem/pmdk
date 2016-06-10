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
 * mutex.cpp -- C++ documentation snippets.
 */

//! [unique_guard_example]
#include <libpmemobj/mutex.hpp>
#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/pool.hpp>
#include <mutex>

namespace nvobj = nvml::obj;

void
unique_guard_example()
{
	// pool root structure
	struct root {
		nvobj::mutex pmutex;
	};

	// create a pmemobj pool
	auto pop = nvobj::pool<root>::create(
		"poolfile", "layout", PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR);
	auto proot = pop.get_root();

	// typical usage schemes
	std::lock_guard<nvobj::mutex> guard(proot->pmutex);

	std::unique_lock<nvobj::mutex> other_guard(proot->pmutex);
}
//! [unique_guard_example]

//! [shared_mutex_example]
#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/pool.hpp>
#include <libpmemobj/shared_mutex.hpp>
#include <mutex>

namespace nvobj = nvml::obj;

void
shared_mutex_example()
{
	// pool root structure
	struct root {
		nvobj::shared_mutex pmutex;
	};

	// create a pmemobj pool
	auto pop = nvobj::pool<root>::create(
		"poolfile", "layout", PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR);
	auto proot = pop.get_root();

	// typical usage schemes
	proot->pmutex.lock_shared();

	std::unique_lock<nvobj::shared_mutex> guard(proot->pmutex);
}
//! [shared_mutex_example]

//! [timed_mutex_example]
#include <chrono>
#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/pool.hpp>
#include <libpmemobj/timed_mutex.hpp>

namespace nvobj = nvml::obj;

void
timed_mutex_example()
{
	// pool root structure
	struct root {
		nvobj::timed_mutex pmutex;
	};

	// create a pmemobj pool
	auto pop = nvobj::pool<root>::create(
		"poolfile", "layout", PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR);
	auto proot = pop.get_root();

	const auto timeout = std::chrono::milliseconds(100);

	// typical usage schemes
	proot->pmutex.try_lock_for(timeout);

	proot->pmutex.try_lock_until(std::chrono::steady_clock::now() +
				     timeout);
}
//! [timed_mutex_example]

//! [cond_var_example]
#include <libpmemobj/condition_variable.hpp>
#include <libpmemobj/mutex.hpp>
#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/pool.hpp>
#include <mutex>
#include <thread>

namespace nvobj = nvml::obj;

void
cond_var_example()
{
	// pool root structure
	struct root {
		nvobj::mutex pmutex;
		nvobj::condition_variable cond;
		int counter;
	};

	// create a pmemobj pool
	auto pop = nvobj::pool<root>::create(
		"poolfile", "layout", PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR);
	auto proot = pop.get_root();

	// run worker to bump up the counter
	std::thread worker([&] {
		std::unique_lock<nvobj::mutex> lock(proot->pmutex);
		while (proot->counter < 1000)
			++proot->counter;
		// unlock before notifying to avoid blocking on waiting thread
		lock.unlock();
		// notify the waiting thread
		proot->cond.notify_one();
	});

	std::unique_lock<nvobj::mutex> lock(proot->pmutex);
	// wait on condition variable
	proot->cond.wait(lock, [&] { return proot->counter >= 1000; });

	worker.join();
}
//! [cond_var_example]
