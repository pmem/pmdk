/*
 * Copyright (c) 2014-2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * libpmemobj.h -- definitions of libpmemobj entry points
 *
 * This library provides support for programming with persistent memory (pmem).
 *
 * libpmemobj provides a pmem-resident transactional object store.
 *
 * See libpmemobj(3) for details.
 */

#ifndef	LIBPMEMOBJ_H
#define	LIBPMEMOBJ_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <setjmp.h>
#include <stdint.h>
#include <stddef.h>

/*
 * opaque type internal to libpmemobj
 */
typedef struct pmemobjpool PMEMobjpool;

/*
 * Version checking...
 */

/*
 * PMEMOBJ_MAJOR_VERSION and PMEMOBJ_MINOR_VERSION provide the current version
 * of the libpmemobj API as provided by this header file.  Applications can
 * verify that the version available at run-time is compatible with the version
 * used at compile-time by passing these defines to pmemobj_check_version().
 */
#define	PMEMOBJ_MAJOR_VERSION 1
#define	PMEMOBJ_MINOR_VERSION 0
const char *pmemobj_check_version(
		unsigned major_required,
		unsigned minor_required);

#define	PMEMOBJ_MIN_POOL ((size_t)(1024 * 1024 * 8)) /* 8 MB */
#define	PMEMOBJ_MAX_LAYOUT ((size_t)1024)
#define	PMEMOBJ_NUM_OID_TYPES ((unsigned)1024)

/*
 * Error information...
 */
const char *pmemobj_strerror(void);

/*
 * Pool management...
 */
PMEMobjpool *pmemobj_open(const char *path, const char *layout);
PMEMobjpool *pmemobj_create(const char *path, const char *layout,
	size_t poolsize, mode_t mode);
PMEMobjpool *pmemobj_create_part(const char *path, const char *layout,
	size_t partsize, mode_t mode, int part_index, int nparts,
	int replica_index, int nreplica);
void pmemobj_close(PMEMobjpool *pop);
int pmemobj_check(const char *path, const char *layout);

/*
 * Passing NULL to pmemobj_set_funcs() tells libpmemobj to continue to use the
 * default for that function.  The replacement functions must not make calls
 * back into libpmemobj.
 */
void pmemobj_set_funcs(
		void *(*malloc_func)(size_t size),
		void (*free_func)(void *ptr));

/*
 * Locking...
 */
#define	_POBJ_CL_ALIGNMENT 64 /* cache line alignment for performance */

typedef union padded_pmemmutex {
	char padding[_POBJ_CL_ALIGNMENT];
	struct {
		uint64_t runid;
		pthread_mutex_t mutex;
	} pmemmutex;
} PMEMmutex;

typedef union padded_pmemrwlock {
	char padding[_POBJ_CL_ALIGNMENT];
	struct {
		uint64_t runid;
		pthread_rwlock_t rwlock;
	} pmemrwlock;
} PMEMrwlock;

typedef union padded_pmemcond {
	char padding[_POBJ_CL_ALIGNMENT];
	struct {
		uint64_t runid;
		pthread_cond_t cond;
	} pmemcond;
} PMEMcond;

void pmemobj_mutex_zero(PMEMobjpool *pop, PMEMmutex *mutexp);
int pmemobj_mutex_lock(PMEMobjpool *pop, PMEMmutex *mutexp);
int pmemobj_mutex_trylock(PMEMobjpool *pop, PMEMmutex *mutexp);
int pmemobj_mutex_unlock(PMEMobjpool *pop, PMEMmutex *mutexp);

void pmemobj_rwlock_zero(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_rdlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_wrlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_timedrdlock(PMEMobjpool *pop,
	PMEMrwlock *restrict rwlockp,
	const struct timespec *restrict abs_timeout);
int pmemobj_rwlock_timedwrlock(PMEMobjpool *pop,
	PMEMrwlock *restrict rwlockp,
	const struct timespec *restrict abs_timeout);
int pmemobj_rwlock_tryrdlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_trywrlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_unlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);

void pmemobj_cond_zero(PMEMobjpool *pop, PMEMcond *condp);
int pmemobj_cond_broadcast(PMEMobjpool *pop, PMEMcond *condp);
int pmemobj_cond_signal(PMEMobjpool *pop, PMEMcond *condp);
int pmemobj_cond_timedwait(PMEMobjpool *pop, PMEMcond *restrict condp,
	PMEMmutex *restrict mutexp, const struct timespec *restrict abstime);
