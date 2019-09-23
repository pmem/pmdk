/*
 * Copyright 2015-2019, Intel Corporation
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
 * tx.c -- transactions implementation
 */

#include <inttypes.h>
#include <wchar.h>

#include "queue.h"
#include "ravl.h"
#include "obj.h"
#include "out.h"
#include "pmalloc.h"
#include "tx.h"
#include "valgrind_internal.h"
#include "memops.h"

struct tx_data {
	PMDK_SLIST_ENTRY(tx_data) tx_entry;
	jmp_buf env;
};

struct tx {
	PMEMobjpool *pop;
	enum pobj_tx_stage stage;
	int last_errnum;
	struct lane *lane;
	PMDK_SLIST_HEAD(txl, tx_lock_data) tx_locks;
	PMDK_SLIST_HEAD(txd, tx_data) tx_entries;

	struct ravl *ranges;

	VEC(, struct pobj_action) actions;

	pmemobj_tx_callback stage_callback;
	void *stage_callback_arg;

	int first_snapshot;
};

/*
 * get_tx -- (internal) returns current transaction
 *
 * This function should be used only in high-level functions.
 */
static struct tx *
get_tx()
{
	static __thread struct tx tx;
	return &tx;
}

struct tx_lock_data {
	union {
		PMEMmutex *mutex;
		PMEMrwlock *rwlock;
	} lock;
	enum pobj_tx_param lock_type;
	PMDK_SLIST_ENTRY(tx_lock_data) tx_lock;
};

struct tx_alloc_args {
	uint64_t flags;
	const void *copy_ptr;
	size_t copy_size;
};

#define COPY_ARGS(flags, copy_ptr, copy_size)\
(struct tx_alloc_args){flags, copy_ptr, copy_size}

#define ALLOC_ARGS(flags)\
(struct tx_alloc_args){flags, NULL, 0}

struct tx_range_def {
	uint64_t offset;
	uint64_t size;
	uint64_t flags;
};

/*
 * tx_range_def_cmp -- compares two snapshot ranges
 */
static int
tx_range_def_cmp(const void *lhs, const void *rhs)
{
	const struct tx_range_def *l = lhs;
	const struct tx_range_def *r = rhs;

	if (l->offset > r->offset)
		return 1;
	else if (l->offset < r->offset)
		return -1;

	return 0;
}

/*
 * tx_params_new -- creates a new transactional parameters instance and fills it
 *	with default values.
 */
struct tx_parameters *
tx_params_new(void)
{
	struct tx_parameters *tx_params = Malloc(sizeof(*tx_params));
	if (tx_params == NULL)
		return NULL;

	tx_params->cache_size = TX_DEFAULT_RANGE_CACHE_SIZE;

	return tx_params;
}

/*
 * tx_params_delete -- deletes transactional parameters instance
 */
void
tx_params_delete(struct tx_parameters *tx_params)
{
	Free(tx_params);
}

static void
obj_tx_abort(int errnum, int user);

/*
 * obj_tx_abort_err -- (internal) pmemobj_tx_abort variant that returns
 * error code
 */
static inline int
obj_tx_abort_err(int errnum)
{
	obj_tx_abort(errnum, 0);
	return errnum;
}

/*
 * obj_tx_abort_null -- (internal) pmemobj_tx_abort variant that returns
 * null PMEMoid
 */
static inline PMEMoid
obj_tx_abort_null(int errnum)
{
	obj_tx_abort(errnum, 0);
	return OID_NULL;
}

/* ASSERT_IN_TX -- checks whether there's open transaction */
#define ASSERT_IN_TX(tx) do {\
	if ((tx)->stage == TX_STAGE_NONE)\
		FATAL("%s called outside of transaction", __func__);\
} while (0)

/* ASSERT_TX_STAGE_WORK -- checks whether current transaction stage is WORK */
#define ASSERT_TX_STAGE_WORK(tx) do {\
	if ((tx)->stage != TX_STAGE_WORK)\
		FATAL("%s called in invalid stage %d", __func__, (tx)->stage);\
} while (0)

/*
 * tx_action_add -- (internal) reserve space and add a new tx action
 */
static struct pobj_action *
tx_action_add(struct tx *tx)
{
	size_t entries_size = (VEC_SIZE(&tx->actions) + 1) *
		sizeof(struct ulog_entry_val);
	if (operation_reserve(tx->lane->external, entries_size) != 0)
		return NULL;

	VEC_INC_BACK(&tx->actions);

	return &VEC_BACK(&tx->actions);
}

/*
 * tx_action_remove -- (internal) remove last tx action
 */
static void
tx_action_remove(struct tx *tx)
{
	VEC_POP_BACK(&tx->actions);
}

/*
 * constructor_tx_alloc -- (internal) constructor for normal alloc
 */
static int
constructor_tx_alloc(void *ctx, void *ptr, size_t usable_size, void *arg)
{
	LOG(5, NULL);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct tx_alloc_args *args = arg;

	/* do not report changes to the new object */
	VALGRIND_ADD_TO_TX(ptr, usable_size);

	if (args->flags & POBJ_FLAG_ZERO)
		memset(ptr, 0, usable_size);

	if (args->copy_ptr && args->copy_size != 0) {
		memcpy(ptr, args->copy_ptr, args->copy_size);
	}

	return 0;
}


struct tx_range_data {
	void *begin;
	void *end;
	PMDK_SLIST_ENTRY(tx_range_data) tx_range;
};

PMDK_SLIST_HEAD(txr, tx_range_data);

/*
 * tx_remove_range -- (internal) removes specified range from ranges list
 */
static void
tx_remove_range(struct txr *tx_ranges, void *begin, void *end)
{
	struct tx_range_data *txr = PMDK_SLIST_FIRST(tx_ranges);

	while (txr) {
		if (begin >= txr->end || end < txr->begin) {
			txr = PMDK_SLIST_NEXT(txr, tx_range);
			continue;
		}

		LOG(4, "detected PMEM lock in undo log; "
			"range %p-%p, lock %p-%p",
			txr->begin, txr->end, begin, end);

		/* split the range into new ones */
		if (begin > txr->begin) {
			struct tx_range_data *txrn = Malloc(sizeof(*txrn));
			if (txrn == NULL)
				FATAL("!Malloc");

			txrn->begin = txr->begin;
			txrn->end = begin;
			LOG(4, "range split; %p-%p", txrn->begin, txrn->end);
			PMDK_SLIST_INSERT_HEAD(tx_ranges, txrn, tx_range);
		}

		if (end < txr->end) {
			struct tx_range_data *txrn = Malloc(sizeof(*txrn));
			if (txrn == NULL)
				FATAL("!Malloc");

			txrn->begin = end;
			txrn->end = txr->end;
			LOG(4, "range split; %p-%p", txrn->begin, txrn->end);
			PMDK_SLIST_INSERT_HEAD(tx_ranges, txrn, tx_range);
		}

		struct tx_range_data *next = PMDK_SLIST_NEXT(txr, tx_range);
		/* remove the original range from the list */
		PMDK_SLIST_REMOVE(tx_ranges, txr, tx_range_data, tx_range);
		Free(txr);

		txr = next;
	}
}
/*
 * tx_restore_range -- (internal) restore a single range from undo log
 *
 * If the snapshot contains any PMEM locks that are held by the current
 * transaction, they won't be overwritten with the saved data to avoid changing
 * their state.  Those locks will be released in tx_end().
 */
