/*
 * Copyright 2014-2017, Intel Corporation
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
 * libpmemobj/tx_base.h -- definitions of libpmemobj transactional entry points
 */

#ifndef LIBPMEMOBJ_TX_BASE_H
#define LIBPMEMOBJ_TX_BASE_H 1

#include <setjmp.h>

#include <libpmemobj/base.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Transactions
 *
 * Stages are changed only by the pmemobj_tx_* functions, each transition
 * to the TX_STAGE_ONABORT is followed by a longjmp to the jmp_buf provided in
 * the pmemobj_tx_begin function.
 */
enum pobj_tx_stage {
	TX_STAGE_NONE,		/* no transaction in this thread */
	TX_STAGE_WORK,		/* transaction in progress */
	TX_STAGE_ONCOMMIT,	/* successfully committed */
	TX_STAGE_ONABORT,	/* tx_begin failed or transaction aborted */
	TX_STAGE_FINALLY,	/* always called */

	MAX_TX_STAGE
};

/*
 * Always returns the current transaction stage for a thread.
 */
enum pobj_tx_stage pmemobj_tx_stage(void);

enum pobj_tx_param {
	TX_PARAM_NONE,
	TX_PARAM_MUTEX,	 /* PMEMmutex */
	TX_PARAM_RWLOCK, /* PMEMrwlock */
	/* EXPERIMENTAL */ TX_PARAM_CB,	 /* pmemobj_tx_callback cb, void *arg */
};

#if !defined(_has_deprecated_with_message) && defined(__clang__)
#if __has_extension(attribute_deprecated_with_message)
#define _has_deprecated_with_message
#endif
#endif

#if !defined(_has_deprecated_with_message) && defined(__GNUC__)
#if __GNUC__ * 100 + __GNUC_MINOR__ >= 601 /* 6.1 */
#define _has_deprecated_with_message
#endif
#endif

#ifdef _has_deprecated_with_message
#define tx_lock_deprecated __attribute__((deprecated(\
		"enum pobj_tx_lock is deprecated, use enum pobj_tx_param")))
#else
#define tx_lock_deprecated
#endif

/* deprecated, do not use */
enum tx_lock_deprecated pobj_tx_lock {
	TX_LOCK_NONE   tx_lock_deprecated = TX_PARAM_NONE,
	TX_LOCK_MUTEX  tx_lock_deprecated = TX_PARAM_MUTEX,
	TX_LOCK_RWLOCK tx_lock_deprecated = TX_PARAM_RWLOCK,
};

typedef void (*pmemobj_tx_callback)(PMEMobjpool *pop, enum pobj_tx_stage stage,
		void *);

#define POBJ_FLAG_ZERO		(((uint64_t)1) << 0)
#define POBJ_FLAG_NO_FLUSH	(((uint64_t)1) << 1)

#define POBJ_XALLOC_ZERO	POBJ_FLAG_ZERO
#define POBJ_XALLOC_NO_FLUSH	POBJ_FLAG_NO_FLUSH
#define POBJ_XALLOC_VALID_FLAGS	(POBJ_XALLOC_ZERO | POBJ_XALLOC_NO_FLUSH)

#define POBJ_XADD_NO_FLUSH	POBJ_FLAG_NO_FLUSH
#define POBJ_XADD_VALID_FLAGS	POBJ_XADD_NO_FLUSH

/*
 * Starts a new transaction in the current thread.
 * If called within an open transaction, starts a nested transaction.
 *
 * If successful, transaction stage changes to TX_STAGE_WORK and function
 * returns zero. Otherwise, stage changes to TX_STAGE_ONABORT and an error
 * number is returned.
 */
int pmemobj_tx_begin(PMEMobjpool *pop, jmp_buf env, ...);

/*
 * Adds lock of given type to current transaction.
 */
int pmemobj_tx_lock(enum pobj_tx_param type, void *lockp);

/*
 * Aborts current transaction
 *
 * Causes transition to TX_STAGE_ONABORT.
 *
 * This function must be called during TX_STAGE_WORK.
 */
void pmemobj_tx_abort(int errnum);

/*
 * Commits current transaction
 *
 * This function must be called during TX_STAGE_WORK.
 */
void pmemobj_tx_commit(void);

/*
 * Cleanups current transaction. Must always be called after pmemobj_tx_begin,
 * even if starting the transaction failed.
 *
 * If called during TX_STAGE_NONE, has no effect.
 *
 * Always causes transition to TX_STAGE_NONE.
 *
 * If transaction was successful, returns 0. Otherwise returns error code set
 * by pmemobj_tx_abort.
 *
 * This function must *not* be called during TX_STAGE_WORK.
 */
int pmemobj_tx_end(void);

/*
 * Performs the actions associated with current stage of the transaction,
 * and makes the transition to the next stage. Current stage must always
 * be obtained by calling pmemobj_tx_stage.
 *
 * This function must be called in transaction.
 */
