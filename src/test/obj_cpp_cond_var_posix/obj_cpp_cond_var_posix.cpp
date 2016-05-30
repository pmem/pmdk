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
 * obj_cpp_cond_var_posix.c -- cpp condition variable test
 */

#include "unittest.h"

#include <libpmemobj/condition_variable.hpp>
#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/pool.hpp>

#include <mutex>
#include <vector>

#define LAYOUT "cpp"

namespace nvobj = nvml::obj;

namespace
{

/* convenience typedef */
typedef void *(*reader_type)(void *);

/* pool root structure */
struct root {
	nvobj::mutex pmutex;
	nvobj::condition_variable cond;
	int counter;
};

/* the number of threads */
const int num_threads = 30;

/* notification limit */
const int limit = 7000;

/* cond wait time in milliseconds */
const std::chrono::milliseconds wait_time(150);

/* arguments for write worker */
struct writer_args {
	nvobj::persistent_ptr<struct root> proot;
	bool notify;
	bool all;
};

/*
 * write_notify -- (internal) bump up the counter up to a limit and notify
 */
void *
write_notify(void *args)
{
	struct writer_args *wargs = static_cast<struct writer_args *>(args);

	std::lock_guard<nvobj::mutex> lock(wargs->proot->pmutex);

	while (wargs->proot->counter < limit)
		wargs->proot->counter++;

	if (wargs->notify) {
		if (wargs->all)
			wargs->proot->cond.notify_all();
		else
			wargs->proot->cond.notify_one();
	}

	return nullptr;
}

/*
 * reader_mutex -- (internal) verify the counter value
 */
void *
reader_mutex(void *arg)
{
	nvobj::persistent_ptr<struct root> *proot =
		static_cast<nvobj::persistent_ptr<struct root> *>(arg);
	(*proot)->pmutex.lock();
	while ((*proot)->counter != limit)
		(*proot)->cond.wait((*proot)->pmutex);

	UT_ASSERTeq((*proot)->counter, limit);
	(*proot)->pmutex.unlock();

	return nullptr;
}

/*
 * reader_mutex_pred -- (internal) verify the counter value
 */
void *
reader_mutex_pred(void *arg)
{
	nvobj::persistent_ptr<struct root> *proot =
		static_cast<nvobj::persistent_ptr<struct root> *>(arg);
	(*proot)->pmutex.lock();
	(*proot)->cond.wait((*proot)->pmutex,
			    [&]() { return (*proot)->counter == limit; });

	UT_ASSERTeq((*proot)->counter, limit);
	(*proot)->pmutex.unlock();

	return nullptr;
}

/*
 * reader_lock -- (internal) verify the counter value
 */
void *
reader_lock(void *arg)
{
	nvobj::persistent_ptr<struct root> *proot =
		static_cast<nvobj::persistent_ptr<struct root> *>(arg);
	std::unique_lock<nvobj::mutex> lock((*proot)->pmutex);
	while ((*proot)->counter != limit)
		(*proot)->cond.wait((*proot)->pmutex);

	UT_ASSERTeq((*proot)->counter, limit);
	lock.unlock();

	return nullptr;
}

/*
 * reader_lock_pred -- (internal) verify the counter value
 */
void *
reader_lock_pred(void *arg)
{
	nvobj::persistent_ptr<struct root> *proot =
		static_cast<nvobj::persistent_ptr<struct root> *>(arg);
	std::unique_lock<nvobj::mutex> lock((*proot)->pmutex);
	(*proot)->cond.wait(lock, [&]() { return (*proot)->counter == limit; });

	UT_ASSERTeq((*proot)->counter, limit);
	lock.unlock();

	return nullptr;
}

/*
 * reader_mutex_until -- (internal) verify the counter value or timeout
 */
void *
reader_mutex_until(void *arg)
{
	nvobj::persistent_ptr<struct root> *proot =
		static_cast<nvobj::persistent_ptr<struct root> *>(arg);
	(*proot)->pmutex.lock();
	auto until = std::chrono::system_clock::now();
	until += wait_time;
	auto ret = (*proot)->cond.wait_until((*proot)->pmutex, until);

	auto now = std::chrono::system_clock::now();
	if (ret == std::cv_status::timeout)
		UT_ASSERT(now >= until);
	else
		UT_ASSERTeq((*proot)->counter, limit);
	(*proot)->pmutex.unlock();

	return nullptr;
}

/*
 * reader_mutex_until_pred -- (internal) verify the counter value or timeout
 */
void *
reader_mutex_until_pred(void *arg)
{
	nvobj::persistent_ptr<struct root> *proot =
		static_cast<nvobj::persistent_ptr<struct root> *>(arg);
	(*proot)->pmutex.lock();
	auto until = std::chrono::system_clock::now();
	until += wait_time;
	auto ret = (*proot)->cond.wait_until((*proot)->pmutex, until, [&]() {
		return (*proot)->counter == limit;
	});

	auto now = std::chrono::system_clock::now();
	if (ret == false)
		UT_ASSERT(now >= until);
	else
		UT_ASSERTeq((*proot)->counter, limit);
	(*proot)->pmutex.unlock();

	return nullptr;
}

/*
 * reader_lock_until -- (internal) verify the counter value or timeout
 */
void *
reader_lock_until(void *arg)
{
	nvobj::persistent_ptr<struct root> *proot =
		static_cast<nvobj::persistent_ptr<struct root> *>(arg);
	std::unique_lock<nvobj::mutex> lock((*proot)->pmutex);
	auto until = std::chrono::system_clock::now();
	until += wait_time;
	auto ret = (*proot)->cond.wait_until((*proot)->pmutex, until);

	auto now = std::chrono::system_clock::now();
	if (ret == std::cv_status::timeout)
		UT_ASSERT(now >= until);
	else
		UT_ASSERTeq((*proot)->counter, limit);
	lock.unlock();

	return nullptr;
}

/*
 * reader_lock_until_pred -- (internal) verify the counter value or timeout
 */
void *
reader_lock_until_pred(void *arg)
{
	nvobj::persistent_ptr<struct root> *proot =
		static_cast<nvobj::persistent_ptr<struct root> *>(arg);
	std::unique_lock<nvobj::mutex> lock((*proot)->pmutex);
	auto until = std::chrono::system_clock::now();
	until += wait_time;
	auto ret = (*proot)->cond.wait_until((*proot)->pmutex, until, [&]() {
		return (*proot)->counter == limit;
	});

	auto now = std::chrono::system_clock::now();
	if (ret == false)
		UT_ASSERT(now >= until);
	else
		UT_ASSERTeq((*proot)->counter, limit);
	lock.unlock();

	return nullptr;
}

/*
 * reader_mutex_for -- (internal) verify the counter value or timeout
 */
void *
reader_mutex_for(void *arg)
{
	nvobj::persistent_ptr<struct root> *proot =
		static_cast<nvobj::persistent_ptr<struct root> *>(arg);
	(*proot)->pmutex.lock();
	auto until = std::chrono::system_clock::now();
	until += wait_time;
	auto ret = (*proot)->cond.wait_for((*proot)->pmutex, wait_time);

	auto now = std::chrono::system_clock::now();
	if (ret == std::cv_status::timeout)
		UT_ASSERT(now >= until);
	else
		UT_ASSERTeq((*proot)->counter, limit);
	(*proot)->pmutex.unlock();

	return nullptr;
}

/*
 * reader_mutex_for_pred -- (internal) verify the counter value or timeout
 */
void *
reader_mutex_for_pred(void *arg)
{
	nvobj::persistent_ptr<struct root> *proot =
		static_cast<nvobj::persistent_ptr<struct root> *>(arg);
	(*proot)->pmutex.lock();
	auto until = std::chrono::system_clock::now();
	until += wait_time;
	auto ret = (*proot)->cond.wait_for((*proot)->pmutex, wait_time, [&]() {
		return (*proot)->counter == limit;
	});

	auto now = std::chrono::system_clock::now();
	if (ret == false)
		UT_ASSERT(now >= until);
	else
		UT_ASSERTeq((*proot)->counter, limit);
	(*proot)->pmutex.unlock();

	return nullptr;
}

/*
 * reader_lock_for -- (internal) verify the counter value or timeout
 */
void *
reader_lock_for(void *arg)
{
	nvobj::persistent_ptr<struct root> *proot =
		static_cast<nvobj::persistent_ptr<struct root> *>(arg);
	std::unique_lock<nvobj::mutex> lock((*proot)->pmutex);
	auto until = std::chrono::system_clock::now();
	until += wait_time;
	auto ret = (*proot)->cond.wait_for((*proot)->pmutex, wait_time);

	auto now = std::chrono::system_clock::now();
	if (ret == std::cv_status::timeout)
		UT_ASSERT(now >= until);
	else
		UT_ASSERTeq((*proot)->counter, limit);
	lock.unlock();

	return nullptr;
}

/*
 * reader_lock_for_pred -- (internal) verify the counter value or timeout
 */
void *
reader_lock_for_pred(void *arg)
{
	nvobj::persistent_ptr<struct root> *proot =
		static_cast<nvobj::persistent_ptr<struct root> *>(arg);
	std::unique_lock<nvobj::mutex> lock((*proot)->pmutex);
	auto until = std::chrono::system_clock::now();
	until += wait_time;
	auto ret = (*proot)->cond.wait_for((*proot)->pmutex, wait_time, [&]() {
		return (*proot)->counter == limit;
	});

	auto now = std::chrono::system_clock::now();
	if (ret == false)
		UT_ASSERT(now >= until);
	else
		UT_ASSERTeq((*proot)->counter, limit);
	lock.unlock();

	return nullptr;
}

/*
 * mutex_test -- (internal) launch worker threads to test the pshared_mutex
 */
template <typename Reader, typename Writer>
void
mutex_test(nvobj::pool<struct root> &pop, bool notify, bool notify_all,
	   Reader writer, Writer reader)
{
	const int total_threads = num_threads * 2;
	pthread_t threads[total_threads];

	nvobj::persistent_ptr<struct root> proot = pop.get_root();
	struct writer_args wargs = {proot, notify, notify_all};

	for (int i = 0; i < total_threads; i += 2) {
		PTHREAD_CREATE(&threads[i], nullptr, reader, &proot);
		PTHREAD_CREATE(&threads[i + 1], nullptr, writer, &wargs);
	}

	for (int i = 0; i < total_threads; ++i)
		PTHREAD_JOIN(threads[i], nullptr);
}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_cpp_cond_var_posix");

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