static void
tx_restore_range(PMEMobjpool *pop, struct tx *tx, struct ulog_entry_buf *range)
{
	COMPILE_ERROR_ON(sizeof(PMEMmutex) != _POBJ_CL_SIZE);
	COMPILE_ERROR_ON(sizeof(PMEMrwlock) != _POBJ_CL_SIZE);
	COMPILE_ERROR_ON(sizeof(PMEMcond) != _POBJ_CL_SIZE);

	struct txr tx_ranges;
	PMDK_SLIST_INIT(&tx_ranges);

	struct tx_range_data *txr;
	txr = Malloc(sizeof(*txr));
	if (txr == NULL) {
		FATAL("!Malloc");
	}

	uint64_t range_offset = ulog_entry_offset(&range->base);

	txr->begin = OBJ_OFF_TO_PTR(pop, range_offset);
	txr->end = (char *)txr->begin + range->size;
	PMDK_SLIST_INSERT_HEAD(&tx_ranges, txr, tx_range);

	struct tx_lock_data *txl;

	/* check if there are any locks within given memory range */
	PMDK_SLIST_FOREACH(txl, &tx->tx_locks, tx_lock) {
		void *lock_begin = txl->lock.mutex;
		/* all PMEM locks have the same size */
		void *lock_end = (char *)lock_begin + _POBJ_CL_SIZE;

		tx_remove_range(&tx_ranges, lock_begin, lock_end);
	}

	ASSERT(!PMDK_SLIST_EMPTY(&tx_ranges));

	void *dst_ptr = OBJ_OFF_TO_PTR(pop, range_offset);

	while (!PMDK_SLIST_EMPTY(&tx_ranges)) {
		txr = PMDK_SLIST_FIRST(&tx_ranges);
		PMDK_SLIST_REMOVE_HEAD(&tx_ranges, tx_range);
		/* restore partial range data from snapshot */
		ASSERT((char *)txr->begin >= (char *)dst_ptr);
		uint8_t *src = &range->data[
				(char *)txr->begin - (char *)dst_ptr];
		ASSERT((char *)txr->end >= (char *)txr->begin);
		size_t size = (size_t)((char *)txr->end - (char *)txr->begin);
		pmemops_memcpy(&pop->p_ops, txr->begin, src, size, 0);
		Free(txr);
	}
}

/*
 * tx_undo_entry_apply -- applies modifications of a single ulog entry
 */
static int
tx_undo_entry_apply(struct ulog_entry_base *e, void *arg,
	const struct pmem_ops *p_ops)
{
	struct ulog_entry_buf *eb;

	switch (ulog_entry_type(e)) {
		case ULOG_OPERATION_BUF_CPY:
			eb = (struct ulog_entry_buf *)e;

			tx_restore_range(p_ops->base, get_tx(), eb);
		break;
		case ULOG_OPERATION_AND:
		case ULOG_OPERATION_OR:
		case ULOG_OPERATION_SET:
		case ULOG_OPERATION_BUF_SET:
		default:
			ASSERT(0);
	}

	return 0;
}

/*
 * tx_abort_set -- (internal) abort all set operations
 */
static void
tx_abort_set(PMEMobjpool *pop, struct lane *lane)
{
	LOG(7, NULL);

	ulog_foreach_entry((struct ulog *)&lane->layout->undo,
		tx_undo_entry_apply, NULL, &pop->p_ops);
	operation_finish(lane->undo);
}

/*
 * tx_flush_range -- (internal) flush one range
 */
static void
tx_flush_range(void *data, void *ctx)
{
	PMEMobjpool *pop = ctx;
	struct tx_range_def *range = data;
	if (!(range->flags & POBJ_FLAG_NO_FLUSH)) {
		pmemops_xflush(&pop->p_ops, OBJ_OFF_TO_PTR(pop, range->offset),
				range->size, PMEMOBJ_F_RELAXED);
	}
	VALGRIND_REMOVE_FROM_TX(OBJ_OFF_TO_PTR(pop, range->offset),
		range->size);
}

/*
 * tx_clean_range -- (internal) clean one range
 */
static void
tx_clean_range(void *data, void *ctx)
{
	PMEMobjpool *pop = ctx;
	struct tx_range_def *range = data;
	VALGRIND_REMOVE_FROM_TX(OBJ_OFF_TO_PTR(pop, range->offset),
		range->size);
	VALGRIND_SET_CLEAN(OBJ_OFF_TO_PTR(pop, range->offset), range->size);
}

/*
 * tx_pre_commit -- (internal) do pre-commit operations
 */
static void
tx_pre_commit(struct tx *tx)
{
	LOG(5, NULL);

	/* Flush all regions and destroy the whole tree. */
	ravl_delete_cb(tx->ranges, tx_flush_range, tx->pop);
	tx->ranges = NULL;
}


/*
 * tx_abort -- (internal) abort all allocated objects
 */
static void
tx_abort(PMEMobjpool *pop, struct lane *lane)
{
	LOG(7, NULL);

	struct tx *tx = get_tx();

	tx_abort_set(pop, lane);

	ravl_delete_cb(tx->ranges, tx_clean_range, pop);
	palloc_cancel(&pop->heap,
		VEC_ARR(&tx->actions), VEC_SIZE(&tx->actions));
	VEC_CLEAR(&tx->actions);
	tx->ranges = NULL;
}

/*
 * tx_get_pop -- returns the current transaction's pool handle, NULL if not
 * within a transaction.
 */
PMEMobjpool *
tx_get_pop(void)
{
	return get_tx()->pop;
}

/*
 * add_to_tx_and_lock -- (internal) add lock to the transaction and acquire it
 */