void pmemobj_tx_process(void);

/*
 * Returns last transaction error code.
 */
int pmemobj_tx_errno(void);

/*
 * Takes a "snapshot" of the memory block of given size and located at given
 * offset 'off' in the object 'oid' and saves it in the undo log.
 * The application is then free to directly modify the object in that memory
 * range. In case of failure or abort, all the changes within this range will
 * be rolled-back automatically.
 *
 * If successful, returns zero.
 * Otherwise, state changes to TX_STAGE_ONABORT and an error number is returned.
 *
 * This function must be called during TX_STAGE_WORK.
 */
int pmemobj_tx_add_range(PMEMoid oid, uint64_t off, size_t size);

/*
 * Takes a "snapshot" of the given memory region and saves it in the undo log.
 * The application is then free to directly modify the object in that memory
 * range. In case of failure or abort, all the changes within this range will
 * be rolled-back automatically. The supplied block of memory has to be within
 * the given pool.
 *
 * If successful, returns zero.
 * Otherwise, state changes to TX_STAGE_ONABORT and an error number is returned.
 *
 * This function must be called during TX_STAGE_WORK.
 */
int pmemobj_tx_add_range_direct(const void *ptr, size_t size);

/*
 * Behaves exactly the same as pmemobj_tx_add_range when 'flags' equals 0.
 * 'Flags' is a bitmask of the following values:
 *  - POBJ_XADD_NO_FLUSH - skips flush on commit
 * This is EXPERIMENTAL API.
 */
int pmemobj_tx_xadd_range(PMEMoid oid, uint64_t off, size_t size,
		uint64_t flags);

/*
 * Behaves exactly the same as pmemobj_tx_add_range_direct when 'flags' equals
 * 0. 'Flags' is a bitmask of the following values:
 *  - POBJ_XADD_NO_FLUSH - skips flush on commit
 * This is EXPERIMENTAL API.
 */
int pmemobj_tx_xadd_range_direct(const void *ptr, size_t size, uint64_t flags);

/*
 * Transactionally allocates a new object.
 *
 * If successful, returns PMEMoid.
 * Otherwise, state changes to TX_STAGE_ONABORT and an OID_NULL is returned.
 *
 * This function must be called during TX_STAGE_WORK.
 */
PMEMoid pmemobj_tx_alloc(size_t size, uint64_t type_num);

/*
 * Transactionally allocates a new object.
 *
 * If successful, returns PMEMoid.
 * Otherwise, state changes to TX_STAGE_ONABORT and an OID_NULL is returned.
 * 'Flags' is a bitmask of the following values:
 *  - POBJ_XALLOC_ZERO - zero the allocated object
 *  - POBJ_XALLOC_NO_FLUSH - skip flush on commit
 *
 * This function must be called during TX_STAGE_WORK.
 * This is EXPERIMENTAL API.
 */
PMEMoid pmemobj_tx_xalloc(size_t size, uint64_t type_num, uint64_t flags);

/*
 * Transactionally allocates new zeroed object.
 *
 * If successful, returns PMEMoid.
 * Otherwise, state changes to TX_STAGE_ONABORT and an OID_NULL is returned.
 *
 * This function must be called during TX_STAGE_WORK.
 */
PMEMoid pmemobj_tx_zalloc(size_t size, uint64_t type_num);

/*
 * Transactionally resizes an existing object.
 *
 * If successful, returns PMEMoid.
 * Otherwise, state changes to TX_STAGE_ONABORT and an OID_NULL is returned.
 *
 * This function must be called during TX_STAGE_WORK.
 */
PMEMoid pmemobj_tx_realloc(PMEMoid oid, size_t size, uint64_t type_num);

/*
 * Transactionally resizes an existing object, if extended new space is zeroed.
 *
 * If successful, returns PMEMoid.
 * Otherwise, state changes to TX_STAGE_ONABORT and an OID_NULL is returned.
 *
 * This function must be called during TX_STAGE_WORK.
 */
PMEMoid pmemobj_tx_zrealloc(PMEMoid oid, size_t size, uint64_t type_num);

/*
 * Transactionally allocates a new object with duplicate of the string s.
 *
 * If successful, returns PMEMoid.
 * Otherwise, state changes to TX_STAGE_ONABORT and an OID_NULL is returned.
 *
 * This function must be called during TX_STAGE_WORK.
 */
PMEMoid pmemobj_tx_strdup(const char *s, uint64_t type_num);

/*
 * Transactionally frees an existing object.
 *
 * If successful, returns zero.
 * Otherwise, state changes to TX_STAGE_ONABORT and an error number is returned.
 *
 * This function must be called during TX_STAGE_WORK.
 */
int pmemobj_tx_free(PMEMoid oid);

#ifdef __cplusplus
}
#endif

#endif	/* libpmemobj/tx_base.h */
