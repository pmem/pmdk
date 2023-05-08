/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2023, Intel Corporation */

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
	TX_PARAM_CB,	 /* pmemobj_tx_callback cb, void *arg */
};

enum pobj_log_type {
	TX_LOG_TYPE_SNAPSHOT,
	TX_LOG_TYPE_INTENT,
};

enum pobj_tx_failure_behavior {
	POBJ_TX_FAILURE_ABORT,
	POBJ_TX_FAILURE_RETURN,
};

#if !defined(pmdk_use_attr_deprec_with_msg) && defined(__COVERITY__)
#define pmdk_use_attr_deprec_with_msg 0
#endif

#if !defined(pmdk_use_attr_deprec_with_msg) && defined(__clang__)
#if __has_extension(attribute_deprecated_with_message)
#define pmdk_use_attr_deprec_with_msg 1
#else
#define pmdk_use_attr_deprec_with_msg 0
#endif
#endif

#if !defined(pmdk_use_attr_deprec_with_msg) && \
		defined(__GNUC__) && !defined(__INTEL_COMPILER)
#if __GNUC__ * 100 + __GNUC_MINOR__ >= 601 /* 6.1 */
#define pmdk_use_attr_deprec_with_msg 1
#else
#define pmdk_use_attr_deprec_with_msg 0
#endif
#endif

#if !defined(pmdk_use_attr_deprec_with_msg)
#define pmdk_use_attr_deprec_with_msg 0
#endif

#if pmdk_use_attr_deprec_with_msg
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

#define POBJ_TX_XALLOC_VALID_FLAGS	(POBJ_XALLOC_ZERO |\
	POBJ_XALLOC_NO_FLUSH |\
	POBJ_XALLOC_ARENA_MASK |\
	POBJ_XALLOC_CLASS_MASK |\
	POBJ_XALLOC_NO_ABORT)

#define POBJ_XADD_NO_FLUSH		POBJ_FLAG_NO_FLUSH
#define POBJ_XADD_NO_SNAPSHOT		POBJ_FLAG_NO_SNAPSHOT
#define POBJ_XADD_ASSUME_INITIALIZED	POBJ_FLAG_ASSUME_INITIALIZED
#define POBJ_XADD_NO_ABORT		POBJ_FLAG_TX_NO_ABORT
#define POBJ_XADD_VALID_FLAGS	(POBJ_XADD_NO_FLUSH |\
	POBJ_XADD_NO_SNAPSHOT |\
	POBJ_XADD_ASSUME_INITIALIZED |\
	POBJ_XADD_NO_ABORT)

#define POBJ_XLOCK_NO_ABORT		POBJ_FLAG_TX_NO_ABORT
#define POBJ_XLOCK_VALID_FLAGS	(POBJ_XLOCK_NO_ABORT)

#define POBJ_XFREE_NO_ABORT		POBJ_FLAG_TX_NO_ABORT
#define POBJ_XFREE_VALID_FLAGS	(POBJ_XFREE_NO_ABORT)

#define POBJ_XPUBLISH_NO_ABORT		POBJ_FLAG_TX_NO_ABORT
#define POBJ_XPUBLISH_VALID_FLAGS	(POBJ_XPUBLISH_NO_ABORT)

#define POBJ_XLOG_APPEND_BUFFER_NO_ABORT	POBJ_FLAG_TX_NO_ABORT
#define POBJ_XLOG_APPEND_BUFFER_VALID_FLAGS (POBJ_XLOG_APPEND_BUFFER_NO_ABORT)
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
 * 'Flags' is a bitmask of the following values:
 *  - POBJ_XLOCK_NO_ABORT - if the function does not end successfully,
 *  do not abort the transaction and return the error number.
 */
int pmemobj_tx_xlock(enum pobj_tx_param type, void *lockp, uint64_t flags);

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
 * Otherwise, stage changes to TX_STAGE_ONABORT and an error number is returned.
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
 * Otherwise, stage changes to TX_STAGE_ONABORT and an error number is returned.
 *
 * This function must be called during TX_STAGE_WORK.
 */
int pmemobj_tx_add_range_direct(const void *ptr, size_t size);