static int
add_to_tx_and_lock(struct tx *tx, enum pobj_tx_param type, void *lock)
{
	LOG(15, NULL);

	int retval = 0;
	struct tx_lock_data *txl;
	/* check if the lock is already on the list */
	PMDK_SLIST_FOREACH(txl, &tx->tx_locks, tx_lock) {
		if (memcmp(&txl->lock, &lock, sizeof(lock)) == 0)
			return 0;
	}

	txl = Malloc(sizeof(*txl));
	if (txl == NULL)
		return ENOMEM;

	txl->lock_type = type;
	switch (txl->lock_type) {
		case TX_PARAM_MUTEX:
			txl->lock.mutex = lock;
			retval = pmemobj_mutex_lock(tx->pop,
				txl->lock.mutex);
			if (retval) {
				ERR("!pmemobj_mutex_lock");
				goto err;
			}
			break;
		case TX_PARAM_RWLOCK:
			txl->lock.rwlock = lock;
			retval = pmemobj_rwlock_wrlock(tx->pop,
				txl->lock.rwlock);
			if (retval) {
				ERR("!pmemobj_rwlock_wrlock");
				goto err;
			}
			break;
		default:
			ERR("Unrecognized lock type");
			ASSERT(0);
			break;
	}

	PMDK_SLIST_INSERT_HEAD(&tx->tx_locks, txl, tx_lock);
	return 0;

err:
	errno = retval;
	Free(txl);

	return retval;
}

/*
 * release_and_free_tx_locks -- (internal) release and remove all locks from the
 *				transaction
 */
static void
release_and_free_tx_locks(struct tx *tx)
{
	LOG(15, NULL);

	while (!PMDK_SLIST_EMPTY(&tx->tx_locks)) {
		struct tx_lock_data *tx_lock = PMDK_SLIST_FIRST(&tx->tx_locks);
		PMDK_SLIST_REMOVE_HEAD(&tx->tx_locks, tx_lock);
		switch (tx_lock->lock_type) {
			case TX_PARAM_MUTEX:
				pmemobj_mutex_unlock(tx->pop,
					tx_lock->lock.mutex);
				break;
			case TX_PARAM_RWLOCK:
				pmemobj_rwlock_unlock(tx->pop,
					tx_lock->lock.rwlock);
				break;
			default:
				ERR("Unrecognized lock type");
				ASSERT(0);
				break;
		}
		Free(tx_lock);
	}
}

/*
 * tx_lane_ranges_insert_def -- (internal) allocates and inserts a new range
 *	definition into the ranges tree
 */
static int
tx_lane_ranges_insert_def(PMEMobjpool *pop, struct tx *tx,
	const struct tx_range_def *rdef)
{
	LOG(3, "rdef->offset %"PRIu64" rdef->size %"PRIu64,
		rdef->offset, rdef->size);

	int ret = ravl_emplace_copy(tx->ranges, rdef);
	if (ret == EEXIST)
		FATAL("invalid state of ranges tree");

	return ret;
}

/*
 * tx_alloc_common -- (internal) common function for alloc and zalloc
 */
static PMEMoid
tx_alloc_common(struct tx *tx, size_t size, type_num_t type_num,
		palloc_constr constructor, struct tx_alloc_args args)
{
	LOG(3, NULL);

	if (size > PMEMOBJ_MAX_ALLOC_SIZE) {
		ERR("requested size too large");
		return obj_tx_abort_null(ENOMEM);
	}

	PMEMobjpool *pop = tx->pop;

	struct pobj_action *action = tx_action_add(tx);
	if (action == NULL)
		return obj_tx_abort_null(ENOMEM);

	if (palloc_reserve(&pop->heap, size, constructor, &args, type_num, 0,
		CLASS_ID_FROM_FLAG(args.flags),
		ARENA_ID_FROM_FLAG(args.flags), action) != 0)
		goto err_oom;

	/* allocate object to undo log */
	PMEMoid retoid = OID_NULL;
	retoid.off = action->heap.offset;
	retoid.pool_uuid_lo = pop->uuid_lo;
	size = palloc_usable_size(&pop->heap, retoid.off);

	const struct tx_range_def r = {retoid.off, size, args.flags};
	if (tx_lane_ranges_insert_def(pop, tx, &r) != 0)
		goto err_oom;

	return retoid;

err_oom:
	tx_action_remove(tx);
	ERR("out of memory");
	return obj_tx_abort_null(ENOMEM);
}

/*
 * tx_realloc_common -- (internal) common function for tx realloc
 */
static PMEMoid
tx_realloc_common(struct tx *tx, PMEMoid oid, size_t size, uint64_t type_num,
	palloc_constr constructor_alloc,
	palloc_constr constructor_realloc,
	uint64_t flags)
{
	LOG(3, NULL);

	if (size > PMEMOBJ_MAX_ALLOC_SIZE) {
		ERR("requested size too large");
		return obj_tx_abort_null(ENOMEM);
	}

	/* if oid is NULL just alloc */
	if (OBJ_OID_IS_NULL(oid))
		return tx_alloc_common(tx, size, (type_num_t)type_num,
				constructor_alloc, ALLOC_ARGS(flags));

	ASSERT(OBJ_OID_IS_VALID(tx->pop, oid));

	/* if size is 0 just free */
	if (size == 0) {
		if (pmemobj_tx_free(oid)) {
			ERR("pmemobj_tx_free failed");
			return oid;
		} else {
			return OID_NULL;
		}
	}

	/* oid is not NULL and size is not 0 so do realloc by alloc and free */
	void *ptr = OBJ_OFF_TO_PTR(tx->pop, oid.off);
	size_t old_size = palloc_usable_size(&tx->pop->heap, oid.off);

	size_t copy_size = old_size < size ? old_size : size;

	PMEMoid new_obj = tx_alloc_common(tx, size, (type_num_t)type_num,
			constructor_realloc, COPY_ARGS(flags, ptr, copy_size));

	if (!OBJ_OID_IS_NULL(new_obj)) {
		if (pmemobj_tx_free(oid)) {
			ERR("pmemobj_tx_free failed");
			VEC_POP_BACK(&tx->actions);
			return OID_NULL;
		}
	}

	return new_obj;
}

/*
 * pmemobj_tx_begin -- initializes new transaction
 */
