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
 * obj_cpp_transaction.cpp -- cpp transaction test
 */

#include "unittest.h"

#include "libpmemobj/make_persistent.hpp"
#include "libpmemobj/mutex.hpp"
#include "libpmemobj/p.hpp"
#include "libpmemobj/persistent_ptr.hpp"
#include "libpmemobj/pool.hpp"
#include "libpmemobj/shared_mutex.hpp"

namespace
{
int counter = 0;
}

#ifndef __cpp_lib_uncaught_exceptions
#define __cpp_lib_uncaught_exceptions 201411
namespace std
{

int
uncaught_exceptions() noexcept
{
	return ::counter;
}

} /* namespace std */
#endif /* __cpp_lib_uncaught_exceptions */

#include "libpmemobj/transaction.hpp"

#define LAYOUT "cpp"

namespace nvobj = nvml::obj;

namespace
{

struct foo {
	nvobj::p<int> bar;
	nvobj::shared_mutex smtx;
};

struct root {
	nvobj::persistent_ptr<foo> pfoo;
	nvobj::persistent_ptr<nvobj::p<int>> parr;
	nvobj::mutex mtx;
};

void
fake_commit()
{
}

void
real_commit()
{
	nvobj::transaction::commit();
}

/*
 * Callable object class.
 */
class transaction_test {
public:
	/*
	 * Constructor.
	 */
	transaction_test(nvobj::pool<root> &pop_) : pop(pop_)
	{
	}

	/*
	 * The transaction worker.
	 */
	void
	operator()()
	{
		auto rootp = this->pop.get_root();

		if (rootp->pfoo == nullptr)
			rootp->pfoo = nvobj::make_persistent<foo>();

		rootp->pfoo->bar = 42;
	}

private:
	nvobj::pool<root> &pop;
};

/*
 * do_transaction -- internal C-style function transaction.
 */
void
do_transaction(nvobj::pool<root> &pop)
{
	auto rootp = pop.get_root();

	rootp->parr = nvobj::make_persistent<nvobj::p<int>>();

	*rootp->parr.get() = 5;
}

/*
 * Closure tests.
 */

/*
 * test_tx_no_throw_no_abort -- test transaction without exceptions and aborts
 */
void
test_tx_no_throw_no_abort(nvobj::pool<root> &pop)
{
	auto rootp = pop.get_root();

	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);