int pmemobj_cond_wait(PMEMobjpool *pop, PMEMcond *condp,
	PMEMmutex *restrict mutexp);

/*
 * Persistent memory object
 */

/*
 * Object handle
 */
typedef struct pmemoid {
	uint64_t pool_uuid_lo;
	uint64_t off;
} PMEMoid;

#define	OID_TYPE(type)\
union {\
	type *_type;\
	PMEMoid oid;\
}

#define	OID_ASSIGN(o, value) ((o).oid = value)

#define	OID_ASSIGN_TYPED(lhs, rhs)\
__builtin_choose_expr(\
	__builtin_types_compatible_p(\
		typeof((lhs)._type),\
		typeof((rhs)._type)),\
	(void) ((lhs).oid = (rhs).oid),\
	(lhs._type = rhs._type))

#define	OID_NULL		((PMEMoid) {0, 0})

#define	OID_IS_NULL(o)	((o).oid.off == 0)

#define	OID_EQUALS(lhs, rhs)\
((lhs).oid.off == (rhs).oid.off &&\
	(lhs).oid.pool_uuid_lo == (rhs).oid.pool_uuid_lo)

/*
 * Returns the direct pointer of an object.
 */
void *pmemobj_direct(PMEMoid oid);

#define	DIRECT_RW(o) ((typeof (*(o)._type)*)pmemobj_direct((o).oid))
#define	DIRECT_RO(o) ((const typeof (*(o)._type)*)pmemobj_direct((o).oid))

#define	D_RW	DIRECT_RW
#define	D_RO	DIRECT_RO

/*
 * Non-transactional atomic allocations
 *
 * Those functions can be used outside transactions. The allocations are always
 * aligned to the cache-line boundary.
 */

/*
 * Allocates a new object from the pool.
 */
PMEMoid pmemobj_alloc(PMEMobjpool *pop, size_t size, int type_num);

/*
 * Allocates a new zeroed object from the pool.
 */
PMEMoid pmemobj_zalloc(PMEMobjpool *pop, size_t size, int type_num);

/*
 * Allocates a new object from the pool and calls a constructor function before
 * returning. It is guaranteed that allocated object is either properly
 * initialized, or if it's interrupted before the constructor completes, the
 * memory reserved for the object is automatically reclaimed.
 */
PMEMoid pmemobj_alloc_construct(PMEMobjpool *pop, size_t size, int type_num,
	void (*constructor)(void *ptr, void *arg), void *arg);

/*
 * Resizes an existing object.
 */
PMEMoid pmemobj_realloc(PMEMobjpool *pop, PMEMoid oid, size_t size,
	int type_num);

/*
 * Resizes an existing object, if extended new space is zeroed.
 */
PMEMoid pmemobj_zrealloc(PMEMobjpool *pop, PMEMoid oid, size_t size,
	int type_num);

/*
 * Allocates a new object with duplicate of the string s.
 */
PMEMoid pmemobj_strdup(PMEMobjpool *pop, const char *s, int type_num);

/*
 * Frees an existing object.
 */
void pmemobj_free(PMEMoid oid);

/*
 * Returns the number of usable bytes in the object. May be greater than
 * the requested size of the object because of internal alignment.
 *
 * Can be used with objects allocated by any of the available methods.
 */
size_t pmemobj_alloc_usable_size(PMEMoid oid);

/*
 * If called for the first time on a newly created pool, the root object
 * of given size is allocated.  Otherwise, it returns the existing root object.
 * In such case, the size must be not less than the actual root object size
 * stored in the pool.  If it's larger, the root object is automatically
 * resized.
 *
 * This function is thread-safe.
 */
PMEMoid pmemobj_root(PMEMobjpool *pop, size_t size);

/*
 * Returns the size in bytes of the root object. Always equal to the requested
 * size.
 */
size_t pmemobj_root_size(PMEMobjpool *pop);

/*
 * The following set of macros and functions allow access to the entire
 * collection of objects, or objects of given type.
 *
 * Use with conjunction with non-transactional allocations. Pmemobj pool acts
 * as a generic container (list) of objects that are not assigned to any
 * user-defined data structures.
 */

/*
 * Returns the first object of the specified type number.
 */
PMEMoid pmemobj_first(PMEMobjpool *pop, int type_num);

/*
 * Returns the next object (in order of allocations) of the same type.
 */
PMEMoid pmemobj_next(PMEMoid oid);

/*
 * Debug helper function and macros
 */
#ifdef	DEBUG