int
pmemobj_tx_begin(PMEMobjpool *pop, jmp_buf env, ...)
{
	LOG(3, NULL);

	int err = 0;
	struct tx *tx = get_tx();

	if (tx->stage == TX_STAGE_WORK) {
		ASSERTne(tx->lane, NULL);
		if (tx->pop != pop) {
			ERR("nested transaction for different pool");
			return obj_tx_abort_err(EINVAL);
		}

		VALGRIND_START_TX;
	} else if (tx->stage == TX_STAGE_NONE) {
		VALGRIND_START_TX;

		lane_hold(pop, &tx->lane);
		operation_start(tx->lane->undo);

		VEC_INIT(&tx->actions);
		PMDK_SLIST_INIT(&tx->tx_entries);
		PMDK_SLIST_INIT(&tx->tx_locks);

		tx->ranges = ravl_new_sized(tx_range_def_cmp,
			sizeof(struct tx_range_def));

		tx->pop = pop;

		tx->first_snapshot = 1;
	} else {
		FATAL("Invalid stage %d to begin new transaction", tx->stage);
	}

	struct tx_data *txd = Malloc(sizeof(*txd));
	if (txd == NULL) {
		err = errno;
		ERR("!Malloc");
		goto err_abort;
	}

	tx->last_errnum = 0;
	if (env != NULL)
		memcpy(txd->env, env, sizeof(jmp_buf));
	else
		memset(txd->env, 0, sizeof(jmp_buf));

	PMDK_SLIST_INSERT_HEAD(&tx->tx_entries, txd, tx_entry);

	tx->stage = TX_STAGE_WORK;

	/* handle locks */
	va_list argp;
	va_start(argp, env);
	enum pobj_tx_param param_type;

	while ((param_type = va_arg(argp, enum pobj_tx_param)) !=
			TX_PARAM_NONE) {
		if (param_type == TX_PARAM_CB) {
			pmemobj_tx_callback cb =
					va_arg(argp, pmemobj_tx_callback);
			void *arg = va_arg(argp, void *);

			if (tx->stage_callback &&
					(tx->stage_callback != cb ||
					tx->stage_callback_arg != arg)) {
				FATAL("transaction callback is already set, "
					"old %p new %p old_arg %p new_arg %p",
					tx->stage_callback, cb,
					tx->stage_callback_arg, arg);
			}

			tx->stage_callback = cb;
			tx->stage_callback_arg = arg;
		} else {
			err = add_to_tx_and_lock(tx, param_type,
				va_arg(argp, void *));
			if (err) {
				va_end(argp);
				goto err_abort;
			}
		}
	}
	va_end(argp);

	ASSERT(err == 0);
	return 0;

err_abort:
	if (tx->stage == TX_STAGE_WORK)
		obj_tx_abort(err, 0);
	else
		tx->stage = TX_STAGE_ONABORT;
	return err;
}

/*
 * pmemobj_tx_lock -- get lane from pool and add lock to transaction.
 */
int
pmemobj_tx_lock(enum pobj_tx_param type, void *lockp)
{
	struct tx *tx = get_tx();
	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	return add_to_tx_and_lock(tx, type, lockp);
}

/*
 * obj_tx_callback -- (internal) executes callback associated with current stage
 */
static void
obj_tx_callback(struct tx *tx)
{
	if (!tx->stage_callback)
		return;

	struct tx_data *txd = PMDK_SLIST_FIRST(&tx->tx_entries);

	/* is this the outermost transaction? */
	if (PMDK_SLIST_NEXT(txd, tx_entry) == NULL)
		tx->stage_callback(tx->pop, tx->stage, tx->stage_callback_arg);
}

/*
 * pmemobj_tx_stage -- returns current transaction stage
 */
enum pobj_tx_stage
pmemobj_tx_stage(void)
{
	LOG(3, NULL);

	return get_tx()->stage;
}

/*
 * obj_tx_abort -- aborts current transaction
 */
static void
obj_tx_abort(int errnum, int user)
{
	LOG(3, NULL);
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	ASSERT(tx->lane != NULL);

	if (errnum == 0)
		errnum = ECANCELED;

	tx->stage = TX_STAGE_ONABORT;
	struct tx_data *txd = PMDK_SLIST_FIRST(&tx->tx_entries);

	if (PMDK_SLIST_NEXT(txd, tx_entry) == NULL) {
		/* this is the outermost transaction */

		/* process the undo log */
		tx_abort(tx->pop, tx->lane);

		lane_release(tx->pop);
		tx->lane = NULL;
	}

	tx->last_errnum = errnum;
	errno = errnum;
	if (user)
		ERR("!explicit transaction abort");

	/* ONABORT */
	obj_tx_callback(tx);

	if (!util_is_zeroed(txd->env, sizeof(jmp_buf)))
		longjmp(txd->env, errnum);
}

/*
 * pmemobj_tx_abort -- aborts current transaction
 *
 * Note: this function should not be called from inside of pmemobj.
 */
void
pmemobj_tx_abort(int errnum)
{
	PMEMOBJ_API_START();
	obj_tx_abort(errnum, 1);
	PMEMOBJ_API_END();
}

/*
 * pmemobj_tx_errno -- returns last transaction error code
 */
int
pmemobj_tx_errno(void)
{
	LOG(3, NULL);

	return get_tx()->last_errnum;
}

static void
tx_post_commit(struct tx *tx)
{
	operation_finish(tx->lane->undo);

	VEC_CLEAR(&tx->actions);
}

/*
 * pmemobj_tx_commit -- commits current transaction
 */
void
pmemobj_tx_commit(void)
{
	LOG(3, NULL);

	PMEMOBJ_API_START();
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	/* WORK */
	obj_tx_callback(tx);

	ASSERT(tx->lane != NULL);

	struct tx_data *txd = PMDK_SLIST_FIRST(&tx->tx_entries);

	if (PMDK_SLIST_NEXT(txd, tx_entry) == NULL) {
		/* this is the outermost transaction */

		PMEMobjpool *pop = tx->pop;

		/* pre-commit phase */
		tx_pre_commit(tx);

		pmemops_drain(&pop->p_ops);

		operation_start(tx->lane->external);
		palloc_publish(&pop->heap, VEC_ARR(&tx->actions),
			VEC_SIZE(&tx->actions), tx->lane->external);

		tx_post_commit(tx);

		lane_release(pop);

		tx->lane = NULL;
	}

	tx->stage = TX_STAGE_ONCOMMIT;

	/* ONCOMMIT */
	obj_tx_callback(tx);
	PMEMOBJ_API_END();
}

/*
 * pmemobj_tx_end -- ends current transaction
 */
