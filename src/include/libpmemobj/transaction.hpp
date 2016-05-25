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

/**
 * @file
 * C++ pmemobj transactions.
 */

#ifndef LIBPMEMOBJ_TRANSACTION_HPP
#define LIBPMEMOBJ_TRANSACTION_HPP

#include <functional>

#include "libpmemobj.h"
#include "libpmemobj/detail/pexceptions.hpp"
#include "libpmemobj/pool.hpp"

namespace nvml
{

namespace obj
{

/**
 * C++ transaction handler class.
 *
 * This class is the pmemobj transaction handler. Scoped transactions
 * are handled through two internal classes: @ref manual and
 * @ref automatic.
 * - @ref manual transactions need to be committed manually, otherwise
 *	they will be aborted on object destruction.\n
 * - @ref automatic transactions are only available in C++17. They
 *	handle transaction commit/abort automatically.
 *
 * This class also exposes a closure-like transaction API, which is the
 * preferred way of handling transactions.
 *
 * The typical usage example would be:
 * @snippet doc_snippets/transaction.cpp general_tx_example
 */
class transaction {
public:
	/**
	 * C++ manual scope transaction class.
	 *
	 * This class is one of pmemobj transaction handlers. All
	 * operations between creating and destroying the transaction
	 * object are treated as performed in a transaction block and
	 * can be rolled back. The manual transaction has to be
	 * committed explicitly otherwise it will abort.
	 *
	 * The locks are held for the entire duration of the transaction. They
	 * are released at the end of the scope, so within the `catch` block,
	 * they are already unlocked. If the cleanup action requires access to
	 * data within a critical section, the locks have to be manually
	 * acquired once again.
	 *
	 *The typical usage example would be:
	 * @snippet doc_snippets/transaction.cpp manual_tx_example
	 */
	class manual {
	public:
		/**
		 * RAII constructor with pmem resident locks.
		 *
		 * Start pmemobj transaction and add list of locks to
		 * new transaction. The list of locks may be empty.
		 *
		 * @param[in,out] pop pool object.
		 * @param[in,out] locks locks of obj::mutex or
		 *	obj::shared_mutex type.
		 *
		 * @throw nvml::transaction_error when pmemobj_tx_begin
		 * function or locks adding failed.
		 */
		template <typename... L>
		manual(obj::pool_base &pop, L &... locks)
		{
			if (pmemobj_tx_begin(pop.get_handle(), NULL,
					     TX_LOCK_NONE) != 0)
				throw transaction_error(
					"failed to start transaction");

			auto err = add_lock(locks...);

			if (err) {
				pmemobj_tx_abort(EINVAL);
				throw transaction_error("failed to"
							" add lock");
			}
		}

		/**
		 * Destructor.
		 *
		 * End pmemobj transaction. If the transaction has not
		 * been committed before object destruction, an abort
		 * will be issued.
		 */
		~manual() noexcept
		{
			/* normal exit or with an active exception */
			if (pmemobj_tx_stage() == TX_STAGE_WORK)
				pmemobj_tx_abort(ECANCELED);

			pmemobj_tx_end();
		}

		/**
		 * Deleted copy constructor.
		 */
		manual(const manual &p) = delete;

		/**
		 * Deleted move constructor.
		 */
		manual(const manual &&p) = delete;

		/**
		 * Deleted assignment operator.
		 */
		manual &operator=(const manual &p) = delete;