/*
 * (debug helper function) logs notice message if used inside a transaction
 */
void _pobj_debug_notice(const char *func_name, const char *file, int line);

/*
 * (debug helper macro) logs notice message if used inside a transaction
 */
#define	_POBJ_DEBUG_NOTICE_IN_TX()\
	_pobj_debug_notice(__func__, NULL, 0)

/*
 * (debug helper macro) logs notice message if used inside a transaction
 *                      - to be used only in FOREACH macros
 */
#define	_POBJ_DEBUG_NOTICE_IN_TX_FOR(macro_name)\
	_pobj_debug_notice(macro_name, __FILE__, __LINE__),

#else
#define	_POBJ_DEBUG_NOTICE_IN_TX() do {} while (0)
#define	_POBJ_DEBUG_NOTICE_IN_TX_FOR(macro_name)
#endif /* DEBUG */

/*
 * Iterates through every existing allocated object.
 */
#define	POBJ_FOREACH(pop, varoid, vartype_num)\
for (_POBJ_DEBUG_NOTICE_IN_TX_FOR("POBJ_FOREACH")\
	vartype_num = 0; vartype_num < PMEMOBJ_NUM_OID_TYPES; ++vartype_num)\
	for (varoid = pmemobj_first(pop, vartype_num);\
		(varoid).off != 0; varoid = pmemobj_next(varoid))

/*
 * Safe variant of POBJ_FOREACH in which pmemobj_free on varoid is allowed
 */
#define	POBJ_FOREACH_SAFE(pop, varoid, nvaroid, vartype_num)\
for (_POBJ_DEBUG_NOTICE_IN_TX_FOR("POBJ_FOREACH_SAFE")\
	vartype_num = 0; vartype_num < PMEMOBJ_NUM_OID_TYPES; ++vartype_num)\
	for (varoid = pmemobj_first(pop, vartype_num);\
		(varoid).off != 0 && (nvaroid = pmemobj_next(varoid), 1);\
		varoid = nvaroid)

/*
 * Iterates through every object of the specified type number.
 */
#define	POBJ_FOREACH_TYPE(pop, var, type_num)\
for (_POBJ_DEBUG_NOTICE_IN_TX_FOR("POBJ_FOREACH_TYPE")\
	OID_ASSIGN(var, pmemobj_first(pop, type_num));\
	OID_IS_NULL(var) == 0;\
	OID_ASSIGN(var, pmemobj_next((var).oid)))

/*
 * Safe variant of POBJ_FOREACH_TYPE in which pmemobj_free on var is allowed
 */
#define	POBJ_FOREACH_SAFE_TYPE(pop, var, nvar, type_num)\
for (_POBJ_DEBUG_NOTICE_IN_TX_FOR("POBJ_FOREACH_SAFE_TYPE")\
	OID_ASSIGN(var, pmemobj_first(pop, type_num));\
	OID_IS_NULL(var) == 0 &&\
	(OID_ASSIGN(nvar, pmemobj_next((var).oid)), 1);\
	OID_ASSIGN_TYPED(var, nvar))

/*
 * Non-transactional persistent atomic circular doubly-linked list
 */
#define	PLIST_ENTRY(type)\
struct {\
	OID_TYPE(type) pe_next;\
	OID_TYPE(type) pe_prev;\
}

#define	PLIST_HEAD(name, type)\
struct name {\
	OID_TYPE(type) pe_first;\
	PMEMmutex lock;\
}

int pmemobj_list_insert(PMEMobjpool *pop, size_t pe_offset, void *head,
	PMEMoid dest, int before, PMEMoid oid);

PMEMoid pmemobj_list_insert_new(PMEMobjpool *pop, size_t pe_offset, void *head,
	PMEMoid dest, int before, size_t size, int type_num);

int pmemobj_list_remove(PMEMobjpool *pop, size_t pe_offset, void *head,
	PMEMoid oid, int free);

int pmemobj_list_move(PMEMobjpool *pop, size_t pe_old_offset,
	void *head_old, size_t pe_new_offset, void *head_new,
	PMEMoid dest, int before, PMEMoid oid);

#define	POBJ_LIST_FIRST(head)	((head)->pe_first)
#define	POBJ_LIST_LAST(head, field)	(D_RO((head)->pe_first)->field.pe_prev)
#define	POBJ_LIST_EMPTY(head)	(OID_IS_NULL((head)->pe_first))
#define	POBJ_LIST_NEXT(elm, field)	(D_RO(elm)->field.pe_next)
#define	POBJ_LIST_PREV(elm, field)	(D_RO(elm)->field.pe_prev)