int
pmemobj_tx_end(void)
{
	LOG(3, NULL);

	struct tx *tx = get_tx();

	if (tx->stage == TX_STAGE_WORK)
		FATAL("pmemobj_tx_end called without pmemobj_tx_commit");

	if (tx->pop == NULL)
		FATAL("pmemobj_tx_end called without pmemobj_tx_begin");

	if (tx->stage_callback &&
			(tx->stage == TX_STAGE_ONCOMMIT ||
			tx->stage == TX_STAGE_ONABORT)) {
		tx->stage = TX_STAGE_FINALLY;
		obj_tx_callback(tx);
	}

	struct tx_data *txd = PMDK_SLIST_FIRST(&tx->tx_entries);
	PMDK_SLIST_REMOVE_HEAD(&tx->tx_entries, tx_entry);

	Free(txd);

	VALGRIND_END_TX;

	if (PMDK_SLIST_EMPTY(&tx->tx_entries)) {
		ASSERTeq(tx->lane, NULL);

		release_and_free_tx_locks(tx);
		tx->pop = NULL;
		tx->stage = TX_STAGE_NONE;
		VEC_DELETE(&tx->actions);

		if (tx->stage_callback) {
			pmemobj_tx_callback cb = tx->stage_callback;
			void *arg = tx->stage_callback_arg;

			tx->stage_callback = NULL;
			tx->stage_callback_arg = NULL;

			cb(tx->pop, TX_STAGE_NONE, arg);
		}
	} else {
		/* resume the next transaction */
		tx->stage = TX_STAGE_WORK;

		/* abort called within inner transaction, waterfall the error */
		if (tx->last_errnum)
			obj_tx_abort(tx->last_errnum, 0);
	}

	return tx->last_errnum;
}

/*
 * pmemobj_tx_process -- processes current transaction stage
 */
void
pmemobj_tx_process(void)
{
	LOG(5, NULL);
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);

	switch (tx->stage) {
	case TX_STAGE_NONE:
		break;
	case TX_STAGE_WORK:
		pmemobj_tx_commit();
		break;
	case TX_STAGE_ONABORT:
	case TX_STAGE_ONCOMMIT:
		tx->stage = TX_STAGE_FINALLY;
		obj_tx_callback(tx);
		break;
	case TX_STAGE_FINALLY:
		tx->stage = TX_STAGE_NONE;
		break;
	default:
		ASSERT(0);
	}
}

/*
 * vg_verify_initialized -- when executed under Valgrind verifies that
 *   the buffer has been initialized; explicit check at snapshotting time,
 *   because Valgrind may find it much later when it's impossible to tell
 *   for which snapshot it triggered
 */
static void
vg_verify_initialized(PMEMobjpool *pop, const struct tx_range_def *def)
{
#if VG_MEMCHECK_ENABLED
	if (!On_valgrind)
		return;

	VALGRIND_DO_DISABLE_ERROR_REPORTING;
	char *start = (char *)pop + def->offset;
	char *uninit = (char *)VALGRIND_CHECK_MEM_IS_DEFINED(start, def->size);
	if (uninit) {
		VALGRIND_PRINTF(
			"Snapshotting uninitialized data in range <%p,%p> (<offset:0x%lx,size:0x%lx>)\n",
			start, start + def->size, def->offset, def->size);

		if (uninit != start)
			VALGRIND_PRINTF("Uninitialized data starts at: %p\n",
					uninit);

		VALGRIND_DO_ENABLE_ERROR_REPORTING;
		VALGRIND_CHECK_MEM_IS_DEFINED(start, def->size);
	} else {
		VALGRIND_DO_ENABLE_ERROR_REPORTING;
	}
#endif
}

/*
 * pmemobj_tx_add_snapshot -- (internal) creates a variably sized snapshot
 */
static int
pmemobj_tx_add_snapshot(struct tx *tx, struct tx_range_def *snapshot)
{
	vg_verify_initialized(tx->pop, snapshot);

	/*
	 * If we are creating the first snapshot, setup a redo log action to
	 * clear the first entry so that the undo log becomes
	 * invalid once the redo log is processed.
	 */
	if (tx->first_snapshot) {
		struct pobj_action *action = tx_action_add(tx);
		if (action == NULL)
			return -1;

		/* first entry of the first ulog */
		struct ulog_entry_base *e =
			(struct ulog_entry_base *)tx->lane->layout->undo.data;
		palloc_set_value(&tx->pop->heap, action,
			&e->offset, 0);

		tx->first_snapshot = 0;
	}

	/*
	 * Depending on the size of the block, either allocate an
	 * entire new object or use cache.
	 */
	void *ptr = OBJ_OFF_TO_PTR(tx->pop, snapshot->offset);
	VALGRIND_ADD_TO_TX(ptr, snapshot->size);

	return operation_add_buffer(tx->lane->undo, ptr, ptr, snapshot->size,
		ULOG_OPERATION_BUF_CPY);
}

/*
 * pmemobj_tx_merge_flags -- (internal) common code for merging flags between
 * two ranges to ensure resultant behavior is correct
 */
static void
pmemobj_tx_merge_flags(struct tx_range_def *dest, struct tx_range_def *merged)
{
	/*
	 * POBJ_XADD_NO_FLUSH should only be set in merged range if set in
	 * both ranges
	 */
	if ((dest->flags & POBJ_XADD_NO_FLUSH) &&
				!(merged->flags & POBJ_XADD_NO_FLUSH)) {
		dest->flags = dest->flags & (~POBJ_XADD_NO_FLUSH);
	}
}

/*
 * pmemobj_tx_add_common -- (internal) common code for adding persistent memory
 * into the transaction
 */