	std::vector<reader_type> notify_functions(
		{reader_mutex, reader_mutex_pred, reader_lock, reader_lock_pred,
		 reader_mutex_until, reader_mutex_until_pred, reader_lock_until,
		 reader_lock_until_pred, reader_mutex_for,
		 reader_mutex_for_pred, reader_lock_for, reader_lock_for_pred});

	for (auto func : notify_functions) {
		int reset_value = 42;

		mutex_test(pop, true, false, write_notify, func);
		pop.get_root()->counter = reset_value;

		mutex_test(pop, true, true, write_notify, func);
		pop.get_root()->counter = reset_value;
	}

	std::vector<reader_type> not_notify_functions(
		{reader_mutex_until, reader_mutex_until_pred, reader_lock_until,
		 reader_lock_until_pred, reader_mutex_for,
		 reader_mutex_for_pred, reader_lock_for, reader_lock_for_pred});

	for (auto func : not_notify_functions) {
		int reset_value = 42;

		mutex_test(pop, false, false, write_notify, func);
		pop.get_root()->counter = reset_value;

		mutex_test(pop, false, true, write_notify, func);
		pop.get_root()->counter = reset_value;
	}

	/* pmemcheck related persist */
	pmemobj_persist(pop.get_handle(), &(pop.get_root()->counter),
			sizeof(pop.get_root()->counter));

	pop.close();

	DONE(NULL);
}