#define	POBJ_LIST_FOREACH(var, head, field)\
for (_POBJ_DEBUG_NOTICE_IN_TX_FOR("POBJ_LIST_FOREACH")\
	OID_ASSIGN_TYPED((var), POBJ_LIST_FIRST((head)));\
	OID_IS_NULL((var)) == 0;\
	OID_EQUALS(POBJ_LIST_NEXT((var), field),\
	POBJ_LIST_FIRST((head))) ?\
	OID_ASSIGN((var), OID_NULL) :\
	OID_ASSIGN_TYPED((var), POBJ_LIST_NEXT((var), field)))

#define	POBJ_LIST_FOREACH_REVERSE(var, head, field)\
for (_POBJ_DEBUG_NOTICE_IN_TX_FOR("POBJ_LIST_FOREACH_REVERSE")\
	OID_ASSIGN_TYPED((var), POBJ_LIST_LAST((head), field));\
	OID_IS_NULL((var)) == 0;\
	OID_EQUALS(POBJ_LIST_PREV((var), field),\
	POBJ_LIST_LAST((head), field)) ?\
	OID_ASSIGN((var), OID_NULL) :\
	OID_ASSIGN_TYPED((var), POBJ_LIST_PREV((var), field)))

#define	POBJ_LIST_INSERT_HEAD(pop, head, elm, field)\
pmemobj_list_insert((pop),\
	offsetof(typeof (*(POBJ_LIST_FIRST(head)._type)), field),\
	(head), POBJ_LIST_FIRST((head)).oid,\
	1 /* before */, (elm).oid)

#define	POBJ_LIST_INSERT_TAIL(pop, head, elm, field)\
pmemobj_list_insert((pop),\
	offsetof(typeof (*(POBJ_LIST_FIRST(head)._type)), field),\
	(head), POBJ_LIST_LAST((head), field).oid,\
	0 /* after */, (elm).oid)

#define	POBJ_LIST_INSERT_AFTER(pop, head, listelm, elm, field)\
pmemobj_list_insert((pop),\
	offsetof(typeof (*(POBJ_LIST_FIRST(head)._type)), field),\
	(head), (listelm).oid,\
	0 /* after */, (elm).oid)

#define	POBJ_LIST_INSERT_BEFORE(pop, head, listelm, elm, field)\
pmemobj_list_insert((pop), offsetof(typeof (*D_RO((elm))),\
	offsetof(typeof (*(POBJ_LIST_FIRST(head)._type)), field),\
	(head), (listelm).oid,\
	1 /* before */, (elm).oid)

#define	POBJ_LIST_INSERT_NEW_HEAD(pop, head, type_num, field)\
pmemobj_list_insert_new((pop),\
	offsetof(typeof (*((head)->pe_first._type)), field),\
	(head), POBJ_LIST_FIRST((head)).oid,\
	1 /* before */,	sizeof (*(POBJ_LIST_FIRST(head)._type)), type_num)

#define	POBJ_LIST_INSERT_NEW_TAIL(pop, head, type_num, field)\
pmemobj_list_insert_new((pop),\
	offsetof(typeof (*((head)->pe_first._type)), field),\
	(head), POBJ_LIST_LAST((head), field).oid,\
	0 /* after */, sizeof (*(POBJ_LIST_FIRST(head)._type)), type_num)

#define	POBJ_LIST_INSERT_NEW_AFTER(pop, head, listelm, type_num, field)\
pmemobj_list_insert_new((pop),\
	offsetof(typeof (*((head)->pe_first._type)), field),\
	(head), (listelm).oid, 0 /* after */,\
	sizeof (*(POBJ_LIST_FIRST(head)._type)), type_num)

#define	POBJ_LIST_INSERT_NEW_BEFORE(pop, head, listelm, type_num, field)\
pmemobj_list_insert_new((pop),\
	offsetof(typeof (*(POBJ_LIST_FIRST(head)._type)), field),\
	(head), (listelm).oid, 1 /* before */,\
	sizeof (*(POBJ_LIST_FIRST(head)._type)), type_num)

#define	POBJ_LIST_REMOVE(pop, head, elm, field)\
pmemobj_list_remove((pop),\
	offsetof(typeof (*(POBJ_LIST_FIRST(head)._type)), field),\
	(head), (elm).oid, 0 /* no free */)

#define	POBJ_LIST_REMOVE_FREE(pop, head, elm, field)\
pmemobj_list_remove((pop),\
	offsetof(typeof (*(POBJ_LIST_FIRST(head)._type)), field),\
	(head), (elm).oid, 1 /* free */)

#define	POBJ_LIST_MOVE_ELEMENT_HEAD(pop, head, head_new, elm, field, field_new)\
pmemobj_list_move((pop),\
	offsetof(typeof (*(POBJ_LIST_FIRST(head)._type)), field),\
	(head),\
	offsetof(typeof (*(POBJ_LIST_FIRST(head_new)._type)), field_new),\
	(head_new),\
	POBJ_LIST_FIRST((head_new)).oid,\
	1 /* before */, (elm).oid)

#define	POBJ_LIST_MOVE_ELEMENT_TAIL(pop, head, head_new, elm, field, field_new)\
pmemobj_list_move((pop),\
	offsetof(typeof (*(POBJ_LIST_FIRST(head)._type)), field),\
	(head),\
	offsetof(typeof (*(POBJ_LIST_FIRST(head_new)._type)), field_new),\
	(head_new),\
	POBJ_LIST_LAST((head_new)).oid,\
	0 /* after */, (elm).oid)

#define	POBJ_LIST_MOVE_ELEMENT_AFTER(pop,\
	head, head_new, listelm, elm, field, field_new)\
