/*
 * Copyright 2015-2018, Intel Corporation
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

struct tx_data {
	SLIST_ENTRY(tx_data) tx_entry;
	jmp_buf env;
};

struct tx {
	PMEMobjpool *pop;
	enum pobj_tx_stage stage;
	int last_errnum;
	struct lane_section *section;
	SLIST_HEAD(txl, tx_lock_data) tx_locks;
	SLIST_HEAD(txd, tx_data) tx_entries;

	pmemobj_tx_callback stage_callback;
	void *stage_callback_arg;
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
	SLIST_ENTRY(tx_lock_data) tx_lock;
};

struct tx_undo_runtime {
	struct pvector_context *ctx[MAX_UNDO_TYPES];
};

#define MAX_MEMOPS_ENTRIES_PER_TX_ALLOC 2
#define MAX_TX_ALLOC_RESERVATIONS (MAX_MEMOPS_ENTRIES /\
	MAX_MEMOPS_ENTRIES_PER_TX_ALLOC)

struct lane_tx_runtime {
	unsigned lane_idx;
	struct ravl *ranges;
	uint64_t cache_offset;
	struct tx_undo_runtime undo;
	struct pobj_action alloc_actv[MAX_TX_ALLOC_RESERVATIONS];
	int actvcnt; /* reservation count */
	int actvundo; /* reservations in undo log (to skip) */
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
 * tx_clr_flag -- flags for clearing undo log list
 */
enum tx_clr_flag {
	/* remove and free each object */
	TX_CLR_FLAG_FREE = 1 << 0,

	/* clear valgrind state */
	TX_CLR_FLAG_VG_CLEAN = 1 << 1,

	/* remove from valgrind tx */
	TX_CLR_FLAG_VG_TX_REMOVE = 1 << 2,

	/*
	 * Conditionally remove and free each object, this is only safe in a
	 * single threaded context such as transaction recovery.
	 */
	TX_CLR_FLAG_FREE_IF_EXISTS = 1 << 3,
};

struct tx_parameters {
	size_t cache_size;
	size_t cache_threshold;
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
	tx_params->cache_threshold = TX_DEFAULT_RANGE_CACHE_THRESHOLD;

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
	if (tx->stage == TX_STAGE_NONE)\
		FATAL("%s called outside of transaction", __func__);\
} while (0)

/* ASSERT_TX_STAGE_WORK -- checks whether current transaction stage is WORK */
#define ASSERT_TX_STAGE_WORK(tx) do {\
	if (tx->stage != TX_STAGE_WORK)\
		FATAL("%s called in invalid stage %d", __func__, tx->stage);\
} while (0)

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

/*
 * constructor_tx_add_range -- (internal) constructor for add_range
 */
static int
constructor_tx_add_range(void *ctx, void *ptr, size_t usable_size, void *arg)
{
	LOG(5, NULL);
	PMEMobjpool *pop = ctx;

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct tx_range_def *args = arg;
	struct tx_range *range = ptr;
	const struct pmem_ops *p_ops = &pop->p_ops;

	/* temporarily add the object copy to the transaction */
	VALGRIND_ADD_TO_TX(range, sizeof(struct tx_range) + args->size);

	range->offset = args->offset;
	range->size = args->size;

	void *src = OBJ_OFF_TO_PTR(pop, args->offset);

	/* flush offset and size */
	pmemops_flush(p_ops, range, sizeof(struct tx_range));
	/* memcpy data and persist */
	pmemops_memcpy_persist(p_ops, range->data, src, args->size);

	VALGRIND_REMOVE_FROM_TX(range, sizeof(struct tx_range) + args->size);

	/* do not report changes to the original object */
	VALGRIND_ADD_TO_TX(src, args->size);

	return 0;
}

/*
 * tx_set_state -- (internal) set transaction state
 */
static inline void
tx_set_state(PMEMobjpool *pop, struct lane_tx_layout *layout, uint64_t state)
{
	layout->state = state;
	pmemops_persist(&pop->p_ops, &layout->state, sizeof(layout->state));
}

/*
 * tx_clear_vec_entry -- (internal) clear undo log vector entry
 */
static void
tx_clear_vec_entry(PMEMobjpool *pop, uint64_t *entry)
{
	VALGRIND_ADD_TO_TX(entry, sizeof(*entry));
	*entry = 0;
	pmemops_persist(&pop->p_ops, entry, sizeof(*entry));
	VALGRIND_REMOVE_FROM_TX(entry, sizeof(*entry));
}

/*
 * tx_free_vec_entry -- free the undo log vector entry
 */
static void
tx_free_vec_entry(PMEMobjpool *pop, uint64_t *entry)
{
	pfree(pop, entry);
}

/*
 * tx_free_existing_vec_entry -- free the undo log vector entry if it points to
 *	a valid object, otherwise zero it.
 */
static void
tx_free_existing_vec_entry(PMEMobjpool *pop, uint64_t *entry)
{
	if (palloc_is_allocated(&pop->heap, *entry))
		pfree(pop, entry);
	else
		tx_clear_vec_entry(pop, entry);
}

/*
 * tx_clear_undo_log_vg -- (internal) tell Valgrind about removal from undo log
 */
static void
tx_clear_undo_log_vg(PMEMobjpool *pop, uint64_t off, enum tx_clr_flag flags)
{
#if VG_PMEMCHECK_ENABLED
	if (!On_valgrind)
		return;

	/*
	 * Clean the valgrind state of the underlying memory for
	 * allocated objects in the undo log, so that not-persisted
	 * modifications after abort are not reported.
	 */
	if (flags & TX_CLR_FLAG_VG_CLEAN) {
		void *ptr = OBJ_OFF_TO_PTR(pop, off);
		size_t size = palloc_usable_size(&pop->heap, off);

		VALGRIND_SET_CLEAN(ptr, size);
	}

	if (flags & TX_CLR_FLAG_VG_TX_REMOVE) {
		/*
		 * This function can be called from transaction
		 * recovery, so in such case pmemobj_alloc_usable_size
		 * is not yet available. Use pmalloc version.
		 */
		size_t size = palloc_usable_size(&pop->heap, off);
		VALGRIND_REMOVE_FROM_TX(OBJ_OFF_TO_PTR(pop, off), size);
	}
#endif
}

/*
 * tx_clear_undo_log -- (internal) clear undo log pointed by head
 */
static void
tx_clear_undo_log(PMEMobjpool *pop, struct pvector_context *undo, int nskip,
	enum tx_clr_flag flags)
{
	LOG(7, NULL);

	uint64_t val;

	while ((val = pvector_last(undo)) != 0) {
		tx_clear_undo_log_vg(pop, val, flags);

		if (nskip > 0) {
			nskip--;
			pvector_pop_back(undo, tx_clear_vec_entry);
			continue;
		}

		if (flags & TX_CLR_FLAG_FREE) {
			pvector_pop_back(undo, tx_free_vec_entry);
		} else if (flags & TX_CLR_FLAG_FREE_IF_EXISTS) {
			pvector_pop_back(undo, tx_free_existing_vec_entry);
		} else {
			pvector_pop_back(undo, tx_clear_vec_entry);
		}
	}
}