/*
 * Behaves exactly the same as pmemobj_tx_add_range when 'flags' equals 0.
 * 'Flags' is a bitmask of the following values:
 *  - POBJ_XADD_NO_FLUSH - skips flush on commit
 *  - POBJ_XADD_NO_SNAPSHOT - added range will not be snapshotted
 *  - POBJ_XADD_ASSUME_INITIALIZED - added range is assumed to be initialized
 *  - POBJ_XADD_NO_ABORT - if the function does not end successfully,
 *  do not abort the transaction and return the error number.
 */
int pmemobj_tx_xadd_range(PMEMoid oid, uint64_t off, size_t size,
		uint64_t flags);

/*
 * Behaves exactly the same as pmemobj_tx_add_range_direct when 'flags' equals
 * 0. 'Flags' is a bitmask of the following values:
 *  - POBJ_XADD_NO_FLUSH - skips flush on commit
 *  - POBJ_XADD_NO_SNAPSHOT - added range will not be snapshotted
 *  - POBJ_XADD_ASSUME_INITIALIZED - added range is assumed to be initialized
 *  - POBJ_XADD_NO_ABORT - if the function does not end successfully,
 *  do not abort the transaction and return the error number.
 */
int pmemobj_tx_xadd_range_direct(const void *ptr, size_t size, uint64_t flags);

/*
 * Transactionally allocates a new object.
 *
 * If successful, returns PMEMoid.
 * Otherwise, stage changes to TX_STAGE_ONABORT and an OID_NULL is returned.
 *
 * This function must be called during TX_STAGE_WORK.
 */
PMEMoid pmemobj_tx_alloc(size_t size, uint64_t type_num);

/*
 * Transactionally allocates a new object.
 *
 * If successful, returns PMEMoid.
 * Otherwise, stage changes to TX_STAGE_ONABORT and an OID_NULL is returned.
 * 'Flags' is a bitmask of the following values:
 *  - POBJ_XALLOC_ZERO - zero the allocated object
 *  - POBJ_XALLOC_NO_FLUSH - skip flush on commit
 *  - POBJ_XALLOC_NO_ABORT - if the function does not end successfully,
 *  do not abort the transaction and return the error number.
 *
 * This function must be called during TX_STAGE_WORK.
 */
PMEMoid pmemobj_tx_xalloc(size_t size, uint64_t type_num, uint64_t flags);

/*
 * Transactionally allocates new zeroed object.
 *
 * If successful, returns PMEMoid.
 * Otherwise, stage changes to TX_STAGE_ONABORT and an OID_NULL is returned.
 *
 * This function must be called during TX_STAGE_WORK.
 */
PMEMoid pmemobj_tx_zalloc(size_t size, uint64_t type_num);

/*
 * Transactionally resizes an existing object.
 *
 * If successful, returns PMEMoid.
 * Otherwise, stage changes to TX_STAGE_ONABORT and an OID_NULL is returned.
 *
 * This function must be called during TX_STAGE_WORK.
 */
PMEMoid pmemobj_tx_realloc(PMEMoid oid, size_t size, uint64_t type_num);

/*
 * Transactionally resizes an existing object, if extended new space is zeroed.
 *
 * If successful, returns PMEMoid.
 * Otherwise, stage changes to TX_STAGE_ONABORT and an OID_NULL is returned.
 *
 * This function must be called during TX_STAGE_WORK.
 */
PMEMoid pmemobj_tx_zrealloc(PMEMoid oid, size_t size, uint64_t type_num);

/*
 * Transactionally allocates a new object with duplicate of the string s.
 *
 * If successful, returns PMEMoid.
 * Otherwise, stage changes to TX_STAGE_ONABORT and an OID_NULL is returned.
 *
 * This function must be called during TX_STAGE_WORK.
 */
PMEMoid pmemobj_tx_strdup(const char *s, uint64_t type_num);

/*
 * Transactionally allocates a new object with duplicate of the string s.
 *
 * If successful, returns PMEMoid.
 * Otherwise, stage changes to TX_STAGE_ONABORT and an OID_NULL is returned.
 * 'Flags' is a bitmask of the following values:
 *  - POBJ_XALLOC_ZERO - zero the allocated object
 *  - POBJ_XALLOC_NO_FLUSH - skip flush on commit
 *  - POBJ_XALLOC_NO_ABORT - if the function does not end successfully,
 *  do not abort the transaction and return the error number.
 *
 * This function must be called during TX_STAGE_WORK.
 */
PMEMoid pmemobj_tx_xstrdup(const char *s, uint64_t type_num, uint64_t flags);