pmemobj_list_move((pop),\
	offsetof(typeof (*(POBJ_LIST_FIRST(head)._type)), field),\
	(head),\
	offsetof(typeof (*(POBJ_LIST_FIRST(head_new)._type)), field_new),\
	(head_new),\
	(listelm).oid,\
	0 /* after */, (elm).oid)

#define	POBJ_LIST_MOVE_ELEMENT_BEFORE(pop,\
	head, head_new, listelm, elm, field, field_new)\
pmemobj_list_move((pop),\
	offsetof(typeof (*(POBJ_LIST_FIRST(head)._type)), field),\
	(head),\
	offsetof(typeof (*(POBJ_LIST_FIRST(head_new)._type)), field_new),\
	(head_new),\
	(listelm).oid,\
	1 /* before */, (elm).oid)

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
enum pobj_tx_stage pmemobj_tx_stage();

enum pobj_tx_lock {
	TX_LOCK_NONE,
	TX_LOCK_MUTEX,	/* PMEMmutex */
	TX_LOCK_RWLOCK	/* PMEMrwlock */
};

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
 * Aborts current transaction
 *
 * Must be called during TX_STAGE_WORK. Otherwise, has no effect.
 *
 * Always causes transition to TX_STAGE_ONABORT.
 */
void pmemobj_tx_abort(int errnum);

/*
 * Commits current transaction
 *
 * If successful and called during TX_STAGE_WORK, transaction stage changes
 * to TX_STAGE_ONCOMMIT and function returns zero. Otherwise, stage changes
 * to TX_STAGE_ONABORT and an error number is returned.
 */
int pmemobj_tx_commit();

/*
 * Cleanups current transaction. Must always be called after pmemobj_tx_begin,
 * even if starting the transaction failed.
 *
 * If called during TX_STAGE_NONE, has no effect.
 *
 * Always causes transition to TX_STAGE_NONE.
 */
void pmemobj_tx_end();

/*
 * Performs the actions associated with current stage of the transaction,
 * and makes the transition to the next stage. Current stage must always
 * be obtained by calling pmemobj_tx_get_stage.
 *
 * If successful, function returns zero. Otherwise, an error number is returned.
 */
int pmemobj_tx_process();