/*
 * tx_abort_alloc -- (internal) abort all allocated objects
 */
static void
tx_abort_alloc(PMEMobjpool *pop, struct tx_undo_runtime *tx_rt,
	struct lane_tx_runtime *lane)
{
	LOG(5, NULL);

	/*
	 * If not in recovery, the active reservations present in the undo log
	 * need to be removed from the undo log without deallocating.
	 */
	enum tx_clr_flag flags = TX_CLR_FLAG_VG_TX_REMOVE |
		TX_CLR_FLAG_VG_CLEAN |
		(lane ? TX_CLR_FLAG_FREE : TX_CLR_FLAG_FREE_IF_EXISTS);

	tx_clear_undo_log(pop, tx_rt->ctx[UNDO_ALLOC],
		lane ? lane->actvundo : 0,
		flags);
}

/*
 * tx_abort_free -- (internal) abort all freeing objects
 */
static void
tx_abort_free(PMEMobjpool *pop, struct tx_undo_runtime *tx_rt)
{
	LOG(5, NULL);

	tx_clear_undo_log(pop, tx_rt->ctx[UNDO_FREE], 0, 0);
}

struct tx_range_data {
	void *begin;
	void *end;
	SLIST_ENTRY(tx_range_data) tx_range;
};

SLIST_HEAD(txr, tx_range_data);

/*
 * tx_remove_range -- (internal) removes specified range from ranges list
 */