static int
pmemobj_tx_add_common(struct tx *tx, struct tx_range_def *args)
{
	LOG(15, NULL);

	if (args->size > PMEMOBJ_MAX_ALLOC_SIZE) {
		ERR("snapshot size too large");
		return obj_tx_abort_err(EINVAL);
	}

	if (args->offset < tx->pop->heap_offset ||
		(args->offset + args->size) >
		(tx->pop->heap_offset + tx->pop->heap_size)) {
		ERR("object outside of heap");
		return obj_tx_abort_err(EINVAL);
	}

	int ret = 0;

	/*
	 * Search existing ranges backwards starting from the end of the
	 * snapshot.
	 */
	struct tx_range_def r = *args;
	struct tx_range_def search = {0, 0, 0};
	/*
	 * If the range is directly adjacent to an existing one,
	 * they can be merged, so search for less or equal elements.
	 */
	enum ravl_predicate p = RAVL_PREDICATE_LESS_EQUAL;
	struct ravl_node *nprev = NULL;
	while (r.size != 0) {
		search.offset = r.offset + r.size;
		struct ravl_node *n = ravl_find(tx->ranges, &search, p);
		/*
		 * We have to skip searching for LESS_EQUAL because
		 * the snapshot we would find is the one that was just
		 * created.
		 */
		p = RAVL_PREDICATE_LESS;

		struct tx_range_def *f = n ? ravl_data(n) : NULL;

		size_t fend = f == NULL ? 0: f->offset + f->size;
		size_t rend = r.offset + r.size;
		if (fend == 0 || fend < r.offset) {
			/*
			 * If found no range or the found range is not
			 * overlapping or adjacent on the left side, we can just
			 * create the entire r.offset + r.size snapshot.
			 *
			 * Snapshot:
			 *	--+-
			 * Existing ranges:
			 *	---- (no ranges)
			 * or	+--- (no overlap)
			 * or	---+ (adjacent on on right side)
			 */
			if (nprev != NULL) {
				/*
				 * But, if we have an existing adjacent snapshot
				 * on the right side, we can just extend it to
				 * include the desired range.
				 */
				struct tx_range_def *fprev = ravl_data(nprev);
				ASSERTeq(rend, fprev->offset);
				fprev->offset -= r.size;
				fprev->size += r.size;
			} else {
				/*
				 * If we don't have anything adjacent, create
				 * a new range in the tree.
				 */
				ret = tx_lane_ranges_insert_def(tx->pop,
					tx, &r);
				if (ret != 0)
					break;
			}
			ret = pmemobj_tx_add_snapshot(tx, &r);
			break;
		} else if (fend <= rend) {
			/*
			 * If found range has its end inside of the desired
			 * snapshot range, we can extend the found range by the
			 * size leftover on the left side.
			 *
			 * Snapshot:
			 *	--+++--
			 * Existing ranges:
			 *	+++---- (overlap on left)
			 * or	---+--- (found snapshot is inside)
			 * or	---+-++ (inside, and adjacent on the rigt)
			 * or	+++++-- (desired snapshot is inside)
			 *
			 */
			struct tx_range_def snapshot = *args;
			snapshot.offset = fend;
			/* the side not yet covered by an existing snapshot */
			snapshot.size = rend - fend;

			/* the number of bytes intersecting in both ranges */
			size_t intersection = fend - MAX(f->offset, r.offset);
			r.size -= intersection + snapshot.size;
			f->size += snapshot.size;
			pmemobj_tx_merge_flags(f, args);

			if (snapshot.size != 0) {
				ret = pmemobj_tx_add_snapshot(tx, &snapshot);
				if (ret != 0)
					break;
			}

			/*
			 * If there's a snapshot adjacent on right side, merge
			 * the two ranges together.
			 */
			if (nprev != NULL) {
				struct tx_range_def *fprev = ravl_data(nprev);
				ASSERTeq(rend, fprev->offset);
				f->size += fprev->size;
				pmemobj_tx_merge_flags(f, fprev);
				ravl_remove(tx->ranges, nprev);
			}
		} else if (fend >= r.offset) {
			/*
			 * If found range has its end extending beyond the
			 * desired snapshot.
			 *
			 * Snapshot:
			 *	--+++--
			 * Existing ranges:
			 *	-----++ (adjacent on the right)
			 * or	----++- (overlapping on the right)
			 * or	----+++ (overlapping and adjacent on the right)
			 * or	--+++++ (desired snapshot is inside)
			 *
			 * Notice that we cannot create a snapshot based solely
			 * on this information without risking overwritting an
			 * existing one. We have to continue iterating, but we
			 * keep the information about adjacent snapshots in the
			 * nprev variable.
			 */
			size_t overlap = rend - MAX(f->offset, r.offset);
			r.size -= overlap;
			pmemobj_tx_merge_flags(f, args);
		} else {
			ASSERT(0);
		}

		nprev = n;
	}

	if (ret != 0) {
		ERR("out of memory");
		return obj_tx_abort_err(ENOMEM);
	}

	return 0;
}

/*
 * pmemobj_tx_add_range_direct -- adds persistent memory range into the
 *					transaction
 */
int
pmemobj_tx_add_range_direct(const void *ptr, size_t size)
{
	LOG(3, NULL);

	PMEMOBJ_API_START();
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	PMEMobjpool *pop = tx->pop;

	if (!OBJ_PTR_FROM_POOL(pop, ptr)) {
		ERR("object outside of pool");
		int ret = obj_tx_abort_err(EINVAL);
		PMEMOBJ_API_END();
		return ret;
	}

	struct tx_range_def args = {
		.offset = (uint64_t)((char *)ptr - (char *)pop),
		.size = size,
		.flags = 0,
	};

	int ret = pmemobj_tx_add_common(tx, &args);

	PMEMOBJ_API_END();
	return ret;
}

/*
 * pmemobj_tx_xadd_range_direct -- adds persistent memory range into the
 *					transaction
 */
int
pmemobj_tx_xadd_range_direct(const void *ptr, size_t size, uint64_t flags)
{
	LOG(3, NULL);
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	PMEMOBJ_API_START();
	int ret;
	if (!OBJ_PTR_FROM_POOL(tx->pop, ptr)) {
		ERR("object outside of pool");
		ret = obj_tx_abort_err(EINVAL);
		PMEMOBJ_API_END();
		return ret;
	}

	if (flags & ~POBJ_XADD_VALID_FLAGS) {
		ERR("unknown flags 0x%" PRIx64, flags & ~POBJ_XADD_VALID_FLAGS);
		ret = obj_tx_abort_err(EINVAL);
		PMEMOBJ_API_END();
		return ret;
	}

	struct tx_range_def args = {
		.offset = (uint64_t)((char *)ptr - (char *)tx->pop),
		.size = size,
		.flags = flags,
	};

	ret = pmemobj_tx_add_common(tx, &args);

	PMEMOBJ_API_END();
	return ret;
}

/*
 * pmemobj_tx_add_range -- adds persistent memory range into the transaction
 */
int
pmemobj_tx_add_range(PMEMoid oid, uint64_t hoff, size_t size)
{
	LOG(3, NULL);

	PMEMOBJ_API_START();
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	if (oid.pool_uuid_lo != tx->pop->uuid_lo) {
		ERR("invalid pool uuid");
		int ret = obj_tx_abort_err(EINVAL);
		PMEMOBJ_API_END();
		return ret;
	}
	ASSERT(OBJ_OID_IS_VALID(tx->pop, oid));

	struct tx_range_def args = {
		.offset = oid.off + hoff,
		.size = size,
		.flags = 0,
	};

	int ret = pmemobj_tx_add_common(tx, &args);

	PMEMOBJ_API_END();
	return ret;
}

/*
 * pmemobj_tx_xadd_range -- adds persistent memory range into the transaction
 */