	try {
		nvobj::transaction::exec_tx(pop, [&]() {
			rootp->pfoo = nvobj::make_persistent<foo>();
		});
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(rootp->pfoo != nullptr);
	UT_ASSERT(rootp->parr == nullptr);

	try {
		nvobj::transaction::exec_tx(
			pop, std::bind(do_transaction, std::ref(pop)),
			rootp->mtx);
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(rootp->pfoo != nullptr);
	UT_ASSERT(rootp->parr != nullptr);
	UT_ASSERTeq(*rootp->parr.get(), 5);

	try {
		nvobj::transaction::exec_tx(pop, transaction_test(pop),
					    rootp->mtx, rootp->pfoo->smtx);
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(rootp->pfoo != nullptr);
	UT_ASSERT(rootp->parr != nullptr);
	UT_ASSERTeq(*rootp->parr.get(), 5);
	UT_ASSERTeq(rootp->pfoo->bar, 42);

	try {
		nvobj::transaction::exec_tx(pop, [&]() {
			nvobj::delete_persistent<foo>(rootp->pfoo);
			nvobj::delete_persistent<nvobj::p<int>>(rootp->parr);
			rootp->pfoo = nullptr;
			rootp->parr = nullptr;
		});
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);
}

/*
 * test_tx_throw_no_abort -- test transaction with exceptions and no aborts
 */
void
test_tx_throw_no_abort(nvobj::pool<root> &pop)
{
	auto rootp = pop.get_root();

	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);

	bool exception_thrown = false;
	try {
		nvobj::transaction::exec_tx(pop, [&]() {
			rootp->pfoo = nvobj::make_persistent<foo>();
			throw std::runtime_error("error");
		});
	} catch (std::runtime_error &re) {
		exception_thrown = true;
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(exception_thrown);
	exception_thrown = false;
	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);

	try {
		nvobj::transaction::exec_tx(pop, [&]() {
			rootp->pfoo = nvobj::make_persistent<foo>();
			nvobj::transaction::exec_tx(pop, [&]() {
				throw std::runtime_error("error");
			});
		});
	} catch (std::runtime_error &re) {
		exception_thrown = true;
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(exception_thrown);
	exception_thrown = false;
	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);

	try {
		nvobj::transaction::exec_tx(pop, [&]() {
			rootp->pfoo = nvobj::make_persistent<foo>();
			try {
				nvobj::transaction::exec_tx(pop, [&]() {
					throw std::runtime_error("error");
				});
			} catch (std::runtime_error &) {
				exception_thrown = true;
			} catch (...) {
				UT_ASSERT(0);
			}
			UT_ASSERT(exception_thrown);
			exception_thrown = false;
		});
	} catch (nvml::transaction_error &) {
		exception_thrown = true;
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(exception_thrown);
	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);
}

/*
 * test_tx_no_throw_abort -- test transaction with an abort and no exceptions
 */
void
test_tx_no_throw_abort(nvobj::pool<root> &pop)
{
	auto rootp = pop.get_root();

	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);

	bool exception_thrown = false;
	try {
		nvobj::transaction::exec_tx(pop, [&]() {
			rootp->pfoo = nvobj::make_persistent<foo>();
			nvobj::transaction::abort(-1);
		});
	} catch (nvml::manual_tx_abort &ta) {
		exception_thrown = true;
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(exception_thrown);
	exception_thrown = false;
	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);

	try {
		nvobj::transaction::exec_tx(pop, [&]() {
			rootp->pfoo = nvobj::make_persistent<foo>();
			nvobj::transaction::exec_tx(
				pop, [&]() { nvobj::transaction::abort(-1); });
		});
	} catch (nvml::manual_tx_abort &ta) {
		exception_thrown = true;
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(exception_thrown);
	exception_thrown = false;
	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);

	try {
		nvobj::transaction::exec_tx(pop, [&]() {
			rootp->pfoo = nvobj::make_persistent<foo>();
			try {
				nvobj::transaction::exec_tx(pop, [&]() {
					nvobj::transaction::abort(-1);
				});
			} catch (nvml::manual_tx_abort &) {
				exception_thrown = true;
			} catch (...) {
				UT_ASSERT(0);
			}
		});
	} catch (nvml::transaction_error &ta) {
		exception_thrown = true;
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERT(exception_thrown);
	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);
}

/*
 * Scoped tests.
 */

/*
 * test_tx_no_throw_no_abort_scope -- test transaction without exceptions
 *	and aborts
 */
template <typename T>
void
test_tx_no_throw_no_abort_scope(nvobj::pool<root> &pop,
				std::function<void()> commit)
{
	auto rootp = pop.get_root();

	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);

	try {
		T to(pop);
		rootp->pfoo = nvobj::make_persistent<foo>();
		commit();
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERTeq(nvobj::transaction::get_last_tx_error(), 0);
	UT_ASSERT(rootp->pfoo != nullptr);
	UT_ASSERT(rootp->parr == nullptr);

	try {
		T to(pop, rootp->mtx);
		do_transaction(pop);
		commit();
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERTeq(nvobj::transaction::get_last_tx_error(), 0);
	UT_ASSERT(rootp->pfoo != nullptr);
	UT_ASSERT(rootp->parr != nullptr);
	UT_ASSERTeq(*rootp->parr.get(), 5);

	try {
		T to(pop, rootp->mtx, rootp->pfoo->smtx);
		transaction_test tt(pop);
		tt.operator()();
		commit();
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERTeq(nvobj::transaction::get_last_tx_error(), 0);
	UT_ASSERT(rootp->pfoo != nullptr);
	UT_ASSERT(rootp->parr != nullptr);
	UT_ASSERTeq(*rootp->parr.get(), 5);
	UT_ASSERTeq(rootp->pfoo->bar, 42);

	try {
		T to(pop);
		nvobj::delete_persistent<foo>(rootp->pfoo);
		nvobj::delete_persistent<nvobj::p<int>>(rootp->parr);
		rootp->pfoo = nullptr;
		rootp->parr = nullptr;
		commit();
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERTeq(nvobj::transaction::get_last_tx_error(), 0);
	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);
}

/*
 * test_tx_throw_no_abort_scope -- test transaction with exceptions
 *	and no aborts
 */
template <typename T>
void
test_tx_throw_no_abort_scope(nvobj::pool<root> &pop)
{
	auto rootp = pop.get_root();

	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);

	bool exception_thrown = false;
	try {
		counter = 0;
		T to(pop);
		rootp->pfoo = nvobj::make_persistent<foo>();
		counter = 1;
		throw std::runtime_error("error");
	} catch (std::runtime_error &re) {
		exception_thrown = true;
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERTeq(nvobj::transaction::get_last_tx_error(), ECANCELED);
	UT_ASSERT(exception_thrown);
	exception_thrown = false;
	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);

	try {
		counter = 0;
		T to(pop);
		rootp->pfoo = nvobj::make_persistent<foo>();
		{
			T to_nested(pop);
			counter = 1;
			throw std::runtime_error("error");
		}
	} catch (std::runtime_error &re) {
		exception_thrown = true;
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERTeq(nvobj::transaction::get_last_tx_error(), ECANCELED);
	UT_ASSERT(exception_thrown);
	exception_thrown = false;
	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);

	try {
		counter = 0;
		T to(pop);
		rootp->pfoo = nvobj::make_persistent<foo>();
		try {
			T to_nested(pop);
			counter = 1;
			throw std::runtime_error("error");
		} catch (std::runtime_error &) {
			exception_thrown = true;
		} catch (...) {
			UT_ASSERT(0);
		}
		UT_ASSERT(exception_thrown);
		exception_thrown = false;
	} catch (...) {
		UT_ASSERT(0);
	}

	/* the transaction will be aborted silently */
	UT_ASSERTeq(nvobj::transaction::get_last_tx_error(), ECANCELED);
	UT_ASSERT(!exception_thrown);
	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);
}

/*
 * test_tx_no_throw_abort_scope -- test transaction with an abort
 *	and no exceptions
 */
template <typename T>
void
test_tx_no_throw_abort_scope(nvobj::pool<root> &pop)
{
	auto rootp = pop.get_root();

	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);