		/**
		 * Deleted move assignment operator.
		 */
		manual &operator=(manual &&p) = delete;
	};

#ifdef __cpp_lib_uncaught_exceptions
	/**
	 * C++ automatic scope transaction class.
	 *
	 * This class is one of pmemobj transaction handlers. All
	 * operations between creating and destroying the transaction
	 * object are treated as performed in a transaction block and
	 * can be rolled back. If you have a C++17 compliant compiler,
	 * the automatic transaction will commit and abort
	 * automatically depending on the context of object destruction.
	 *
	 * The locks are held for the entire duration of the transaction. They
	 * are released at the end of the scope, so within the `catch` block,
	 * they are already unlocked. If the cleanup action requires access to
	 * data within a critical section, the locks have to be manually
	 * acquired once again.
	 *
	 * The typical usage example would be:
	 * @snippet doc_snippets/transaction.cpp automatic_tx_example
	 */
	class automatic {
	public:
		/**
		 * RAII constructor with pmem resident locks.
		 *
		 * Start pmemobj transaction and add list of locks to
		 * new transaction. The list of locks may be empty.
		 *
		 * This class is only available if the
		 * `__cpp_lib_uncaught_exceptions` feature macro is
		 * defined. This is a C++17 feature.
		 *
		 * @param[in,out] pop pool object.
		 * @param[in,out] locks locks of obj::mutex or
		 *	obj::shared_mutex type.
		 *
		 * @throw nvml::transaction_error when pmemobj_tx_begin
		 * function or locks adding failed.
		 */
		template <typename... L>
		automatic(obj::pool_base &pop, L &... locks)
		{
			if (pmemobj_tx_begin(pop.get_handle(), NULL,
					     TX_LOCK_NONE) != 0)
				throw transaction_error(
					"failed to start transaction");

			auto err = add_lock(locks...);

			if (err) {
				pmemobj_tx_abort(EINVAL);
				throw transaction_error("failed to add"
							" lock");
			}
		}

		/**
		 * Destructor.
		 *
		 * End pmemobj transaction. Depending on the context
		 * of object destruction, the transaction will
		 * automatically be either committed or aborted.
		 */
		~automatic() noexcept
		{
			/* manual abort or commit end transaction */
			if (pmemobj_tx_stage() != TX_STAGE_WORK) {
				pmemobj_tx_end();
				return;
			}

			if (this->exceptions.new_uncaught_exception())
				/* exit with an active exception */
				pmemobj_tx_abort(ECANCELED);
			else
				/* normal exit commit tx */
				pmemobj_tx_commit();

			pmemobj_tx_end();
		}

		/**
		 * Deleted copy constructor.
		 */
		automatic(const automatic &p) = delete;

		/**
		 * Deleted move constructor.
		 */
		automatic(const automatic &&p) = delete;

		/**
		 * Deleted assignment operator.
		 */
		automatic &operator=(const automatic &p) = delete;

		/**
		 * Deleted move assignment operator.
		 */
		automatic &operator=(automatic &&p) = delete;

	private:
		/**
		 * Internal class for counting active exceptions.
		 */
		class uncaught_exception_counter {
		public:
			/**
			 * Default constructor.
			 *
			 * Sets the number of active exceptions on
			 * object creation.
			 */
			uncaught_exception_counter()
			    : count(std::uncaught_exceptions())
			{
			}

			/**
			 * Notifies is a new exception is being handled.
			 *
			 * @return true if a new exception was throw
			 *	in the scope of the object, false
			 *	otherwise.
			 */
			bool
			new_uncaught_exception()
			{
				return std::uncaught_exceptions() > this->count;
			}

		private:
			/**
			 * The number of active exceptions.
			 */
			int count;
		} exceptions;
	};
#endif /* __cpp_lib_uncaught_exceptions */

	/*
	 * Deleted default constructor.
	 */
	transaction() = delete;

	/**
	 * Default destructor.
	 *
	 * End pmemobj transaction. If the transaction has not been
	 * committed before object destruction, an abort will be issued.
	 */
	~transaction() noexcept = delete;

	/**
	 * Manually abort the current transaction.
	 *
	 * If called within an inner transaction, the outer transactions
	 * will also be aborted.
	 *
	 * @param[in] err the error to be reported as the reason of the
	 *	abort.
	 *
	 * @throw transaction_error if the transaction is in an invalid
	 *	state.
	 * @throw manual_tx_abort this exception is thrown to
	 *	signify a transaction abort.
	 */
	static void
	abort(int err)
	{
		if (pmemobj_tx_stage() != TX_STAGE_WORK)
			throw transaction_error("wrong stage for"
						" abort");

		pmemobj_tx_abort(err);
		throw manual_tx_abort("explicit abort " + std::to_string(err));
	}