/*
 * Transactionally allocates a new object with duplicate of the wide character
 * string s.
 *
 * If successful, returns PMEMoid.
 * Otherwise, stage changes to TX_STAGE_ONABORT and an OID_NULL is returned.
 *
 * This function must be called during TX_STAGE_WORK.
 */
PMEMoid pmemobj_tx_wcsdup(const wchar_t *s, uint64_t type_num);

/*
 * Transactionally allocates a new object with duplicate of the wide character
 * string s.
 *
 * If successful, returns PMEMoid.
 * Otherwise, stage changes to TX_STAGE_ONABORT and an OID_NULL is returned.
 * 'Flags' is a bitmask of the following values:
 *  - POBJ_XALLOC_ZERO - zero the allocated object
 *  - POBJ_XALLOC_NO_FLUSH - skip flush on commit
 *  - POBJ_XALLOC_NO_ABORT - if the function does not end successfully,
 *  do not abort the transaction and return the error number.
 *
 * This function must be called during TX_STAGE_WORK.
 */
PMEMoid pmemobj_tx_xwcsdup(const wchar_t *s, uint64_t type_num, uint64_t flags);

/*
 * Transactionally frees an existing object.
 *
 * If successful, returns zero.
 * Otherwise, stage changes to TX_STAGE_ONABORT and an error number is returned.
 *
 * This function must be called during TX_STAGE_WORK.
 */
int pmemobj_tx_free(PMEMoid oid);

/*
 * Transactionally frees an existing object.
 *
 * If successful, returns zero.
 * Otherwise, the stage changes to TX_STAGE_ONABORT and the error number is
 * returned.
 * 'Flags' is a bitmask of the following values:
 *  - POBJ_XFREE_NO_ABORT - if the function does not end successfully,
 *  do not abort the transaction and return the error number.
 *
 * This function must be called during TX_STAGE_WORK.
 */
int pmemobj_tx_xfree(PMEMoid oid, uint64_t flags);

/*
 * Append user allocated buffer to the ulog.
 *
 * If successful, returns zero.
 * Otherwise, stage changes to TX_STAGE_ONABORT and an error number is returned.
 *
 * This function must be called during TX_STAGE_WORK.
 */
int pmemobj_tx_log_append_buffer(enum pobj_log_type type,
	void *addr, size_t size);

/*
 * Append user allocated buffer to the ulog.
 *
 * If successful, returns zero.
 * Otherwise, stage changes to TX_STAGE_ONABORT and an error number is returned.
 * 'Flags' is a bitmask of the following values:
 *  - POBJ_XLOG_APPEND_BUFFER_NO_ABORT - if the function does not end
 *  successfully, do not abort the transaction and return the error number.
 *
 * This function must be called during TX_STAGE_WORK.
 */
int pmemobj_tx_xlog_append_buffer(enum pobj_log_type type,
	void *addr, size_t size, uint64_t flags);

/*
 * Enables or disables automatic ulog allocations.
 *
 * If successful, returns zero.
 * Otherwise, stage changes to TX_STAGE_ONABORT and an error number is returned.
 *
 * This function must be called during TX_STAGE_WORK.
 */
int pmemobj_tx_log_auto_alloc(enum pobj_log_type type, int on_off);

/*
 * Calculates and returns size for user buffers for snapshots.
 */
size_t pmemobj_tx_log_snapshots_max_size(size_t *sizes, size_t nsizes);

/*
 * Calculates and returns size for user buffers for intents.
 */
size_t pmemobj_tx_log_intents_max_size(size_t nintents);

/*
 * Sets volatile pointer to the user data for the current transaction.
 */
void pmemobj_tx_set_user_data(void *data);

/*
 * Gets volatile pointer to the user data associated with the current
 * transaction.
 */
void *pmemobj_tx_get_user_data(void);

/*
 * Sets the failure behavior of transactional functions.
 *
 * This function must be called during TX_STAGE_WORK.
 */
void pmemobj_tx_set_failure_behavior(enum pobj_tx_failure_behavior behavior);

/*
 * Returns failure behavior for the current transaction.
 *
 * This function must be called during TX_STAGE_WORK.
 */
enum pobj_tx_failure_behavior pmemobj_tx_get_failure_behavior(void);

#ifdef __cplusplus
}
#endif

#endif	/* libpmemobj/tx_base.h */