int
pmemobj_tx_xadd_range(PMEMoid oid, uint64_t hoff, size_t size, uint64_t flags)
{
	LOG(3, NULL);

	PMEMOBJ_API_START();
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	int ret;
	if (oid.pool_uuid_lo != tx->pop->uuid_lo) {
		ERR("invalid pool uuid");
		ret = obj_tx_abort_err(EINVAL);
		PMEMOBJ_API_END();
		return ret;
	}
	ASSERT(OBJ_OID_IS_VALID(tx->pop, oid));

	if (flags & ~POBJ_XADD_VALID_FLAGS) {
		ERR("unknown flags 0x%" PRIx64, flags & ~POBJ_XADD_VALID_FLAGS);
		ret = obj_tx_abort_err(EINVAL);
		PMEMOBJ_API_END();
		return ret;
	}

	struct tx_range_def args = {
		.offset = oid.off + hoff,
		.size = size,
		.flags = flags,
	};

	ret = pmemobj_tx_add_common(tx, &args);
	PMEMOBJ_API_END();
	return ret;
}

/*
 * pmemobj_tx_alloc -- allocates a new object
 */
PMEMoid
pmemobj_tx_alloc(size_t size, uint64_t type_num)
{
	LOG(3, NULL);

	PMEMOBJ_API_START();
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	PMEMoid oid;
	if (size == 0) {
		ERR("allocation with size 0");
		oid = obj_tx_abort_null(EINVAL);
		PMEMOBJ_API_END();
		return oid;
	}

	oid = tx_alloc_common(tx, size, (type_num_t)type_num,
			constructor_tx_alloc, ALLOC_ARGS(0));

	PMEMOBJ_API_END();
	return oid;
}

/*
 * pmemobj_tx_zalloc -- allocates a new zeroed object
 */
PMEMoid
pmemobj_tx_zalloc(size_t size, uint64_t type_num)
{
	LOG(3, NULL);
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	PMEMOBJ_API_START();
	PMEMoid oid;
	if (size == 0) {
		ERR("allocation with size 0");
		oid = obj_tx_abort_null(EINVAL);
		PMEMOBJ_API_END();
		return oid;
	}

	oid = tx_alloc_common(tx, size, (type_num_t)type_num,
			constructor_tx_alloc, ALLOC_ARGS(POBJ_FLAG_ZERO));

	PMEMOBJ_API_END();
	return oid;
}

/*
 * pmemobj_tx_xalloc -- allocates a new object
 */
PMEMoid
pmemobj_tx_xalloc(size_t size, uint64_t type_num, uint64_t flags)
{
	LOG(3, NULL);
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);
	PMEMOBJ_API_START();

	PMEMoid oid;
	if (size == 0) {
		ERR("allocation with size 0");
		oid = obj_tx_abort_null(EINVAL);
		PMEMOBJ_API_END();
		return oid;
	}

	if (flags & ~POBJ_TX_XALLOC_VALID_FLAGS) {
		ERR("unknown flags 0x%" PRIx64,
				flags & ~POBJ_TX_XALLOC_VALID_FLAGS);
		oid = obj_tx_abort_null(EINVAL);
		PMEMOBJ_API_END();
		return oid;
	}

	oid = tx_alloc_common(tx, size, (type_num_t)type_num,
			constructor_tx_alloc, ALLOC_ARGS(flags));

	PMEMOBJ_API_END();
	return oid;
}

/*
 * pmemobj_tx_realloc -- resizes an existing object
 */
PMEMoid
pmemobj_tx_realloc(PMEMoid oid, size_t size, uint64_t type_num)
{
	LOG(3, NULL);
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	PMEMOBJ_API_START();
	PMEMoid ret = tx_realloc_common(tx, oid, size, type_num,
			constructor_tx_alloc, constructor_tx_alloc, 0);
	PMEMOBJ_API_END();
	return ret;
}


/*
 * pmemobj_zrealloc -- resizes an existing object, any new space is zeroed.
 */
PMEMoid
pmemobj_tx_zrealloc(PMEMoid oid, size_t size, uint64_t type_num)
{
	LOG(3, NULL);
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	PMEMOBJ_API_START();
	PMEMoid ret = tx_realloc_common(tx, oid, size, type_num,
			constructor_tx_alloc, constructor_tx_alloc,
			POBJ_FLAG_ZERO);
	PMEMOBJ_API_END();
	return ret;
}

/*
 * pmemobj_tx_strdup -- allocates a new object with duplicate of the string s.
 */
PMEMoid
pmemobj_tx_strdup(const char *s, uint64_t type_num)
{
	LOG(3, NULL);
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	PMEMOBJ_API_START();
	PMEMoid oid;
	if (NULL == s) {
		ERR("cannot duplicate NULL string");
		oid = obj_tx_abort_null(EINVAL);
		PMEMOBJ_API_END();
		return oid;
	}

	size_t len = strlen(s);

	if (len == 0) {
		oid = tx_alloc_common(tx, sizeof(char), (type_num_t)type_num,
				constructor_tx_alloc,
				ALLOC_ARGS(POBJ_FLAG_ZERO));
		PMEMOBJ_API_END();
		return oid;
	}

	size_t size = (len + 1) * sizeof(char);

	oid = tx_alloc_common(tx, size, (type_num_t)type_num,
			constructor_tx_alloc, COPY_ARGS(0, s, size));

	PMEMOBJ_API_END();
	return oid;
}

/*
 * pmemobj_tx_wcsdup -- allocates a new object with duplicate of the wide
 * character string s.
 */
PMEMoid
pmemobj_tx_wcsdup(const wchar_t *s, uint64_t type_num)
{
	LOG(3, NULL);
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	PMEMOBJ_API_END();
	PMEMoid oid;
	if (NULL == s) {
		ERR("cannot duplicate NULL string");
		oid = obj_tx_abort_null(EINVAL);
		PMEMOBJ_API_END();
		return oid;
	}

	size_t len = wcslen(s);

	if (len == 0) {
		oid = tx_alloc_common(tx, sizeof(wchar_t),
				(type_num_t)type_num, constructor_tx_alloc,
				ALLOC_ARGS(POBJ_FLAG_ZERO));
		PMEMOBJ_API_END();
		return oid;
	}

	size_t size = (len + 1) * sizeof(wchar_t);

	oid = tx_alloc_common(tx, size, (type_num_t)type_num,
			constructor_tx_alloc, COPY_ARGS(0, s, size));

	PMEMOBJ_API_END();
	return oid;
}

/*
 * pmemobj_tx_free -- frees an existing object
 */