	bool exception_thrown = false;
	try {
		counter = 0;
		T to(pop);
		rootp->pfoo = nvobj::make_persistent<foo>();
		counter = 1;
		nvobj::transaction::abort(ECANCELED);
	} catch (nvml::manual_tx_abort &ta) {
		exception_thrown = true;
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERTeq(nvobj::transaction::get_last_tx_error(), ECANCELED);
	UT_ASSERT(exception_thrown);
	exception_thrown = false;
	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);

	try {
		counter = 0;
		T to(pop);
		rootp->pfoo = nvobj::make_persistent<foo>();
		{
			T to_nested(pop);
			counter = 1;
			nvobj::transaction::abort(EINVAL);
		}
	} catch (nvml::manual_tx_abort &ta) {
		exception_thrown = true;
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERTeq(nvobj::transaction::get_last_tx_error(), EINVAL);
	UT_ASSERT(exception_thrown);
	exception_thrown = false;
	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);

	try {
		counter = 0;
		T to(pop);
		rootp->pfoo = nvobj::make_persistent<foo>();
		try {
			T to_nested(pop);
			counter = 1;
			nvobj::transaction::abort(-1);
		} catch (nvml::manual_tx_abort &) {
			exception_thrown = true;
		} catch (...) {
			UT_ASSERT(0);
		}
	} catch (nvml::transaction_error &ta) {
		exception_thrown = true;
	} catch (...) {
		UT_ASSERT(0);
	}

	UT_ASSERTeq(nvobj::transaction::get_last_tx_error(), -1);
	UT_ASSERT(exception_thrown);
	UT_ASSERT(rootp->pfoo == nullptr);
	UT_ASSERT(rootp->parr == nullptr);
}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_cpp_transaction");

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	nvobj::pool<root> pop;
	try {
		pop = nvobj::pool<root>::create(path, LAYOUT, PMEMOBJ_MIN_POOL,
						S_IWUSR | S_IRUSR);
	} catch (...) {
		UT_FATAL("!pmemobj_create: %s", path);
	}

	test_tx_no_throw_no_abort(pop);
	test_tx_throw_no_abort(pop);
	test_tx_no_throw_abort(pop);

	test_tx_no_throw_no_abort_scope<nvobj::transaction::manual>(
		pop, real_commit);
	test_tx_throw_no_abort_scope<nvobj::transaction::manual>(pop);
	test_tx_no_throw_abort_scope<nvobj::transaction::manual>(pop);

	test_tx_no_throw_no_abort_scope<nvobj::transaction::automatic>(
		pop, fake_commit);
	test_tx_throw_no_abort_scope<nvobj::transaction::automatic>(pop);
	test_tx_no_throw_abort_scope<nvobj::transaction::automatic>(pop);

	pop.close();

	DONE(NULL);
}