static void
tx_remove_range(struct txr *tx_ranges, void *begin, void *end)
{
	struct tx_range_data *txr = SLIST_FIRST(tx_ranges);

	while (txr) {
		if (begin >= txr->end || end < txr->begin) {
			txr = SLIST_NEXT(txr, tx_range);
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
			SLIST_INSERT_HEAD(tx_ranges, txrn, tx_range);
		}

		if (end < txr->end) {
			struct tx_range_data *txrn = Malloc(sizeof(*txrn));
			if (txrn == NULL)
				FATAL("!Malloc");

			txrn->begin = end;
			txrn->end = txr->end;
			LOG(4, "range split; %p-%p", txrn->begin, txrn->end);
			SLIST_INSERT_HEAD(tx_ranges, txrn, tx_range);
		}

		struct tx_range_data *next = SLIST_NEXT(txr, tx_range);
		/* remove the original range from the list */
		SLIST_REMOVE(tx_ranges, txr, tx_range_data, tx_range);
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
tx_restore_range(PMEMobjpool *pop, struct tx *tx, struct tx_range *range)
{
	COMPILE_ERROR_ON(sizeof(PMEMmutex) != _POBJ_CL_SIZE);
	COMPILE_ERROR_ON(sizeof(PMEMrwlock) != _POBJ_CL_SIZE);
	COMPILE_ERROR_ON(sizeof(PMEMcond) != _POBJ_CL_SIZE);

	struct lane_tx_runtime *runtime =
			(struct lane_tx_runtime *)tx->section->runtime;
	ASSERTne(runtime, NULL);

	struct txr tx_ranges;
	SLIST_INIT(&tx_ranges);

	struct tx_range_data *txr;
	txr = Malloc(sizeof(*txr));
	if (txr == NULL) {
		FATAL("!Malloc");
	}

	txr->begin = OBJ_OFF_TO_PTR(pop, range->offset);
	txr->end = (char *)txr->begin + range->size;
	SLIST_INSERT_HEAD(&tx_ranges, txr, tx_range);

	struct tx_lock_data *txl;

	/* check if there are any locks within given memory range */
	SLIST_FOREACH(txl, &tx->tx_locks, tx_lock) {
		void *lock_begin = txl->lock.mutex;
		/* all PMEM locks have the same size */
		void *lock_end = (char *)lock_begin + _POBJ_CL_SIZE;

		tx_remove_range(&tx_ranges, lock_begin, lock_end);
	}

	ASSERT(!SLIST_EMPTY(&tx_ranges));

	void *dst_ptr = OBJ_OFF_TO_PTR(pop, range->offset);

	while (!SLIST_EMPTY(&tx_ranges)) {
		txr = SLIST_FIRST(&tx_ranges);
		SLIST_REMOVE_HEAD(&tx_ranges, tx_range);
		/* restore partial range data from snapshot */
		ASSERT((char *)txr->begin >= (char *)dst_ptr);
		uint8_t *src = &range->data[
				(char *)txr->begin - (char *)dst_ptr];
		ASSERT((char *)txr->end >= (char *)txr->begin);
		size_t size = (size_t)((char *)txr->end - (char *)txr->begin);
		pmemops_memcpy_persist(&pop->p_ops, txr->begin, src, size);
		Free(txr);
	}
}

/*
 * tx_foreach_set -- (internal) iterates over every memory range
 */
static void
tx_foreach_set(PMEMobjpool *pop, struct tx *tx, struct tx_undo_runtime *tx_rt,
	void (*cb)(PMEMobjpool *pop, struct tx *tx, struct tx_range *range))
{
	LOG(7, NULL);

	struct tx_range *range = NULL;
	uint64_t off;
	struct pvector_context *ctx = tx_rt->ctx[UNDO_SET];
	for (off = pvector_first(ctx); off != 0; off = pvector_next(ctx)) {
		range = OBJ_OFF_TO_PTR(pop, off);
		cb(pop, tx, range);
	}

	struct tx_range_cache *cache;
	uint64_t cache_size;
	ctx = tx_rt->ctx[UNDO_SET_CACHE];
	for (off = pvector_first(ctx); off != 0; off = pvector_next(ctx)) {
		cache = OBJ_OFF_TO_PTR(pop, off);
		cache_size = palloc_usable_size(&pop->heap, off);

		for (uint64_t cache_offset = 0; cache_offset < cache_size; ) {
			range = (struct tx_range *)
				((char *)cache + cache_offset);
			if (range->offset == 0 || range->size == 0)
				break;

			cb(pop, tx, range);

			size_t amask = pop->conversion_flags &
				CONVERSION_FLAG_OLD_SET_CACHE ?
				TX_RANGE_MASK_LEGACY : TX_RANGE_MASK;
			cache_offset += TX_ALIGN_SIZE(range->size, amask) +
				sizeof(struct tx_range);
		}
	}
}

/*
 * tx_abort_restore_range -- (internal) restores content of the memory range
 */
static void
tx_abort_restore_range(PMEMobjpool *pop, struct tx *tx, struct tx_range *range)
{
	tx_restore_range(pop, tx, range);
	VALGRIND_REMOVE_FROM_TX(OBJ_OFF_TO_PTR(pop, range->offset),
			range->size);
}

/*
 * tx_abort_recover_range -- (internal) restores content while skipping locks
 */
static void
tx_abort_recover_range(PMEMobjpool *pop, struct tx *tx, struct tx_range *range)
{
	ASSERTeq(tx, NULL);
	void *ptr = OBJ_OFF_TO_PTR(pop, range->offset);
	pmemops_memcpy_persist(&pop->p_ops, ptr, range->data, range->size);
}

/*
 * tx_clear_set_cache_but_first -- (internal) removes all but the first cache
 *	from the UNDO_SET_CACHE vector
 *
 * Only the valgrind related flags are valid for the vg_flags variable.
 */
static void
tx_clear_set_cache_but_first(PMEMobjpool *pop, struct tx_undo_runtime *tx_rt,
	struct tx *tx, enum tx_clr_flag vg_flags)
{
	LOG(4, NULL);

	struct pvector_context *cache_undo = tx_rt->ctx[UNDO_SET_CACHE];
	uint64_t first_cache = pvector_first(cache_undo);

	if (first_cache == 0)
		return;

	uint64_t off;

	int zero_all = tx == NULL;

	while ((off = pvector_last(cache_undo)) != first_cache) {
		tx_clear_undo_log_vg(pop, off, vg_flags);

		pvector_pop_back(cache_undo, tx_free_vec_entry);
		zero_all = 1;
	}

	tx_clear_undo_log_vg(pop, first_cache, vg_flags);
	struct tx_range_cache *cache = OBJ_OFF_TO_PTR(pop, first_cache);

	size_t sz;
	if (zero_all) {
		sz = palloc_usable_size(&pop->heap, first_cache);
	} else {
		ASSERTne(tx, NULL);
		struct lane_tx_runtime *r = tx->section->runtime;
		sz = r->cache_offset;
	}

	if (sz) {
		VALGRIND_ADD_TO_TX(cache, sz);
		pmemops_memset_persist(&pop->p_ops, cache, 0, sz);
		VALGRIND_REMOVE_FROM_TX(cache, sz);
	}

#ifdef DEBUG
	if (!zero_all && /* for recovery we know we zeroed everything */
		!pop->tx_debug_skip_expensive_checks) {
		uint64_t usable_size = palloc_usable_size(&pop->heap,
			first_cache);
		ASSERTeq(util_is_zeroed(cache, usable_size), 1);
	}
#endif
}

/*
 * tx_abort_set -- (internal) abort all set operations
 */
static void
tx_abort_set(PMEMobjpool *pop, struct tx_undo_runtime *tx_rt, int recovery)
{
	LOG(7, NULL);

	struct tx *tx = recovery ? NULL : get_tx();

	if (recovery)
		tx_foreach_set(pop, NULL, tx_rt, tx_abort_recover_range);
	else
		tx_foreach_set(pop, tx, tx_rt, tx_abort_restore_range);

	if (recovery) /* if recovering from a crash, remove all of the caches */
		tx_clear_undo_log(pop, tx_rt->ctx[UNDO_SET_CACHE], 0,
			TX_CLR_FLAG_FREE | TX_CLR_FLAG_VG_CLEAN);
	else /* otherwise leave the first one */
		tx_clear_set_cache_but_first(pop, tx_rt, tx,
			TX_CLR_FLAG_VG_CLEAN);

	tx_clear_undo_log(pop, tx_rt->ctx[UNDO_SET], 0,
		TX_CLR_FLAG_FREE | TX_CLR_FLAG_VG_CLEAN);
}

/*
 * tx_post_commit_alloc -- (internal) do post commit operations for
 * allocated objects
 */
static void
tx_post_commit_alloc(PMEMobjpool *pop, struct tx_undo_runtime *tx_rt)
{
	LOG(7, NULL);

	tx_clear_undo_log(pop, tx_rt->ctx[UNDO_ALLOC], 0,
			TX_CLR_FLAG_VG_TX_REMOVE);
}

/*
 * tx_post_commit_free -- (internal) do post commit operations for
 * freeing objects
 */
static void
tx_post_commit_free(PMEMobjpool *pop, struct tx_undo_runtime *tx_rt)
{
	LOG(7, NULL);

	tx_clear_undo_log(pop, tx_rt->ctx[UNDO_FREE], 0,
		TX_CLR_FLAG_FREE | TX_CLR_FLAG_VG_TX_REMOVE);
}

#if VG_PMEMCHECK_ENABLED
/*
 * tx_post_commit_range_vg_tx_remove -- (internal) removes object from
 * transaction tracked by pmemcheck
 */
static void
tx_post_commit_range_vg_tx_remove(PMEMobjpool *pop, struct tx *tx,
		struct tx_range *range)
{
	VALGRIND_REMOVE_FROM_TX(OBJ_OFF_TO_PTR(pop, range->offset),
			range->size);
}
#endif

/*
 * tx_post_commit_set -- (internal) do post commit operations for
 * add range
 */
static void
tx_post_commit_set(PMEMobjpool *pop, struct tx *tx,
		struct tx_undo_runtime *tx_rt, int recovery)
{
	LOG(7, NULL);

#if VG_PMEMCHECK_ENABLED
	if (On_valgrind)
		tx_foreach_set(pop, tx, tx_rt,
				tx_post_commit_range_vg_tx_remove);
#endif

	if (recovery) /* if recovering from a crash, remove all of the caches */
		tx_clear_undo_log(pop, tx_rt->ctx[UNDO_SET_CACHE], 0,
			TX_CLR_FLAG_FREE);
	else /* otherwise leave the first one */
		tx_clear_set_cache_but_first(pop, tx_rt, tx, 0);

	tx_clear_undo_log(pop, tx_rt->ctx[UNDO_SET], 0, TX_CLR_FLAG_FREE);
}

/*
 * tx_fulfill_reservations -- fulfills all volatile state
 *	allocation reservations
 */
static void
tx_fulfill_reservations(struct tx *tx)
{
	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx->section->runtime;

	if (lane->actvcnt == 0)
		return;

	PMEMobjpool *pop = tx->pop;

	struct redo_log *redo = pmalloc_redo_hold(pop);

	struct operation_context ctx;
	operation_init(&ctx, pop, pop->redo, redo);

	palloc_publish(&pop->heap, lane->alloc_actv, lane->actvcnt, &ctx);
	lane->actvcnt = 0;
	lane->actvundo = 0;

	pmalloc_redo_release(pop);
}

/*
 * tx_cancel_reservations -- cancels all volatile state allocation reservations
 */
static void
tx_cancel_reservations(PMEMobjpool *pop, struct lane_tx_runtime *lane)
{
	palloc_cancel(&pop->heap, lane->alloc_actv, lane->actvcnt);
	lane->actvcnt = 0;
	lane->actvundo = 0;
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
		pmemops_flush(&pop->p_ops, OBJ_OFF_TO_PTR(pop, range->offset),
				range->size);
	}
}

/*
 * tx_pre_commit -- (internal) do pre-commit operations
 */
static void
tx_pre_commit(PMEMobjpool *pop, struct tx *tx, struct lane_tx_runtime *lane)
{
	LOG(5, NULL);

	ASSERTne(tx->section->runtime, NULL);

	tx_fulfill_reservations(tx);

	/* Flush all regions and destroy the whole tree. */
	ravl_delete_cb(lane->ranges, tx_flush_range, pop);
	lane->ranges = NULL;
}

/*
 * tx_rebuild_undo_runtime -- (internal) reinitializes runtime state of vectors
 */
static int
tx_rebuild_undo_runtime(PMEMobjpool *pop, struct lane_tx_layout *layout,
	struct tx_undo_runtime *tx_rt)
{
	LOG(7, NULL);

	int i;
	for (i = UNDO_ALLOC; i < MAX_UNDO_TYPES; ++i) {
		if (tx_rt->ctx[i] == NULL)
			tx_rt->ctx[i] = pvector_new(pop, &layout->undo_log[i]);
		else
			pvector_reinit(tx_rt->ctx[i]);

		if (tx_rt->ctx[i] == NULL)
			goto error_init;
	}

	return 0;

error_init:
	for (--i; i >= 0; --i)
		pvector_delete(tx_rt->ctx[i]);

	return -1;
}

/*
 * tx_destroy_undo_runtime -- (internal) destroys runtime state of undo logs
 */
static void
tx_destroy_undo_runtime(struct tx_undo_runtime *tx)
{
	LOG(7, NULL);

	for (int i = UNDO_ALLOC; i < MAX_UNDO_TYPES; ++i)
		pvector_delete(tx->ctx[i]);
}

/*
 * tx_post_commit -- (internal) do post commit operations
 */
static void
tx_post_commit(PMEMobjpool *pop, struct tx *tx, struct lane_tx_layout *layout,
		int recovery)
{
	LOG(7, NULL);

	struct tx_undo_runtime *tx_rt;
	struct tx_undo_runtime new_rt = { .ctx = {NULL, } };
	if (recovery) {
		if (tx_rebuild_undo_runtime(pop, layout, &new_rt) != 0)
			FATAL("!Cannot rebuild runtime undo log state");

		tx_rt = &new_rt;
	} else {
		struct lane_tx_runtime *lane = tx->section->runtime;
		tx_rt = &lane->undo;
	}

	tx_post_commit_set(pop, tx, tx_rt, recovery);
	tx_post_commit_alloc(pop, tx_rt);
	tx_post_commit_free(pop, tx_rt);

	if (recovery)
		tx_destroy_undo_runtime(tx_rt);
}

/*
 * tx_abort -- (internal) abort all allocated objects
 */
static void
tx_abort(PMEMobjpool *pop, struct lane_tx_runtime *lane,
		struct lane_tx_layout *layout, int recovery)
{
	LOG(7, NULL);

	struct tx_undo_runtime *tx_rt;
	struct tx_undo_runtime new_rt = { .ctx = {NULL, } };
	if (recovery) {
		if (tx_rebuild_undo_runtime(pop, layout, &new_rt) != 0)
			FATAL("!Cannot rebuild runtime undo log state");

		tx_rt = &new_rt;
	} else {
		tx_rt = &lane->undo;
	}

	tx_abort_set(pop, tx_rt, recovery);
	tx_abort_alloc(pop, tx_rt, lane);
	tx_abort_free(pop, tx_rt);

	if (recovery) {
		tx_destroy_undo_runtime(tx_rt);
	} else {
		tx_cancel_reservations(pop, lane);
		ASSERTne(lane, NULL);
		ravl_delete(lane->ranges);
		lane->ranges = NULL;
	}
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
	SLIST_FOREACH(txl, &tx->tx_locks, tx_lock) {
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
				errno = retval;
				ERR("!pmemobj_mutex_lock");
			}
			break;
		case TX_PARAM_RWLOCK:
			txl->lock.rwlock = lock;
			retval = pmemobj_rwlock_wrlock(tx->pop,
				txl->lock.rwlock);
			if (retval) {
				errno = retval;
				ERR("!pmemobj_rwlock_wrlock");
			}
			break;
		default:
			ERR("Unrecognized lock type");
			ASSERT(0);
			break;
	}

	if (retval == 0)
		SLIST_INSERT_HEAD(&tx->tx_locks, txl, tx_lock);

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

	while (!SLIST_EMPTY(&tx->tx_locks)) {
		struct tx_lock_data *tx_lock = SLIST_FIRST(&tx->tx_locks);
		SLIST_REMOVE_HEAD(&tx->tx_locks, tx_lock);
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
tx_lane_ranges_insert_def(struct lane_tx_runtime *lane,
	const struct tx_range_def *rdef)
{
	LOG(3, "rdef->offset %"PRIu64" rdef->size %"PRIu64,
		rdef->offset, rdef->size);

	int ret = ravl_emplace_copy(lane->ranges, rdef);
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

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx->section->runtime;

	PMEMobjpool *pop = tx->pop;

	if ((lane->actvcnt + 1) == MAX_TX_ALLOC_RESERVATIONS) {
		tx_fulfill_reservations(tx);
	}

	int rs = lane->actvcnt;

	uint64_t flags = args.flags;

	if (palloc_reserve(&pop->heap, size, constructor, &args, type_num, 0,
		CLASS_ID_FROM_FLAG(flags), &lane->alloc_actv[rs]) != 0) {
		ERR("out of memory");
		return obj_tx_abort_null(ENOMEM);
	}

	lane->actvcnt++;

	/* allocate object to undo log */
	PMEMoid retoid = OID_NULL;
	retoid.off = lane->alloc_actv[rs].heap.offset;
	retoid.pool_uuid_lo = pop->uuid_lo;
	size = palloc_usable_size(&pop->heap, retoid.off);

	const struct tx_range_def r = {retoid.off, size, flags};
	if (tx_lane_ranges_insert_def(lane, &r) != 0)
		goto err_oom;

	uint64_t *entry_offset = pvector_push_back(lane->undo.ctx[UNDO_ALLOC]);
	if (entry_offset == NULL)
		goto err_oom;

	/*
	 * The offset of the object is stored in the undo vector before it is
	 * actually allocated. The only phase at which we are sure the objects
	 * in the undo logs are actually allocated is in post-commit.
	 * This means that when handling abort, each offset needs to be checked
	 * whether it should be freed or not.
	 */
	*entry_offset = retoid.off;
	pmemops_persist(&pop->p_ops, entry_offset, sizeof(*entry_offset));

	lane->actvundo++;

	return retoid;

err_oom:

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

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx->section->runtime;

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
			pvector_pop_back(lane->undo.ctx[UNDO_ALLOC],
				tx_free_vec_entry);
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

	struct lane_tx_runtime *lane = NULL;
	if (tx->stage == TX_STAGE_WORK) {
		ASSERTne(tx->section, NULL);
		if (tx->pop != pop) {
			ERR("nested transaction for different pool");
			return obj_tx_abort_err(EINVAL);
		}

		VALGRIND_START_TX;
	} else if (tx->stage == TX_STAGE_NONE) {
		VALGRIND_START_TX;

		unsigned idx = lane_hold(pop, &tx->section,
			LANE_SECTION_TRANSACTION);

		lane = tx->section->runtime;
		VALGRIND_ANNOTATE_NEW_MEMORY(lane, sizeof(*lane));

		SLIST_INIT(&tx->tx_entries);
		SLIST_INIT(&tx->tx_locks);

		lane->ranges = ravl_new_sized(tx_range_def_cmp,
			sizeof(struct tx_range_def));
		lane->cache_offset = 0;
		lane->lane_idx = idx;

		lane->actvcnt = 0;
		lane->actvundo = 0;

		struct lane_tx_layout *layout =
			(struct lane_tx_layout *)tx->section->layout;

		if (tx_rebuild_undo_runtime(pop, layout, &lane->undo) != 0) {
			tx->stage = TX_STAGE_ONABORT;
			err = errno;
			return err;
		}

		tx->pop = pop;
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

	SLIST_INSERT_HEAD(&tx->tx_entries, txd, tx_entry);

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

	struct tx_data *txd = SLIST_FIRST(&tx->tx_entries);

	/* is this the outermost transaction? */
	if (SLIST_NEXT(txd, tx_entry) == NULL)
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

	ASSERT(tx->section != NULL);

	if (errnum == 0)
		errnum = ECANCELED;

	tx->stage = TX_STAGE_ONABORT;
	struct lane_tx_runtime *lane = tx->section->runtime;
	struct tx_data *txd = SLIST_FIRST(&tx->tx_entries);

	if (SLIST_NEXT(txd, tx_entry) == NULL) {
		/* this is the outermost transaction */
		struct lane_tx_layout *layout =
				(struct lane_tx_layout *)tx->section->layout;

		/* process the undo log */
		tx_abort(tx->pop, lane, layout, 0 /* abort */);
		lane_release(tx->pop);
		tx->section = NULL;
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
	obj_tx_abort(errnum, 1);
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

/*
 * tx_post_commit_cleanup -- performs all the necessary cleanup on a lane after
 *	successful commit
 */
static void
tx_post_commit_cleanup(PMEMobjpool *pop,
	struct lane_section *section, int detached)
{
	struct lane_tx_runtime *runtime =
			(struct lane_tx_runtime *)section->runtime;
	struct lane_tx_layout *layout =
		(struct lane_tx_layout *)section->layout;

	struct tx *tx = get_tx();

	if (detached) {
#if VG_HELGRIND_ENABLED || VG_DRD_ENABLED
		/* cleanup the state of lane data in race detection tools */
		if (On_valgrind) {
			VALGRIND_ANNOTATE_NEW_MEMORY(layout, sizeof(*layout));
			VALGRIND_ANNOTATE_NEW_MEMORY(runtime, sizeof(*runtime));
			int ret = tx_rebuild_undo_runtime(pop, layout,
				&runtime->undo);
			ASSERTeq(ret, 0); /* can't fail, valgrind-related */
		}
#endif

		lane_attach(pop, runtime->lane_idx);
		tx->pop = pop;
		tx->section = section;
		tx->stage = TX_STAGE_ONCOMMIT;
	}

	/* post commit phase */
	tx_post_commit(pop, tx, layout, 0 /* not recovery */);

	/* clear transaction state */
	tx_set_state(pop, layout, TX_STATE_NONE);

	runtime->cache_offset = 0;
	/* cleanup cache */

	ASSERTeq(pvector_size(runtime->undo.ctx[UNDO_ALLOC]), 0);
	ASSERTeq(pvector_size(runtime->undo.ctx[UNDO_SET]), 0);
	ASSERTeq(pvector_size(runtime->undo.ctx[UNDO_FREE]), 0);
	ASSERT(pvector_size(runtime->undo.ctx[UNDO_FREE]) == 0 ||
		pvector_size(runtime->undo.ctx[UNDO_FREE]) == 1);

	lane_release(pop);
}

/*
 * pmemobj_tx_commit -- commits current transaction
 */
void
pmemobj_tx_commit(void)
{
	LOG(3, NULL);
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	/* WORK */
	obj_tx_callback(tx);

	ASSERT(tx->section != NULL);

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx->section->runtime;
	struct tx_data *txd = SLIST_FIRST(&tx->tx_entries);

	if (SLIST_NEXT(txd, tx_entry) == NULL) {
		/* this is the outermost transaction */

		struct lane_tx_layout *layout =
			(struct lane_tx_layout *)tx->section->layout;
		PMEMobjpool *pop = tx->pop;

		/* pre-commit phase */
		tx_pre_commit(pop, tx, lane);

		pmemops_drain(&pop->p_ops);

		/* set transaction state as committed */
		tx_set_state(pop, layout, TX_STATE_COMMITTED);

		if (pop->tx_postcommit_tasks != NULL &&
			ringbuf_tryenqueue(pop->tx_postcommit_tasks,
				tx->section) == 0) {
			lane_detach(pop);
		} else {
			tx_post_commit_cleanup(pop, tx->section, 0);
		}

		tx->section = NULL;
	}

	tx->stage = TX_STAGE_ONCOMMIT;

	/* ONCOMMIT */
	obj_tx_callback(tx);
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

	struct tx_data *txd = SLIST_FIRST(&tx->tx_entries);
	SLIST_REMOVE_HEAD(&tx->tx_entries, tx_entry);

	Free(txd);

	VALGRIND_END_TX;

	if (SLIST_EMPTY(&tx->tx_entries)) {
		ASSERTeq(tx->section, NULL);

		release_and_free_tx_locks(tx);
		tx->pop = NULL;
		tx->stage = TX_STAGE_NONE;

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
	case MAX_TX_STAGE:
		ASSERT(0);
	}
}

/*
 * pmemobj_tx_add_large -- (internal) adds large memory range to undo log
 */
static int
pmemobj_tx_add_large(struct tx *tx, struct tx_range_def *args)
{
	struct lane_tx_runtime *runtime = tx->section->runtime;
	struct pvector_context *undo = runtime->undo.ctx[UNDO_SET];
	uint64_t *entry = pvector_push_back(undo);
	if (entry == NULL) {
		ERR("large set undo log too large");
		return -1;
	}

	/* insert snapshot to undo log */
	int ret = pmalloc_construct(tx->pop, entry,
			args->size + sizeof(struct tx_range),
			constructor_tx_add_range, args,
			0, OBJ_INTERNAL_OBJECT_MASK, 0);

	if (ret != 0) {
		pvector_pop_back(undo, NULL);
	}

	return ret;
}

/*
 * constructor_tx_range_cache -- (internal) cache constructor
 */
static int
constructor_tx_range_cache(void *ctx, void *ptr, size_t usable_size, void *arg)
{
	LOG(5, NULL);
	PMEMobjpool *pop = ctx;
	const struct pmem_ops *p_ops = &pop->p_ops;

	ASSERTne(ptr, NULL);

	VALGRIND_ADD_TO_TX(ptr, usable_size);

	pmemops_memset_persist(p_ops, ptr, 0, usable_size);

	VALGRIND_REMOVE_FROM_TX(ptr, usable_size);

	return 0;
}

/*
 * pmemobj_tx_get_range_cache -- (internal) returns first available cache
 */
static struct tx_range_cache *
pmemobj_tx_get_range_cache(PMEMobjpool *pop, struct tx *tx,
	struct pvector_context *undo, uint64_t *remaining_space)
{
	uint64_t last_cache = pvector_last(undo);
	uint64_t cache_size;

	struct tx_range_cache *cache = NULL;
	/* get the last element from the caches list */
	if (last_cache != 0) {
		cache = OBJ_OFF_TO_PTR(pop, last_cache);
		cache_size = palloc_usable_size(&pop->heap, last_cache);
	}

	struct lane_tx_runtime *runtime = tx->section->runtime;

	/* verify if the cache exists and has at least 8 bytes of free space */
	if (cache == NULL || runtime->cache_offset +
		sizeof(struct tx_range) >= cache_size) {
		/* no existing cache, allocate a new one */
		uint64_t *entry = pvector_push_back(undo);
		if (entry == NULL) {
			ERR("cache set undo log too large");
			return NULL;
		}
		int err = pmalloc_construct(pop, entry,
			pop->tx_params->cache_size,
			constructor_tx_range_cache, NULL,
			0, OBJ_INTERNAL_OBJECT_MASK, 0);

		if (err != 0) {
			pvector_pop_back(undo, NULL);
			return NULL;
		}

		cache = OBJ_OFF_TO_PTR(pop, *entry);
		cache_size = palloc_usable_size(&pop->heap, *entry);

		/* since the cache is new, we start the count from 0 */
		runtime->cache_offset = 0;
	}

	*remaining_space = cache_size - runtime->cache_offset;

	return cache;
}

/*
 * pmemobj_tx_add_small -- (internal) adds small memory range to undo log cache
 */
static int
pmemobj_tx_add_small(struct tx *tx, struct tx_range_def *args)
{
	PMEMobjpool *pop = tx->pop;

	struct lane_tx_runtime *runtime = tx->section->runtime;
	struct pvector_context *undo = runtime->undo.ctx[UNDO_SET_CACHE];
	const struct pmem_ops *p_ops = &pop->p_ops;

	uint64_t remaining_space;
	struct tx_range_cache *cache = pmemobj_tx_get_range_cache(pop, tx,
		undo, &remaining_space);
	if (cache == NULL) {
		ERR("Failed to create range cache");
		return 1;
	}

	/* those structures are binary compatible */
	struct tx_range *range =
		(struct tx_range *)((char *)cache + runtime->cache_offset);

	uint64_t data_offset = args->offset;
	uint64_t data_size = args->size;
	uint64_t range_size = TX_ALIGN_SIZE(args->size, TX_RANGE_MASK) +
		sizeof(struct tx_range);

	if (remaining_space < range_size) {
		ASSERT(remaining_space > sizeof(struct tx_range));
		range_size = remaining_space;
		data_size = remaining_space - sizeof(struct tx_range);

		args->offset += data_size;
		args->size -= data_size;
	} else {
		args->size = 0;
	}

	runtime->cache_offset += range_size;

	VALGRIND_ADD_TO_TX(range, range_size);

	/* this isn't transactional so we have to keep the order */
	void *src = OBJ_OFF_TO_PTR(pop, data_offset);
	VALGRIND_ADD_TO_TX(src, data_size);

	pmemops_memcpy_persist(p_ops, range->data, src, data_size);

	/* the range is only valid if both size and offset are != 0 */
	range->size = data_size;
	range->offset = data_offset;
	pmemops_persist(p_ops, range, sizeof(struct tx_range));

	VALGRIND_REMOVE_FROM_TX(range, range_size);

	if (args->size != 0)
		return pmemobj_tx_add_small(tx, args);

	return 0;
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
	 * Depending on the size of the block, either allocate an
	 * entire new object or use cache.
	 */
	return snapshot->size > tx->pop->tx_params->cache_threshold ?
		pmemobj_tx_add_large(tx, snapshot) :
		pmemobj_tx_add_small(tx, snapshot);
}

/*
 * pmemobj_tx_add_common -- (internal) common code for adding persistent memory
 *				into the transaction
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
	struct lane_tx_runtime *runtime = tx->section->runtime;

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
		struct ravl_node *n = ravl_find(runtime->ranges, &search, p);
		p = RAVL_PREDICATE_LESS_EQUAL;
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
				ret = tx_lane_ranges_insert_def(runtime, &r);
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

			if (snapshot.size != 0) {
				ret = pmemobj_tx_add_snapshot(tx, &snapshot);
				if (ret != 0)
					break;
			}

			/*
			 * We have to skip searching for LESS_EQUAL because
			 * the snapshot we would just find the snapshot we just
			 * created, which would be a waste of time.
			 */
			p = RAVL_PREDICATE_LESS;

			/*
			 * If there's a snapshot adjacent on right side, merge
			 * the two ranges together.
			 */
			if (nprev != NULL) {
				struct tx_range_def *fprev = ravl_data(nprev);
				ASSERTeq(rend, fprev->offset);
				f->size += fprev->size;
				ravl_remove(runtime->ranges, nprev);
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

			p = RAVL_PREDICATE_LESS;
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
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	PMEMobjpool *pop = tx->pop;

	if (!OBJ_PTR_FROM_POOL(pop, ptr)) {
		ERR("object outside of pool");
		return obj_tx_abort_err(EINVAL);
	}

	struct tx_range_def args = {
		.offset = (uint64_t)((char *)ptr - (char *)pop),
		.size = size,
		.flags = 0,
	};

	return pmemobj_tx_add_common(tx, &args);
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

	if (!OBJ_PTR_FROM_POOL(tx->pop, ptr)) {
		ERR("object outside of pool");
		return obj_tx_abort_err(EINVAL);
	}

	if (flags & ~POBJ_XADD_VALID_FLAGS) {
		ERR("unknown flags 0x%" PRIx64, flags & ~POBJ_XADD_VALID_FLAGS);
		return obj_tx_abort_err(EINVAL);
	}

	struct tx_range_def args = {
		.offset = (uint64_t)((char *)ptr - (char *)tx->pop),
		.size = size,
		.flags = flags,
	};

	return pmemobj_tx_add_common(tx, &args);
}

/*
 * pmemobj_tx_add_range -- adds persistent memory range into the transaction
 */
int
pmemobj_tx_add_range(PMEMoid oid, uint64_t hoff, size_t size)
{
	LOG(3, NULL);
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	if (oid.pool_uuid_lo != tx->pop->uuid_lo) {
		ERR("invalid pool uuid");
		return obj_tx_abort_err(EINVAL);
	}
	ASSERT(OBJ_OID_IS_VALID(tx->pop, oid));

	struct tx_range_def args = {
		.offset = oid.off + hoff,
		.size = size,
		.flags = 0,
	};

	return pmemobj_tx_add_common(tx, &args);
}

/*
 * pmemobj_tx_xadd_range -- adds persistent memory range into the transaction
 */
int
pmemobj_tx_xadd_range(PMEMoid oid, uint64_t hoff, size_t size, uint64_t flags)
{
	LOG(3, NULL);
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	if (oid.pool_uuid_lo != tx->pop->uuid_lo) {
		ERR("invalid pool uuid");
		return obj_tx_abort_err(EINVAL);
	}
	ASSERT(OBJ_OID_IS_VALID(tx->pop, oid));

	if (flags & ~POBJ_XADD_VALID_FLAGS) {
		ERR("unknown flags 0x%" PRIx64, flags & ~POBJ_XADD_VALID_FLAGS);
		return obj_tx_abort_err(EINVAL);
	}

	struct tx_range_def args = {
		.offset = oid.off + hoff,
		.size = size,
		.flags = flags,
	};

	return pmemobj_tx_add_common(tx, &args);
}

/*
 * pmemobj_tx_alloc -- allocates a new object
 */
PMEMoid
pmemobj_tx_alloc(size_t size, uint64_t type_num)
{
	LOG(3, NULL);
	struct tx *tx = get_tx();

	ASSERT_IN_TX(tx);
	ASSERT_TX_STAGE_WORK(tx);

	if (size == 0) {
		ERR("allocation with size 0");
		return obj_tx_abort_null(EINVAL);
	}

	return tx_alloc_common(tx, size, (type_num_t)type_num,
			constructor_tx_alloc, ALLOC_ARGS(0));
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

	if (size == 0) {
		ERR("allocation with size 0");
		return obj_tx_abort_null(EINVAL);
	}

	return tx_alloc_common(tx, size, (type_num_t)type_num,
			constructor_tx_alloc, ALLOC_ARGS(POBJ_FLAG_ZERO));
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

	if (size == 0) {
		ERR("allocation with size 0");
		return obj_tx_abort_null(EINVAL);
	}

	if (flags & ~POBJ_TX_XALLOC_VALID_FLAGS) {
		ERR("unknown flags 0x%" PRIx64,
				flags & ~POBJ_TX_XALLOC_VALID_FLAGS);
		return obj_tx_abort_null(EINVAL);
	}

	return tx_alloc_common(tx, size, (type_num_t)type_num,
			constructor_tx_alloc, ALLOC_ARGS(flags));
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

	return tx_realloc_common(tx, oid, size, type_num,
			constructor_tx_alloc, constructor_tx_alloc, 0);
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

	return tx_realloc_common(tx, oid, size, type_num,
			constructor_tx_alloc, constructor_tx_alloc,
			POBJ_FLAG_ZERO);
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

	if (NULL == s) {
		ERR("cannot duplicate NULL string");
		return obj_tx_abort_null(EINVAL);
	}

	size_t len = strlen(s);

	if (len == 0)
		return tx_alloc_common(tx, sizeof(char), (type_num_t)type_num,
				constructor_tx_alloc,
				ALLOC_ARGS(POBJ_FLAG_ZERO));

	size_t size = (len + 1) * sizeof(char);

	return tx_alloc_common(tx, size, (type_num_t)type_num,
			constructor_tx_alloc, COPY_ARGS(0, s, size));
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

	if (NULL == s) {
		ERR("cannot duplicate NULL string");
		return obj_tx_abort_null(EINVAL);
	}

	size_t len = wcslen(s);

	if (len == 0)
		return tx_alloc_common(tx, sizeof(wchar_t),
				(type_num_t)type_num, constructor_tx_alloc,
				ALLOC_ARGS(POBJ_FLAG_ZERO));

	size_t size = (len + 1) * sizeof(wchar_t);

	return tx_alloc_common(tx, size, (type_num_t)type_num,
			constructor_tx_alloc, COPY_ARGS(0, s, size));
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

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx->section->runtime;
	PMEMobjpool *pop = tx->pop;

	if (pop->uuid_lo != oid.pool_uuid_lo) {
		ERR("invalid pool uuid");
		return obj_tx_abort_err(EINVAL);
	}
	ASSERT(OBJ_OID_IS_VALID(pop, oid));

	uint64_t *entry = pvector_push_back(lane->undo.ctx[UNDO_FREE]);
	if (entry == NULL) {
		ERR("free undo log too large");
		return obj_tx_abort_err(ENOMEM);
	}
	*entry = oid.off;
	pmemops_persist(&pop->p_ops, entry, sizeof(*entry));

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

	tx_fulfill_reservations(tx);
	ASSERT(actvcnt <= MAX_TX_ALLOC_RESERVATIONS);
	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx->section->runtime;

	struct pvector_context *ctx = lane->undo.ctx[UNDO_ALLOC];

	int nentries = 0;
	size_t i;
	for (i = 0; i < actvcnt; ++i) {
		if (actv[i].type != POBJ_ACTION_TYPE_HEAP) {
			ERR("only heap actions can be "
			"published with a transaction");
			break;
		}
		uint64_t *e = pvector_push_back(ctx);
		if (e == NULL)
			break;
		*e = actv[i].heap.offset;
		pmemops_persist(&tx->pop->p_ops, e, sizeof(*e));
		nentries++;

		size_t size = palloc_usable_size(&tx->pop->heap,
			actv[i].heap.offset);

		const struct tx_range_def r = {actv[i].heap.offset,
				size, POBJ_FLAG_NO_FLUSH};
		if (tx_lane_ranges_insert_def(lane, &r) != 0)
			break;
	}

	if (i != actvcnt) { /* failed to store entries in the undo log */
		while (nentries--) {
			pvector_pop_back(ctx, tx_clear_vec_entry);
		}
		ERR("alloc undo log too large");
		return obj_tx_abort_err(ENOMEM);
	}

	memcpy(lane->alloc_actv, actv,
		sizeof(struct pobj_action) * actvcnt);

	lane->actvcnt = (int)actvcnt;
	lane->actvundo = (int)actvcnt;

	return 0;
}

/*
 * lane_transaction_construct_rt -- construct runtime part of transaction
 * section
 */
static void *
lane_transaction_construct_rt(PMEMobjpool *pop)
{
	/*
	 * Lane construction is executed before recovery so it's important
	 * to keep in mind that any volatile state that could have been
	 * initialized here might be invalid once the recovery finishes.
	 */
	return Zalloc(sizeof(struct lane_tx_runtime));
}

/*
 * lane_transaction_destroy_rt -- destroy runtime part of transaction section
 */
static void
lane_transaction_destroy_rt(PMEMobjpool *pop, void *rt)
{
	struct lane_tx_runtime *lane = rt;
	tx_destroy_undo_runtime(&lane->undo);
	Free(lane);
}

/*
 * lane_transaction_recovery -- recovery of transaction lane section
 */
static int
lane_transaction_recovery(PMEMobjpool *pop, void *data, unsigned length)
{
	struct lane_tx_layout *layout = data;
	int ret = 0;
	ASSERT(sizeof(*layout) <= length);

	if (layout->state == TX_STATE_COMMITTED) {
		/*
		 * The transaction has been committed so we have to
		 * process the undo log, do the post commit phase
		 * and clear the transaction state.
		 */
		tx_post_commit(pop, NULL, layout, 1 /* recovery */);
		tx_set_state(pop, layout, TX_STATE_NONE);
	} else {
		/* process undo log and restore all operations */
		tx_abort(pop, NULL, layout, 1 /* recovery */);
	}

	return ret;
}

/*
 * lane_transaction_check -- consistency check of transaction lane section
 */
static int
lane_transaction_check(PMEMobjpool *pop, void *data, unsigned length)
{
	LOG(3, "tx lane %p", data);

	struct lane_tx_layout *tx_sec = data;

	if (tx_sec->state != TX_STATE_NONE &&
		tx_sec->state != TX_STATE_COMMITTED) {
		ERR("tx lane: invalid transaction state");
		return -1;
	}

	return 0;
}

/*
 * lane_transaction_boot -- global runtime init routine of tx section
 */
static int
lane_transaction_boot(PMEMobjpool *pop)
{
	/* NOP */
	return 0;
}

/*
 * lane_transaction_cleanup -- global runtime cleanup routine of tx section
 */
static int
lane_transaction_cleanup(PMEMobjpool *pop)
{
	/* NOP */
	return 0;
}

static struct section_operations transaction_ops = {
	.construct_rt = lane_transaction_construct_rt,
	.destroy_rt = lane_transaction_destroy_rt,
	.recover = lane_transaction_recovery,
	.check = lane_transaction_check,
	.boot = lane_transaction_boot,
	.cleanup = lane_transaction_cleanup,
};

SECTION_PARM(LANE_SECTION_TRANSACTION, &transaction_ops);

/*
 * CTL_READ_HANDLER(size) -- gets the cache size transaction parameter
 */
static int
CTL_READ_HANDLER(size)(PMEMobjpool *pop,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	ssize_t *arg_out = arg;

	*arg_out = (ssize_t)pop->tx_params->cache_size;

	return 0;
}

/*
 * CTL_WRITE_HANDLER(size) -- sets the cache size transaction parameter
 */
static int
CTL_WRITE_HANDLER(size)(PMEMobjpool *pop,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	ssize_t arg_in = *(int *)arg;

	if (arg_in < 0 || arg_in > (ssize_t)PMEMOBJ_MAX_ALLOC_SIZE) {
		errno = EINVAL;
		ERR("invalid cache size, must be between 0 and max alloc size");
		return -1;
	}

	size_t argu = (size_t)arg_in;

	pop->tx_params->cache_size = argu;
	if (pop->tx_params->cache_threshold > argu)
		pop->tx_params->cache_threshold = argu;

	return 0;
}

static struct ctl_argument CTL_ARG(size) = CTL_ARG_LONG_LONG;

/*
 * CTL_READ_HANDLER(threshold) -- gets the cache threshold transaction parameter
 */
static int
CTL_READ_HANDLER(threshold)(PMEMobjpool *pop,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	ssize_t *arg_out = arg;

	*arg_out = (ssize_t)pop->tx_params->cache_threshold;

	return 0;
}

/*
 * CTL_WRITE_HANDLER(threshold) --
 *	sets the cache threshold transaction parameter
 */
static int
CTL_WRITE_HANDLER(threshold)(PMEMobjpool *pop,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	ssize_t arg_in = *(int *)arg;

	if (arg_in < 0 || arg_in > (ssize_t)pop->tx_params->cache_size) {
		errno = EINVAL;
		ERR("invalid threshold size, must be between 0 and cache size");
		return -1;
	}

	pop->tx_params->cache_threshold = (size_t)arg_in;

	return 0;
}

static struct ctl_argument CTL_ARG(threshold) = CTL_ARG_LONG_LONG;

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
CTL_READ_HANDLER(skip_expensive_checks)(PMEMobjpool *pop,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	int *arg_out = arg;

	*arg_out = pop->tx_debug_skip_expensive_checks;

	return 0;
}

/*
 * CTL_WRITE_HANDLER(skip_expensive_checks) -- stores "skip_expensive_checks"
 * var in pool ctl
 */
static int
CTL_WRITE_HANDLER(skip_expensive_checks)(PMEMobjpool *pop,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	int arg_in = *(int *)arg;

	pop->tx_debug_skip_expensive_checks = arg_in;
	return 0;
}

static struct ctl_argument CTL_ARG(skip_expensive_checks) = CTL_ARG_BOOLEAN;

static const struct ctl_node CTL_NODE(debug)[] = {
	CTL_LEAF_RW(skip_expensive_checks),

	CTL_NODE_END
};

/*
 * CTL_WRITE_HANDLER(queue_depth) -- returns the depth of the post commit queue
 */
static int
CTL_READ_HANDLER(queue_depth)(PMEMobjpool *pop, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	int *arg_out = arg;

	*arg_out = (int)ringbuf_length(pop->tx_postcommit_tasks);

	return 0;
}

/*
 * CTL_WRITE_HANDLER(queue_depth) -- sets the depth of the post commit queue
 */
static int
CTL_WRITE_HANDLER(queue_depth)(PMEMobjpool *pop, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	int arg_in = *(int *)arg;

	struct ringbuf *ntasks = ringbuf_new((unsigned)arg_in);
	if (ntasks == NULL)
		return -1;

	if (pop->tx_postcommit_tasks != NULL) {
		ringbuf_delete(pop->tx_postcommit_tasks);
	}

	pop->tx_postcommit_tasks = ntasks;

	return 0;
}

static struct ctl_argument CTL_ARG(queue_depth) = CTL_ARG_INT;

/*
 * CTL_READ_HANDLER(worker) -- launches the post commit worker thread function
 */
static int
CTL_READ_HANDLER(worker)(PMEMobjpool *pop, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{

	struct lane_section *section;
	while ((section = ringbuf_dequeue_s(pop->tx_postcommit_tasks,
		sizeof(*section))) != NULL) {
		tx_post_commit_cleanup(pop, section, 1);
	}

	return 0;
}

/*
 * CTL_READ_HANDLER(stop) -- stops all post commit workers
 */
static int
CTL_READ_HANDLER(stop)(PMEMobjpool *pop, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	ringbuf_stop(pop->tx_postcommit_tasks);

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