#define	_POBJ_TX_BEGIN(pop, ...)\
{\
	jmp_buf _tx_env;\
	int _stage = TX_STAGE_NONE;\
	setjmp(_tx_env);\
	if (_stage == TX_STAGE_NONE)\
		pmemobj_tx_begin(pop, _tx_env, __VA_ARGS__, TX_LOCK_NONE);\
	while ((_stage = pmemobj_tx_stage()) != TX_STAGE_NONE) {\
		switch (_stage) {\
			case TX_STAGE_WORK:

#define	TX_BEGIN_LOCK(pop, ...)\
_POBJ_TX_BEGIN(pop, ##__VA_ARGS__)

#define	TX_BEGIN(pop) _POBJ_TX_BEGIN(pop, TX_LOCK_NONE)

#define	TX_ONABORT\
				pmemobj_tx_process();\
				break;\
			case TX_STAGE_ONABORT:

#define	TX_ONCOMMIT\
				pmemobj_tx_process();\
				break;\
			case TX_STAGE_ONCOMMIT:

#define	TX_FINALLY\
				pmemobj_tx_process();\
				break;\
			case TX_STAGE_FINALLY:

#define	TX_END\
				pmemobj_tx_process();\
				break;\
			default:\
				pmemobj_tx_process();\
				break;\
		}\
	}\
	pmemobj_tx_end();\
}

/*
 * Takes a "snapshot" of the memory block of given size and located at given
 * offset in the object, and saves it in the undo log.
 * The application is then free to directly modify the object in that memory
 * range. In case of failure or abort, all the changes within this range will
 * be rolled-back automatically.
 *
 * If successful and called during TX_STAGE_WORK, function returns zero.
 * Otherwise, state changes to TX_STAGE_ONABORT and an error number is returned.
 */
int pmemobj_tx_add_range(PMEMoid oid, uint64_t off, size_t size);

/*
 * Transactionally allocates a new object.
 *
 * If successful and called during TX_STAGE_WORK, function returns zero.
 * Otherwise, state changes to TX_STAGE_ONABORT and an error number is returned.
 */
PMEMoid pmemobj_tx_alloc(size_t size, int type_num);

/*
 * Transactionally allocates new zeroed object.
 *
 * If successful and called during TX_STAGE_WORK, function returns zero.
 * Otherwise, state changes to TX_STAGE_ONABORT and an error number is returned.
 */
PMEMoid pmemobj_tx_zalloc(size_t size, int type_num);

/*
 * Transactionally resizes an existing object.
 *
 * If successful and called during TX_STAGE_WORK, function returns zero.
 * Otherwise, state changes to TX_STAGE_ONABORT and an error number is returned.
 */
PMEMoid pmemobj_tx_realloc(PMEMoid oid, size_t size, int type_num);

/*
 * Transactionally resizes an existing object, if extended new space is zeroed.
 *
 * If successful and called during TX_STAGE_WORK, function returns zero.
 * Otherwise, state changes to TX_STAGE_ONABORT and an error number is returned.
 */
PMEMoid pmemobj_tx_zrealloc(PMEMoid oid, size_t size, int type_num);

/*
 * Transactionally allocates a new object with duplicate of the string s.
 *
 * If successful and called during TX_STAGE_WORK, function returns zero.
 * Otherwise, state changes to TX_STAGE_ONABORT and an error number is returned.
 */
PMEMoid pmemobj_tx_strdup(const char *s, int type_num);

/*
 * Transactionally frees an existing object.
 *
 * If successful and called during TX_STAGE_WORK, function returns zero.
 * Otherwise, state changes to TX_STAGE_ONABORT and an error number is returned.
 */
int pmemobj_tx_free(PMEMoid oid);

#define	TX_ADD(o)\
pmemobj_tx_add_range((o).oid, 0, sizeof (*(o)._type));

#define	TX_ADD_FIELD(o, field)\
pmemobj_tx_add_range((o).oid, offsetof(typeof(*(o)._type), field),\
		sizeof (D_RO(o)->field));\

/*
 * TX_ALLOC (et al.) must be called inside the OID_ASSIGN macro.
 */
#define	TX_ALLOC(type, type_num)\
pmemobj_tx_alloc(sizeof (type), type_num)

#define	TX_ZALLOC(type, type_num)\
pmemobj_tx_zalloc(sizeof (type), type_num)

#define	TX_REALLOC(o, size, type_num)\
pmemobj_tx_realloc((o).oid, size, type_num)

#define	TX_ZREALLOC(o, size, type_num)\
pmemobj_tx_zrealloc((o).oid, size, type_num)

#define	TX_STRDUP(s, type_num)\
pmemobj_tx_strdup(s, type_num)

#define	TX_FREE(o)\
pmemobj_tx_free((o).oid)

#define	TX_SET(o, field, value) (\
{\
	TX_ADD_FIELD(o, field);\
	D_RW(o)->field = value; })

#define	TX_MEMCPY(o, destf, src, num) (\
{\
	TX_ADD_FIELD(o, destf);\
	pmem_memcpy_persist((void *)&D_RO(o)->destf, src, num); })

#define	TX_MEMSET(o, destf, c, num) (\
{\
	TX_ADD_FIELD(o, destf);\
	pmem_memset_persist((void *)&D_RO(o)->destf, c, num); })

#ifdef __cplusplus
}
#endif
#endif	/* libpmemobj.h */