	/**
	 * Manually commit a transaction.
	 *
	 * It is the sole responsibility of the caller, that after the
	 * call to transaction::commit() no other operations are done
	 * within the transaction.
	 *
	 * @throw transaction_error on any errors with ending the
	 *	transaction.
	 */
	static void
	commit()
	{
		if (pmemobj_tx_stage() != TX_STAGE_WORK)
			throw transaction_error("wrong stage for"
						" commit");

		pmemobj_tx_commit();
	}

	static int
	get_last_tx_error() noexcept
	{
		return pmemobj_tx_errno();
	}

	/**
	 * Execute a closure-like transaction and lock `locks`.
	 *
	 * The locks have to be persistent memory resident locks. An
	 * attempt to lock the locks will be made. If any of the
	 * specified locks is already locked, the method will block.
	 * The locks are held until the end of the transaction. The
	 * transaction does not have to be committed manually. Manual
	 * aborts will end the transaction with an active exception.
	 *
	 * If an exception is thrown within the transaction, it gets aborted
	 * and the exception is rethrown. Therefore extra care has to be taken
	 * with proper error handling.
	 *
	 * The locks are held for the entire duration of the transaction. They
	 * are released at the end of the scope, so within the `catch` block,
	 * they are already unlocked. If the cleanup action requires access to
	 * data within a critical section, the locks have to be manually
	 * acquired once again.
	 *
	 * @param[in,out] pool the pool in which the transaction will take
	 *	place.
	 * @param[in] tx an std::function<void ()> which will perform
	 *	operations within this transaction.
	 * @param[in,out] locks locks to be taken for the duration of
	 *	the transaction.
	 *
	 * @throw transaction_error on any error pertaining the execution
	 *	of the transaction.
	 * @throw manual_tx_abort on manual transaction abort.
	 */
	template <typename... Locks>
	static void
	exec_tx(pool_base &pool, std::function<void()> tx, Locks &... locks)
	{
		if (pmemobj_tx_begin(pool.get_handle(), NULL, TX_LOCK_NONE) !=
		    0)
			throw transaction_error("failed to start transaction");

		auto err = add_lock(locks...);

		if (err) {
			pmemobj_tx_abort(err);
			pmemobj_tx_end();
			throw transaction_error("failed to add a lock to the"
						" transaction");
		}

		try {
			tx();
		} catch (manual_tx_abort &) {
			pmemobj_tx_end();
			throw;
		} catch (...) {
			/* first exception caught */
			if (pmemobj_tx_stage() == TX_STAGE_WORK)
				pmemobj_tx_abort(ECANCELED);

			/* waterfall tx_end for outer tx */
			pmemobj_tx_end();
			throw;
		}

		auto stage = pmemobj_tx_stage();

		if (stage == TX_STAGE_WORK) {
			pmemobj_tx_commit();
		} else if (stage == TX_STAGE_ONABORT) {
			pmemobj_tx_end();
			throw transaction_error("transaction aborted");
		} else if (stage == TX_STAGE_NONE) {
			throw transaction_error("transaction ended"
						"prematurely");
		}

		pmemobj_tx_end();
	}

private:
	/**
	 * Recursively add locks to the active transaction.
	 *
	 * The locks are taken in the provided order.
	 *
	 * @param[in,out] lock the lock to add.
	 * @param[in,out] locks the rest of the locks to be added to the
	 *	active transaction.
	 *
	 * @return error number if adding any of the locks failed,
	 *	0 otherwise.
	 */
	template <typename L, typename... Locks>
	static int
	add_lock(L &lock, Locks &... locks) noexcept
	{
		auto err =
			pmemobj_tx_lock(lock.lock_type(), lock.native_handle());

		if (err)
			return err;

		return add_lock(locks...);
	}

	/**
	 * Method ending the recursive algorithm.
	 */
	static inline int
	add_lock() noexcept
	{
		return 0;
	}
};

} /* namespace obj */

} /* namespace nvml */

#endif /* LIBPMEMOBJ_TRANSACTION_HPP */