int
pmemobj_tx_free(PMEMoid oid)
{
	LOG(3, NULL);
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	if (OBJ_OID_IS_NULL(oid))
		return 0;


	PMEMobjpool *pop = tx->pop;

	if (pop->uuid_lo != oid.pool_uuid_lo) {
		ERR("invalid pool uuid");
		return obj_tx_abort_err(EINVAL);
	}
	ASSERT(OBJ_OID_IS_VALID(pop, oid));

	PMEMOBJ_API_START();

	struct pobj_action *action;

	struct tx_range_def range = {oid.off, 0, 0};
	struct ravl_node *n = ravl_find(tx->ranges, &range,
		RAVL_PREDICATE_EQUAL);

	/*
	 * If attempting to free an object allocated within the same
	 * transaction, simply cancel the alloc and remove it from the actions.
	 */
	if (n != NULL) {
		VEC_FOREACH_BY_PTR(action, &tx->actions) {
			if (action->type == POBJ_ACTION_TYPE_HEAP &&
				action->heap.offset == oid.off) {
				struct tx_range_def *r = ravl_data(n);
				void *ptr = OBJ_OFF_TO_PTR(pop, r->offset);
				VALGRIND_SET_CLEAN(ptr, r->size);
				VALGRIND_REMOVE_FROM_TX(ptr, r->size);
				ravl_remove(tx->ranges, n);
				palloc_cancel(&pop->heap, action, 1);
				VEC_ERASE_BY_PTR(&tx->actions, action);
				PMEMOBJ_API_END();
				return 0;
			}
		}
	}

	action = tx_action_add(tx);
	if (action == NULL) {
		int ret = obj_tx_abort_err(errno);
		PMEMOBJ_API_END();
		return ret;
	}

	palloc_defer_free(&pop->heap, oid.off, action);

	PMEMOBJ_API_END();
	return 0;
}

/*
 * pmemobj_tx_publish -- publishes actions inside of a transaction
 */
int
pmemobj_tx_publish(struct pobj_action *actv, size_t actvcnt)
{
	struct tx *tx = get_tx();
	ASSERT_TX_STAGE_WORK(tx);
	PMEMOBJ_API_START();

	size_t entries_size = (VEC_SIZE(&tx->actions) + actvcnt) *
		sizeof(struct ulog_entry_val);

	if (operation_reserve(tx->lane->external, entries_size) != 0) {
		int ret = obj_tx_abort_err(ENOMEM);
		PMEMOBJ_API_END();
		return ret;
	}

	for (size_t i = 0; i < actvcnt; ++i) {
		VEC_PUSH_BACK(&tx->actions, actv[i]);
	}

	PMEMOBJ_API_END();
	return 0;
}

/*
 * CTL_READ_HANDLER(size) -- gets the cache size transaction parameter
 */
static int
CTL_READ_HANDLER(size)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;

	ssize_t *arg_out = arg;

	*arg_out = (ssize_t)pop->tx_params->cache_size;

	return 0;
}

/*
 * CTL_WRITE_HANDLER(size) -- sets the cache size transaction parameter
 */
static int
CTL_WRITE_HANDLER(size)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;

	ssize_t arg_in = *(int *)arg;

	if (arg_in < 0 || arg_in > (ssize_t)PMEMOBJ_MAX_ALLOC_SIZE) {
		errno = EINVAL;
		ERR("invalid cache size, must be between 0 and max alloc size");
		return -1;
	}

	size_t argu = (size_t)arg_in;

	pop->tx_params->cache_size = argu;

	return 0;
}

static const struct ctl_argument CTL_ARG(size) = CTL_ARG_LONG_LONG;

/*
 * CTL_READ_HANDLER(threshold) -- gets the cache threshold transaction parameter
 */
static int
CTL_READ_HANDLER(threshold)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	LOG(1, "tx.cache.threshold parameter is depracated");

	return 0;
}

/*
 * CTL_WRITE_HANDLER(threshold) -- depracated
 */
static int
CTL_WRITE_HANDLER(threshold)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	LOG(1, "tx.cache.threshold parameter is depracated");

	return 0;
}

static const struct ctl_argument CTL_ARG(threshold) = CTL_ARG_LONG_LONG;

static const struct ctl_node CTL_NODE(cache)[] = {
	CTL_LEAF_RW(size),
	CTL_LEAF_RW(threshold),

	CTL_NODE_END
};

/*
 * CTL_READ_HANDLER(skip_expensive_checks) -- returns "skip_expensive_checks"
 * var from pool ctl
 */
static int
CTL_READ_HANDLER(skip_expensive_checks)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;

	int *arg_out = arg;

	*arg_out = pop->tx_debug_skip_expensive_checks;

	return 0;
}

/*
 * CTL_WRITE_HANDLER(skip_expensive_checks) -- stores "skip_expensive_checks"
 * var in pool ctl
 */
static int
CTL_WRITE_HANDLER(skip_expensive_checks)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	PMEMobjpool *pop = ctx;

	int arg_in = *(int *)arg;

	pop->tx_debug_skip_expensive_checks = arg_in;
	return 0;
}

static const struct ctl_argument CTL_ARG(skip_expensive_checks) =
		CTL_ARG_BOOLEAN;

static const struct ctl_node CTL_NODE(debug)[] = {
	CTL_LEAF_RW(skip_expensive_checks),

	CTL_NODE_END
};

/*
 * CTL_WRITE_HANDLER(queue_depth) -- returns the depth of the post commit queue
 */
static int
CTL_READ_HANDLER(queue_depth)(void *ctx, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	return 0;
}

/*
 * CTL_WRITE_HANDLER(queue_depth) -- sets the depth of the post commit queue
 */
static int
CTL_WRITE_HANDLER(queue_depth)(void *ctx, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	return 0;
}

static const struct ctl_argument CTL_ARG(queue_depth) = CTL_ARG_INT;

/*
 * CTL_READ_HANDLER(worker) -- launches the post commit worker thread function
 */
static int
CTL_READ_HANDLER(worker)(void *ctx, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	return 0;
}

/*
 * CTL_READ_HANDLER(stop) -- stops all post commit workers
 */
static int
CTL_READ_HANDLER(stop)(void *ctx, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	return 0;
}

static const struct ctl_node CTL_NODE(post_commit)[] = {
	CTL_LEAF_RW(queue_depth),
	CTL_LEAF_RO(worker),
	CTL_LEAF_RO(stop),

	CTL_NODE_END
};

static const struct ctl_node CTL_NODE(tx)[] = {
	CTL_CHILD(debug),
	CTL_CHILD(cache),
	CTL_CHILD(post_commit),

	CTL_NODE_END
};

/*
 * tx_ctl_register -- registers ctl nodes for "tx" module
 */
void
tx_ctl_register(PMEMobjpool *pop)
{
	CTL_REGISTER_MODULE(pop->ctl, tx);
}
