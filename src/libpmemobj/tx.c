/*
 * Copyright 2015-2017, Intel Corporation
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

#include "queue.h"
#include "ctree.h"
#include "obj.h"
#include "out.h"
#include "pmalloc.h"
#include "tx.h"
#include "valgrind_internal.h"

struct tx_data {
	SLIST_ENTRY(tx_data) tx_entry;
	jmp_buf env;
};

static __thread struct {
	enum pobj_tx_stage stage;
	int last_errnum;
	struct lane_section *section;

	pmemobj_tx_callback stage_callback;
	void *stage_callback_arg;
} tx;

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

#define RANGE_FLAGS_MIN_BIT 48
#define RANGE_FLAGS_MASK (0xffffULL << RANGE_FLAGS_MIN_BIT)

#define RANGE_GET_SIZE(val) ((val) & ~RANGE_FLAGS_MASK)

#define RANGE_FLAG_NO_FLUSH (0x1ULL << RANGE_FLAGS_MIN_BIT)

struct lane_tx_runtime {
	PMEMobjpool *pop;
	struct ctree *ranges;
	uint64_t cache_offset;
	struct tx_undo_runtime undo;
	SLIST_HEAD(txd, tx_data) tx_entries;
	SLIST_HEAD(txl, tx_lock_data) tx_locks;
};

struct tx_alloc_args {
	type_num_t type_num;
	uint64_t entry_offset;
	uint64_t flags;
};

struct tx_alloc_copy_args {
	struct tx_alloc_args super;
	size_t size;
	const void *ptr;
	size_t copy_size;
	uint64_t flags;
};

struct tx_add_range_args {
	PMEMobjpool *pop;
	uint64_t offset;
	uint64_t size;
	uint64_t flags;
};

/*
 * tx_clr_flag -- flags for clearing undo log list
 */
enum tx_clr_flag {
	TX_CLR_FLAG_FREE = 1 << 0, /* remove and free each object */
	TX_CLR_FLAG_VG_CLEAN = 1 << 1, /* clear valgrind state */
	TX_CLR_FLAG_VG_TX_REMOVE = 1 << 2, /* remove from valgrind tx */
};

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
#define ASSERT_IN_TX() do {\
	if (tx.stage == TX_STAGE_NONE)\
		FATAL("%s called outside of transaction", __func__);\
} while (0)

/* ASSERT_TX_STAGE_WORK -- checks whether current transaction stage is WORK */
#define ASSERT_TX_STAGE_WORK() do {\
	if (tx.stage != TX_STAGE_WORK)\
		FATAL("%s called in invalid stage %d", __func__, tx.stage);\
} while (0)

/*
 * constructor_tx_alloc -- (internal) constructor for normal alloc
 */
static int
constructor_tx_alloc(void *ctx, void *ptr, size_t usable_size, void *arg)
{
	LOG(3, NULL);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct tx_alloc_args *args = arg;

	/* do not report changes to the new object */
	VALGRIND_ADD_TO_TX(ptr, usable_size);

	if (args->flags & POBJ_FLAG_ZERO)
		memset(ptr, 0, usable_size);

	return 0;
}

/*
 * constructor_tx_copy -- (internal) copy constructor
 */
static int
constructor_tx_copy(void *ctx, void *ptr, size_t usable_size, void *arg)
{
	LOG(3, NULL);
	PMEMobjpool *pop = ctx;

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct tx_alloc_copy_args *args = arg;
	args->super.flags = args->flags;

	constructor_tx_alloc(pop, ptr, usable_size, &args->super);

	memcpy(ptr, args->ptr, args->copy_size);

	return 0;
}

/*
 * constructor_tx_add_range -- (internal) constructor for add_range
 */
static int
constructor_tx_add_range(void *ctx, void *ptr, size_t usable_size, void *arg)
{
	LOG(3, NULL);
	PMEMobjpool *pop = ctx;

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct tx_add_range_args *args = arg;
	struct tx_range *range = ptr;
	const struct pmem_ops *p_ops = &pop->p_ops;

	/* temporarily add the object copy to the transaction */
	VALGRIND_ADD_TO_TX(range, sizeof(struct tx_range) + args->size);

	range->offset = args->offset;
	range->size = args->size;

	void *src = OBJ_OFF_TO_PTR(args->pop, args->offset);

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
 * tx_clear_undo_log_vg -- (internal) tell Valgrind about removal from undo log
 */
static void
tx_clear_undo_log_vg(PMEMobjpool *pop, uint64_t off, enum tx_clr_flag flags)
{
#ifdef USE_VG_PMEMCHECK
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
tx_clear_undo_log(PMEMobjpool *pop, struct pvector_context *undo,
	enum tx_clr_flag flags)
{
	LOG(3, NULL);

	uint64_t val;

	while ((val = pvector_last(undo)) != 0) {
		tx_clear_undo_log_vg(pop, val, flags);

		if (flags & TX_CLR_FLAG_FREE) {
			pvector_pop_back(undo, tx_free_vec_entry);
		} else {
			pvector_pop_back(undo, tx_clear_vec_entry);
		}
	}
}

/*
 * tx_abort_alloc -- (internal) abort all allocated objects
 */
static void
tx_abort_alloc(PMEMobjpool *pop, struct tx_undo_runtime *tx_rt)
{
	LOG(3, NULL);

	tx_clear_undo_log(pop, tx_rt->ctx[UNDO_ALLOC],
		TX_CLR_FLAG_FREE |
		TX_CLR_FLAG_VG_CLEAN |
		TX_CLR_FLAG_VG_TX_REMOVE);
}

/*
 * tx_abort_free -- (internal) abort all freeing objects
 */
static void
tx_abort_free(PMEMobjpool *pop, struct tx_undo_runtime *tx_rt)
{
	LOG(3, NULL);

	tx_clear_undo_log(pop, tx_rt->ctx[UNDO_FREE], 0);
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
tx_restore_range(PMEMobjpool *pop, struct tx_range *range)
{
	COMPILE_ERROR_ON(sizeof(PMEMmutex) != _POBJ_CL_SIZE);
	COMPILE_ERROR_ON(sizeof(PMEMrwlock) != _POBJ_CL_SIZE);
	COMPILE_ERROR_ON(sizeof(PMEMcond) != _POBJ_CL_SIZE);

	struct lane_tx_runtime *runtime =
			(struct lane_tx_runtime *)tx.section->runtime;
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
	SLIST_FOREACH(txl, &(runtime->tx_locks), tx_lock) {
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
tx_foreach_set(PMEMobjpool *pop, struct tx_undo_runtime *tx_rt,
	void (*cb)(PMEMobjpool *pop, struct tx_range *range))
{
	LOG(3, NULL);

	struct tx_range *range = NULL;
	uint64_t off;
	struct pvector_context *ctx = tx_rt->ctx[UNDO_SET];
	for (off = pvector_first(ctx); off != 0; off = pvector_next(ctx)) {
		range = OBJ_OFF_TO_PTR(pop, off);
		cb(pop, range);
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

			cb(pop, range);
			cache_offset += TX_RANGE_ALIGN_SIZE(range->size) +
				sizeof(struct tx_range);
		}
	}
}

/*
 * tx_abort_restore_range -- (internal) restores content of the memory range
 */
static void
tx_abort_restore_range(PMEMobjpool *pop, struct tx_range *range)
{
	tx_restore_range(pop, range);
	VALGRIND_REMOVE_FROM_TX(OBJ_OFF_TO_PTR(pop, range->offset),
			range->size);
}

/*
 * tx_abort_recover_range -- (internal) restores content while skipping locks
 */
static void
tx_abort_recover_range(PMEMobjpool *pop, struct tx_range *range)
{
	void *ptr = OBJ_OFF_TO_PTR(pop, range->offset);
	pmemops_memcpy_persist(&pop->p_ops, ptr, range->data, range->size);
}

/*
 * tx_abort_set -- (internal) abort all set operations
 */
static void
tx_abort_set(PMEMobjpool *pop, struct tx_undo_runtime *tx_rt, int recovery)
{
	LOG(3, NULL);

	if (recovery)
		tx_foreach_set(pop, tx_rt, tx_abort_recover_range);
	else
		tx_foreach_set(pop, tx_rt, tx_abort_restore_range);

	tx_clear_undo_log(pop, tx_rt->ctx[UNDO_SET_CACHE],
		TX_CLR_FLAG_FREE | TX_CLR_FLAG_VG_CLEAN);
	tx_clear_undo_log(pop, tx_rt->ctx[UNDO_SET],
		TX_CLR_FLAG_FREE | TX_CLR_FLAG_VG_CLEAN);
}

/*
 * tx_post_commit_alloc -- (internal) do post commit operations for
 * allocated objects
 */
static void
tx_post_commit_alloc(PMEMobjpool *pop, struct tx_undo_runtime *tx_rt)
{
	LOG(3, NULL);

	tx_clear_undo_log(pop, tx_rt->ctx[UNDO_ALLOC],
			TX_CLR_FLAG_VG_TX_REMOVE);
}

/*
 * tx_post_commit_free -- (internal) do post commit operations for
 * freeing objects
 */
static void
tx_post_commit_free(PMEMobjpool *pop, struct tx_undo_runtime *tx_rt)
{
	LOG(3, NULL);

	tx_clear_undo_log(pop, tx_rt->ctx[UNDO_FREE],
		TX_CLR_FLAG_FREE | TX_CLR_FLAG_VG_TX_REMOVE);
}

#ifdef USE_VG_PMEMCHECK
/*
 * tx_post_commit_range_vg_tx_remove -- (internal) removes object from
 * transaction tracked by pmemcheck
 */
static void
tx_post_commit_range_vg_tx_remove(PMEMobjpool *pop, struct tx_range *range)
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
tx_post_commit_set(PMEMobjpool *pop, struct tx_undo_runtime *tx_rt,
		int recovery)
{
	LOG(3, NULL);

#ifdef USE_VG_PMEMCHECK
	if (On_valgrind)
		tx_foreach_set(pop, tx_rt, tx_post_commit_range_vg_tx_remove);
#endif

	struct pvector_context *cache_undo = tx_rt->ctx[UNDO_SET_CACHE];
	uint64_t first_cache = pvector_first(cache_undo);
	uint64_t off;

	int zero_all = recovery;

	while ((off = pvector_last(cache_undo)) != first_cache) {
		pvector_pop_back(cache_undo, tx_free_vec_entry);
		zero_all = 1;
	}

	if (first_cache != 0) {
		struct tx_range_cache *cache = OBJ_OFF_TO_PTR(pop, first_cache);

		size_t sz;
		if (zero_all) {
			sz = palloc_usable_size(&pop->heap, first_cache);
		} else {
			struct lane_tx_runtime *r = tx.section->runtime;
			sz = r->cache_offset;
		}

		if (sz) {
			VALGRIND_ADD_TO_TX(cache, sz);
			pmemops_memset_persist(&pop->p_ops, cache, 0, sz);
			VALGRIND_REMOVE_FROM_TX(cache, sz);
		}

#ifdef DEBUG
		if (!zero_all) { /* for recovery we know we zeroed everything */
			uint64_t usable_size = palloc_usable_size(&pop->heap,
				first_cache);
			ASSERTeq(util_is_zeroed(cache, usable_size), 1);
		}
#endif
	}

	tx_clear_undo_log(pop, tx_rt->ctx[UNDO_SET], TX_CLR_FLAG_FREE);
}

/*
 * tx_flush_range -- (internal) flush one range
 */
static void
tx_flush_range(uint64_t offset, uint64_t size_flags, void *ctx)
{
	if (size_flags & RANGE_FLAG_NO_FLUSH)
		return;
	PMEMobjpool *pop = ctx;
	pmemops_flush(&pop->p_ops, OBJ_OFF_TO_PTR(pop, offset),
			RANGE_GET_SIZE(size_flags));
}

/*
 * tx_pre_commit -- (internal) do pre-commit operations
 */
static void
tx_pre_commit(PMEMobjpool *pop, struct lane_tx_runtime *lane)
{
	LOG(3, NULL);

	ASSERTne(tx.section->runtime, NULL);

	/* Flush all regions and destroy the whole tree. */
	ctree_delete_cb(lane->ranges, tx_flush_range, pop);
	lane->ranges = NULL;
}

/*
 * tx_rebuild_undo_runtime -- (internal) reinitializes runtime state of vectors
 */
static int
tx_rebuild_undo_runtime(PMEMobjpool *pop, struct lane_tx_layout *layout,
	struct tx_undo_runtime *tx_rt)
{
	LOG(3, NULL);

	int i;
	for (i = UNDO_ALLOC; i < MAX_UNDO_TYPES; ++i) {
		if (tx_rt->ctx[i] == NULL)
			tx_rt->ctx[i] = pvector_new(pop, &layout->undo_log[i]);

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
	LOG(3, NULL);

	for (int i = UNDO_ALLOC; i < MAX_UNDO_TYPES; ++i)
		pvector_delete(tx->ctx[i]);
}

/*
 * tx_post_commit -- (internal) do post commit operations
 */
static void
tx_post_commit(PMEMobjpool *pop, struct lane_tx_layout *layout, int recovery)
{
	LOG(3, NULL);

	struct tx_undo_runtime *tx_rt;
	struct tx_undo_runtime new_rt = { .ctx = {NULL, } };
	if (recovery) {
		if (tx_rebuild_undo_runtime(pop, layout, &new_rt) != 0)
			FATAL("!Cannot rebuild runtime undo log state");

		tx_rt = &new_rt;
	} else {
		struct lane_tx_runtime *lane = tx.section->runtime;
		tx_rt = &lane->undo;
	}

	tx_post_commit_set(pop, tx_rt, recovery);
	tx_post_commit_alloc(pop, tx_rt);
	tx_post_commit_free(pop, tx_rt);

	if (recovery)
		tx_destroy_undo_runtime(tx_rt);
}

#ifdef USE_VG_MEMCHECK
/*
 * tx_abort_register_valgrind -- tells Valgrind about objects from specified
 *				 undo log
 */
static void
tx_abort_register_valgrind(PMEMobjpool *pop, struct pvector_context *ctx)
{
	uint64_t off;
	for (off = pvector_first(ctx); off != 0; off = pvector_next(ctx)) {
		/*
		 * Can't use pmemobj_direct and pmemobj_alloc_usable_size
		 * because pool has not been registered yet.
		 */
		void *p = (char *)pop + off;
		size_t sz = palloc_usable_size(&pop->heap, off);

		VALGRIND_DO_MEMPOOL_ALLOC(pop->heap.layout, p, sz);
		VALGRIND_DO_MAKE_MEM_DEFINED(p, sz);
	}
}
#endif

/*
 * tx_abort -- (internal) abort all allocated objects
 */
static void
tx_abort(PMEMobjpool *pop, struct lane_tx_runtime *lane,
		struct lane_tx_layout *layout, int recovery)
{
	LOG(3, NULL);

	struct tx_undo_runtime *tx_rt;
	struct tx_undo_runtime new_rt = { .ctx = {NULL, } };
	if (recovery) {
		if (tx_rebuild_undo_runtime(pop, layout, &new_rt) != 0)
			FATAL("!Cannot rebuild runtime undo log state");

		tx_rt = &new_rt;
	} else {
		tx_rt = &lane->undo;
	}

#ifdef USE_VG_MEMCHECK
	if (recovery && On_valgrind) {
		tx_abort_register_valgrind(pop, tx_rt->ctx[UNDO_SET]);
		tx_abort_register_valgrind(pop, tx_rt->ctx[UNDO_ALLOC]);
		tx_abort_register_valgrind(pop, tx_rt->ctx[UNDO_SET_CACHE]);
	}
#endif

	tx_abort_set(pop, tx_rt, recovery);
	tx_abort_alloc(pop, tx_rt);
	tx_abort_free(pop, tx_rt);

	if (recovery) {
		tx_destroy_undo_runtime(tx_rt);
	} else {
		ASSERTne(lane, NULL);
		ctree_delete(lane->ranges);
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
	if (tx.stage == TX_STAGE_NONE)
		return NULL;

	struct lane_tx_runtime *lane =
			(struct lane_tx_runtime *)tx.section->runtime;

	return lane->pop;
}

/*
 * add_to_tx_and_lock -- (internal) add lock to the transaction and acquire it
 */
static int
add_to_tx_and_lock(struct lane_tx_runtime *lane, enum pobj_tx_param type,
	void *lock)
{
	LOG(15, NULL);
	int retval = 0;
	struct tx_lock_data *txl;
	/* check if the lock is already on the list */
	SLIST_FOREACH(txl, &(lane->tx_locks), tx_lock) {
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
			retval = pmemobj_mutex_lock(lane->pop,
				txl->lock.mutex);
			if (retval) {
				errno = retval;
				ERR("!pmemobj_mutex_lock");
			}
			break;
		case TX_PARAM_RWLOCK:
			txl->lock.rwlock = lock;
			retval = pmemobj_rwlock_wrlock(lane->pop,
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

	SLIST_INSERT_HEAD(&lane->tx_locks, txl, tx_lock);

	return retval;
}

/*
 * release_and_free_tx_locks -- (internal) release and remove all locks from the
 *				transaction
 */
static void
release_and_free_tx_locks(struct lane_tx_runtime *lane)
{
	LOG(15, NULL);

	while (!SLIST_EMPTY(&lane->tx_locks)) {
		struct tx_lock_data *tx_lock = SLIST_FIRST(&lane->tx_locks);
		SLIST_REMOVE_HEAD(&lane->tx_locks, tx_lock);
		switch (tx_lock->lock_type) {
			case TX_PARAM_MUTEX:
				pmemobj_mutex_unlock(lane->pop,
					tx_lock->lock.mutex);
				break;
			case TX_PARAM_RWLOCK:
				pmemobj_rwlock_unlock(lane->pop,
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
 * tx_alloc_common -- (internal) common function for alloc and zalloc
 */
static PMEMoid
tx_alloc_common(size_t size, type_num_t type_num, palloc_constr constructor,
		uint64_t flags)
{
	LOG(3, NULL);

	if (size > PMEMOBJ_MAX_ALLOC_SIZE) {
		ERR("requested size too large");
		return obj_tx_abort_null(ENOMEM);
	}

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx.section->runtime;

	uint64_t *entry_offset = pvector_push_back(lane->undo.ctx[UNDO_ALLOC]);
	if (entry_offset == NULL) {
		ERR("allocation undo log too large");
		return obj_tx_abort_null(ENOMEM);
	}

	struct tx_alloc_args args = {
		.type_num = type_num,
		.entry_offset = (uint64_t)entry_offset,
		.flags = flags,
	};

	/* allocate object to undo log */
	PMEMoid retoid = OID_NULL;
	PMEMobjpool *pop = lane->pop;

	pmalloc_construct(pop, entry_offset, size, constructor, &args,
		type_num, 0);

	retoid.off = *entry_offset;
	retoid.pool_uuid_lo = pop->uuid_lo;

	if (OBJ_OID_IS_NULL(retoid))
		goto err_oom;

	uint64_t range_flags = (flags & POBJ_FLAG_NO_FLUSH) ?
			RANGE_FLAG_NO_FLUSH : 0;
	size = palloc_usable_size(&pop->heap, retoid.off);
	ASSERTeq(size & RANGE_FLAGS_MASK, 0);

	if (ctree_insert_unlocked(lane->ranges, retoid.off,
			size | range_flags) != 0)
		goto err_oom;

	return retoid;

err_oom:
	pvector_pop_back(lane->undo.ctx[UNDO_ALLOC], NULL);

	ERR("out of memory");
	return obj_tx_abort_null(ENOMEM);
}

/*
 * tx_alloc_copy_common -- (internal) common function for alloc with data copy
 */
static PMEMoid
tx_alloc_copy_common(size_t size, type_num_t type_num, const void *ptr,
	size_t copy_size, palloc_constr constructor, uint64_t flags)
{
	LOG(3, NULL);

	if (size > PMEMOBJ_MAX_ALLOC_SIZE) {
		ERR("requested size too large");
		return obj_tx_abort_null(ENOMEM);
	}

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx.section->runtime;

	uint64_t *entry_offset = pvector_push_back(lane->undo.ctx[UNDO_ALLOC]);
	if (entry_offset == NULL) {
		ERR("allocation undo log too large");
		return obj_tx_abort_null(ENOMEM);
	}

	struct tx_alloc_copy_args args = {
		.super = {
			.type_num = type_num,
			.entry_offset = (uint64_t)entry_offset,
		},
		.size = size,
		.ptr = ptr,
		.copy_size = copy_size,
		.flags = flags,
	};

	/* allocate object to undo log */
	PMEMoid retoid;
	PMEMobjpool *pop = lane->pop;
	int ret = pmalloc_construct(pop, entry_offset, size,
			constructor, &args, type_num, 0);

	retoid.off = *entry_offset;
	retoid.pool_uuid_lo = pop->uuid_lo;

	if (ret || OBJ_OID_IS_NULL(retoid))
		goto err_oom;

	size = palloc_usable_size(&pop->heap, retoid.off);
	if (ctree_insert_unlocked(lane->ranges, retoid.off, size) != 0)
		goto err_oom;

	return retoid;

err_oom:
	pvector_pop_back(lane->undo.ctx[UNDO_ALLOC], NULL);

	ERR("out of memory");
	return obj_tx_abort_null(ENOMEM);
}

/*
 * tx_realloc_common -- (internal) common function for tx realloc
 */
static PMEMoid
tx_realloc_common(PMEMoid oid, size_t size, uint64_t type_num,
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
		(struct lane_tx_runtime *)tx.section->runtime;

	/* if oid is NULL just alloc */
	if (OBJ_OID_IS_NULL(oid))
		return tx_alloc_common(size, (type_num_t)type_num,
				constructor_alloc, flags);

	ASSERT(OBJ_OID_IS_VALID(lane->pop, oid));

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
	void *ptr = OBJ_OFF_TO_PTR(lane->pop, oid.off);
	size_t old_size = palloc_usable_size(&lane->pop->heap, oid.off);

	size_t copy_size = old_size < size ? old_size : size;

	PMEMoid new_obj = tx_alloc_copy_common(size, (type_num_t)type_num,
			ptr, copy_size, constructor_realloc, flags);

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

	struct lane_tx_runtime *lane = NULL;
	if (tx.stage == TX_STAGE_WORK) {
		lane = tx.section->runtime;
		if (lane->pop != pop) {
			ERR("nested transaction for different pool");
			return obj_tx_abort_err(EINVAL);
		}

		VALGRIND_START_TX;
	} else if (tx.stage == TX_STAGE_NONE) {
		VALGRIND_START_TX;

		lane_hold(pop, &tx.section, LANE_SECTION_TRANSACTION);

		lane = tx.section->runtime;
		SLIST_INIT(&lane->tx_entries);
		SLIST_INIT(&lane->tx_locks);
		lane->ranges = ctree_new();
		lane->cache_offset = 0;

		struct lane_tx_layout *layout =
			(struct lane_tx_layout *)tx.section->layout;

		if (tx_rebuild_undo_runtime(pop, layout, &lane->undo) != 0) {
			tx.stage = TX_STAGE_ONABORT;
			err = errno;
			return err;
		}

		lane->pop = pop;
	} else {
		FATAL("Invalid stage %d to begin new transaction", tx.stage);
	}

	struct tx_data *txd = Malloc(sizeof(*txd));
	if (txd == NULL) {
		err = errno;
		ERR("!Malloc");
		goto err_abort;
	}

	tx.last_errnum = 0;
	if (env != NULL)
		memcpy(txd->env, env, sizeof(jmp_buf));
	else
		memset(txd->env, 0, sizeof(jmp_buf));

	SLIST_INSERT_HEAD(&lane->tx_entries, txd, tx_entry);

	tx.stage = TX_STAGE_WORK;

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

			if (tx.stage_callback &&
					(tx.stage_callback != cb ||
					tx.stage_callback_arg != arg)) {
				FATAL("transaction callback is already set, "
					"old %p new %p old_arg %p new_arg %p",
					tx.stage_callback, cb,
					tx.stage_callback_arg, arg);
			}

			tx.stage_callback = cb;
			tx.stage_callback_arg = arg;
		} else {
			err = add_to_tx_and_lock(lane, param_type,
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
	if (tx.stage == TX_STAGE_WORK)
		obj_tx_abort(err, 0);
	else
		tx.stage = TX_STAGE_ONABORT;
	return err;
}

/*
 * pmemobj_tx_lock -- get lane from pool and add lock to transaction.
 */
int
pmemobj_tx_lock(enum pobj_tx_param type, void *lockp)
{
	ASSERT_IN_TX();
	ASSERT_TX_STAGE_WORK();

	struct lane_tx_runtime *lane = tx.section->runtime;

	return add_to_tx_and_lock(lane, type, lockp);
}

/*
 * obj_tx_callback -- (internal) executes callback associated with current stage
 */
static void
obj_tx_callback(void)
{
	if (!tx.stage_callback)
		return;

	struct lane_tx_runtime *lane = tx.section->runtime;
	struct tx_data *txd = SLIST_FIRST(&lane->tx_entries);

	/* is this the outermost transaction? */
	if (SLIST_NEXT(txd, tx_entry) == NULL)
		tx.stage_callback(lane->pop, tx.stage, tx.stage_callback_arg);
}

/*
 * pmemobj_tx_stage -- returns current transaction stage
 */
enum pobj_tx_stage
pmemobj_tx_stage(void)
{
	LOG(3, NULL);

	return tx.stage;
}

/*
 * obj_tx_abort -- aborts current transaction
 */
static void
obj_tx_abort(int errnum, int user)
{
	LOG(3, NULL);

	ASSERT_IN_TX();
	ASSERT_TX_STAGE_WORK();

	ASSERT(tx.section != NULL);

	if (errnum == 0)
		errnum = ECANCELED;

	tx.stage = TX_STAGE_ONABORT;
	struct lane_tx_runtime *lane = tx.section->runtime;
	struct tx_data *txd = SLIST_FIRST(&lane->tx_entries);

	if (SLIST_NEXT(txd, tx_entry) == NULL) {
		/* this is the outermost transaction */

		struct lane_tx_layout *layout =
				(struct lane_tx_layout *)tx.section->layout;

		/* process the undo log */
		tx_abort(lane->pop, lane, layout, 0 /* abort */);
	}

	tx.last_errnum = errnum;
	errno = errnum;
	if (user)
		ERR("!explicit transaction abort");

	/* ONABORT */
	obj_tx_callback();

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

	return tx.last_errnum;
}

/*
 * pmemobj_tx_commit -- commits current transaction
 */
void
pmemobj_tx_commit(void)
{
	LOG(3, NULL);

	ASSERT_IN_TX();
	ASSERT_TX_STAGE_WORK();

	/* WORK */
	obj_tx_callback();

	ASSERT(tx.section != NULL);

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx.section->runtime;
	struct tx_data *txd = SLIST_FIRST(&lane->tx_entries);

	if (SLIST_NEXT(txd, tx_entry) == NULL) {
		/* this is the outermost transaction */

		struct lane_tx_layout *layout =
			(struct lane_tx_layout *)tx.section->layout;
		PMEMobjpool *pop = lane->pop;

		/* pre-commit phase */
		tx_pre_commit(pop, lane);

		pmemops_drain(&pop->p_ops);

		/* set transaction state as committed */
		tx_set_state(pop, layout, TX_STATE_COMMITTED);

		/* post commit phase */
		tx_post_commit(pop, layout, 0 /* not recovery */);

		/* clear transaction state */
		tx_set_state(pop, layout, TX_STATE_NONE);
	}

	tx.stage = TX_STAGE_ONCOMMIT;

	/* ONCOMMIT */
	obj_tx_callback();
}

/*
 * pmemobj_tx_end -- ends current transaction
 */
int
pmemobj_tx_end(void)
{
	LOG(3, NULL);

	if (tx.stage == TX_STAGE_WORK)
		FATAL("pmemobj_tx_end called without pmemobj_tx_commit");

	if (tx.section == NULL)
		FATAL("pmemobj_tx_end called without pmemobj_tx_begin");

	if (tx.stage_callback &&
			(tx.stage == TX_STAGE_ONCOMMIT ||
			tx.stage == TX_STAGE_ONABORT)) {
		tx.stage = TX_STAGE_FINALLY;
		obj_tx_callback();
	}

	struct lane_tx_runtime *lane = tx.section->runtime;
	struct tx_data *txd = SLIST_FIRST(&lane->tx_entries);
	SLIST_REMOVE_HEAD(&lane->tx_entries, tx_entry);

	Free(txd);

	VALGRIND_END_TX;

	if (SLIST_EMPTY(&lane->tx_entries)) {
		/* this is the outermost transaction */
		struct lane_tx_layout *layout =
			(struct lane_tx_layout *)tx.section->layout;

		/* cleanup cache */
		lane->cache_offset = 0;

		/* the transaction state and undo log should be clear */
		ASSERTeq(layout->state, TX_STATE_NONE);
		if (layout->state != TX_STATE_NONE)
			LOG(2, "invalid transaction state");

		ASSERTeq(pvector_nvalues(lane->undo.ctx[UNDO_ALLOC]), 0);
		ASSERTeq(pvector_nvalues(lane->undo.ctx[UNDO_SET]), 0);
		ASSERTeq(pvector_nvalues(lane->undo.ctx[UNDO_FREE]), 0);
		ASSERT(pvector_nvalues(lane->undo.ctx[UNDO_FREE]) == 0 ||
			pvector_nvalues(lane->undo.ctx[UNDO_FREE]) == 1);

		tx.stage = TX_STAGE_NONE;
		release_and_free_tx_locks(lane);
		PMEMobjpool *pop = lane->pop;
		lane_release(pop);
		tx.section = NULL;

		if (tx.stage_callback) {
			pmemobj_tx_callback cb = tx.stage_callback;
			void *arg = tx.stage_callback_arg;

			tx.stage_callback = NULL;
			tx.stage_callback_arg = NULL;

			cb(pop, TX_STAGE_NONE, arg);
		}
	} else {
		/* resume the next transaction */
		tx.stage = TX_STAGE_WORK;

		/* abort called within inner transaction, waterfall the error */
		if (tx.last_errnum)
			obj_tx_abort(tx.last_errnum, 0);
	}

	return tx.last_errnum;
}

/*
 * pmemobj_tx_process -- processes current transaction stage
 */
void
pmemobj_tx_process(void)
{
	LOG(3, NULL);

	ASSERT_IN_TX();
	ASSERTne(tx.section, NULL);

	switch (tx.stage) {
	case TX_STAGE_NONE:
		break;
	case TX_STAGE_WORK:
		pmemobj_tx_commit();
		break;
	case TX_STAGE_ONABORT:
	case TX_STAGE_ONCOMMIT:
		tx.stage = TX_STAGE_FINALLY;
		obj_tx_callback();
		break;
	case TX_STAGE_FINALLY:
		tx.stage = TX_STAGE_NONE;
		break;
	case MAX_TX_STAGE:
		ASSERT(0);
	}
}

/*
 * pmemobj_tx_add_large -- (internal) adds large memory range to undo log
 */
static int
pmemobj_tx_add_large(struct tx_add_range_args *args)
{
	struct lane_tx_runtime *runtime = tx.section->runtime;
	struct pvector_context *undo = runtime->undo.ctx[UNDO_SET];
	uint64_t *entry = pvector_push_back(undo);
	if (entry == NULL) {
		ERR("large set undo log too large");
		return -1;
	}

	/* insert snapshot to undo log */
	int ret = pmalloc_construct(args->pop, entry,
			args->size + sizeof(struct tx_range),
			constructor_tx_add_range, args,
			0, OBJ_INTERNAL_OBJECT_MASK);

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
	LOG(3, NULL);
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
pmemobj_tx_get_range_cache(PMEMobjpool *pop,
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

	struct lane_tx_runtime *runtime = tx.section->runtime;

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
			TX_RANGE_CACHE_SIZE,
			constructor_tx_range_cache, NULL,
			0, OBJ_INTERNAL_OBJECT_MASK);

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
pmemobj_tx_add_small(struct tx_add_range_args *args)
{
	PMEMobjpool *pop = args->pop;

	struct lane_tx_runtime *runtime = tx.section->runtime;
	struct pvector_context *undo = runtime->undo.ctx[UNDO_SET_CACHE];
	const struct pmem_ops *p_ops = &pop->p_ops;

	uint64_t remaining_space;
	struct tx_range_cache *cache = pmemobj_tx_get_range_cache(pop,
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
	uint64_t range_size = TX_RANGE_ALIGN_SIZE(args->size) +
		sizeof(struct tx_range);

	if (remaining_space < range_size) {
		ASSERT(remaining_space > sizeof(struct tx_range));
		range_size = remaining_space;
		data_size = remaining_space - sizeof(struct tx_range);

		args->offset += range_size;
		args->size -= range_size;
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

	if (args->size != 0) {
		pmemobj_tx_add_small(args);
	}

	return 0;
}

/*
 * pmemobj_tx_add_common -- (internal) common code for adding persistent memory
 *				into the transaction
 */
static int
pmemobj_tx_add_common(struct tx_add_range_args *args)
{
	LOG(15, NULL);

	if (args->size > PMEMOBJ_MAX_ALLOC_SIZE) {
		ERR("snapshot size too large");
		return obj_tx_abort_err(EINVAL);
	}

	if (args->offset < args->pop->heap_offset ||
		(args->offset + args->size) >
		(args->pop->heap_offset + args->pop->heap_size)) {
		ERR("object outside of heap");
		return obj_tx_abort_err(EINVAL);
	}

	struct lane_tx_runtime *runtime = tx.section->runtime;

	/* starting from the end, search for all overlapping ranges */
	uint64_t spoint = args->offset + args->size - 1; /* start point */
	uint64_t apoint = 0; /* add point */
	int ret = 0;
	uint64_t range_flags = (args->flags & POBJ_FLAG_NO_FLUSH) ?
			RANGE_FLAG_NO_FLUSH : 0;

	while (spoint >= args->offset) {
		apoint = spoint + 1;
		/* find range less than starting point */
		uint64_t size_flags = ctree_find_le_unlocked(runtime->ranges,
				&spoint);
		uint64_t size = RANGE_GET_SIZE(size_flags);
		struct tx_add_range_args nargs;
		nargs.pop = args->pop;

		if (spoint < args->offset) { /* the found offset is earlier */
			nargs.size = apoint - args->offset;
			/* overlap on the left edge */
			if (spoint + size > args->offset) {
				nargs.offset = spoint + size;
				if (nargs.size <= nargs.offset - args->offset)
					break;
				nargs.size -= nargs.offset - args->offset;
			} else {
				nargs.offset = args->offset;
			}

			if (args->size == 0)
				break;

			spoint = 0; /* this is the end of our search */
		} else { /* found offset is equal or greater than offset */
			nargs.offset = spoint + size;
			spoint -= 1;
			if (nargs.offset >= apoint)
				continue;

			nargs.size = apoint - nargs.offset;
		}

		ret = ctree_insert_unlocked(runtime->ranges, nargs.offset,
				nargs.size | range_flags);
		if (ret != 0) {
			if (ret == EEXIST)
				FATAL("invalid state of ranges tree");

			break;
		}

		/*
		 * Depending on the size of the block, either allocate an
		 * entire new object or use cache.
		 */
		ret = nargs.size > TX_RANGE_CACHE_THRESHOLD ?
			pmemobj_tx_add_large(&nargs) :
			pmemobj_tx_add_small(&nargs);

		if (ret != 0)
			break;
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

	ASSERT_IN_TX();
	ASSERT_TX_STAGE_WORK();

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx.section->runtime;

	if ((char *)ptr < (char *)lane->pop ||
			(char *)ptr >= (char *)lane->pop + lane->pop->size) {
		ERR("object outside of pool");
		return obj_tx_abort_err(EINVAL);
	}

	struct tx_add_range_args args = {
		.pop = lane->pop,
		.offset = (uint64_t)((char *)ptr - (char *)lane->pop),
		.size = size,
		.flags = 0,
	};

	return pmemobj_tx_add_common(&args);
}

/*
 * pmemobj_tx_xadd_range_direct -- adds persistent memory range into the
 *					transaction
 */
int
pmemobj_tx_xadd_range_direct(const void *ptr, size_t size, uint64_t flags)
{
	LOG(3, NULL);

	ASSERT_IN_TX();
	ASSERT_TX_STAGE_WORK();

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx.section->runtime;

	if (!OBJ_PTR_FROM_POOL(lane->pop, ptr)) {
		ERR("object outside of pool");
		return obj_tx_abort_err(EINVAL);
	}

	if (flags & ~POBJ_XADD_VALID_FLAGS) {
		ERR("unknown flags 0x%" PRIx64, flags & ~POBJ_XADD_VALID_FLAGS);
		return obj_tx_abort_err(EINVAL);
	}

	struct tx_add_range_args args = {
		.pop = lane->pop,
		.offset = (uint64_t)((char *)ptr - (char *)lane->pop),
		.size = size,
		.flags = flags,
	};

	return pmemobj_tx_add_common(&args);
}

/*
 * pmemobj_tx_add_range -- adds persistent memory range into the transaction
 */
int
pmemobj_tx_add_range(PMEMoid oid, uint64_t hoff, size_t size)
{
	LOG(3, NULL);

	ASSERT_IN_TX();
	ASSERT_TX_STAGE_WORK();

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx.section->runtime;

	if (oid.pool_uuid_lo != lane->pop->uuid_lo) {
		ERR("invalid pool uuid");
		return obj_tx_abort_err(EINVAL);
	}
	ASSERT(OBJ_OID_IS_VALID(lane->pop, oid));

	struct tx_add_range_args args = {
		.pop = lane->pop,
		.offset = oid.off + hoff,
		.size = size,
		.flags = 0,
	};

	return pmemobj_tx_add_common(&args);
}

/*
 * pmemobj_tx_xadd_range -- adds persistent memory range into the transaction
 */
int
pmemobj_tx_xadd_range(PMEMoid oid, uint64_t hoff, size_t size, uint64_t flags)
{
	LOG(3, NULL);

	ASSERT_IN_TX();
	ASSERT_TX_STAGE_WORK();

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx.section->runtime;

	if (oid.pool_uuid_lo != lane->pop->uuid_lo) {
		ERR("invalid pool uuid");
		return obj_tx_abort_err(EINVAL);
	}
	ASSERT(OBJ_OID_IS_VALID(lane->pop, oid));

	if (flags & ~POBJ_XADD_VALID_FLAGS) {
		ERR("unknown flags 0x%" PRIx64, flags & ~POBJ_XADD_VALID_FLAGS);
		return obj_tx_abort_err(EINVAL);
	}

	struct tx_add_range_args args = {
		.pop = lane->pop,
		.offset = oid.off + hoff,
		.size = size,
		.flags = flags,
	};

	return pmemobj_tx_add_common(&args);
}

/*
 * pmemobj_tx_alloc -- allocates a new object
 */
PMEMoid
pmemobj_tx_alloc(size_t size, uint64_t type_num)
{
	LOG(3, NULL);

	ASSERT_IN_TX();
	ASSERT_TX_STAGE_WORK();

	if (size == 0) {
		ERR("allocation with size 0");
		return obj_tx_abort_null(EINVAL);
	}

	return tx_alloc_common(size, (type_num_t)type_num,
			constructor_tx_alloc, 0);
}

/*
 * pmemobj_tx_zalloc -- allocates a new zeroed object
 */
PMEMoid
pmemobj_tx_zalloc(size_t size, uint64_t type_num)
{
	LOG(3, NULL);

	ASSERT_IN_TX();
	ASSERT_TX_STAGE_WORK();

	if (size == 0) {
		ERR("allocation with size 0");
		return obj_tx_abort_null(EINVAL);
	}

	return tx_alloc_common(size, (type_num_t)type_num,
			constructor_tx_alloc, POBJ_FLAG_ZERO);
}

/*
 * pmemobj_tx_xalloc -- allocates a new object
 */
PMEMoid
pmemobj_tx_xalloc(size_t size, uint64_t type_num, uint64_t flags)
{
	LOG(3, NULL);

	ASSERT_IN_TX();
	ASSERT_TX_STAGE_WORK();

	if (size == 0) {
		ERR("allocation with size 0");
		return obj_tx_abort_null(EINVAL);
	}

	if (flags & ~POBJ_XALLOC_VALID_FLAGS) {
		ERR("unknown flags 0x%" PRIx64,
				flags & ~POBJ_XALLOC_VALID_FLAGS);
		return obj_tx_abort_null(EINVAL);
	}

	return tx_alloc_common(size, (type_num_t)type_num,
			constructor_tx_alloc, flags);
}

/*
 * pmemobj_tx_realloc -- resizes an existing object
 */
PMEMoid
pmemobj_tx_realloc(PMEMoid oid, size_t size, uint64_t type_num)
{
	LOG(3, NULL);

	ASSERT_IN_TX();
	ASSERT_TX_STAGE_WORK();

	return tx_realloc_common(oid, size, type_num,
			constructor_tx_alloc, constructor_tx_copy, 0);
}


/*
 * pmemobj_zrealloc -- resizes an existing object, any new space is zeroed.
 */
PMEMoid
pmemobj_tx_zrealloc(PMEMoid oid, size_t size, uint64_t type_num)
{
	LOG(3, NULL);

	ASSERT_IN_TX();
	ASSERT_TX_STAGE_WORK();

	return tx_realloc_common(oid, size, type_num,
			constructor_tx_alloc, constructor_tx_copy,
			POBJ_FLAG_ZERO);
}

/*
 * pmemobj_tx_strdup -- allocates a new object with duplicate of the string s.
 */
PMEMoid
pmemobj_tx_strdup(const char *s, uint64_t type_num)
{
	LOG(3, NULL);

	ASSERT_IN_TX();
	ASSERT_TX_STAGE_WORK();

	if (NULL == s) {
		ERR("cannot duplicate NULL string");
		return obj_tx_abort_null(EINVAL);
	}

	size_t len = strlen(s);

	if (len == 0)
		return tx_alloc_common(sizeof(char), (type_num_t)type_num,
				constructor_tx_alloc, POBJ_FLAG_ZERO);

	size_t size = (len + 1) * sizeof(char);

	return tx_alloc_copy_common(size, (type_num_t)type_num, s, size,
			constructor_tx_copy, 0);
}

/*
 * pmemobj_tx_free -- frees an existing object
 */
int
pmemobj_tx_free(PMEMoid oid)
{
	LOG(3, NULL);

	ASSERT_IN_TX();
	ASSERT_TX_STAGE_WORK();

	if (OBJ_OID_IS_NULL(oid))
		return 0;

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx.section->runtime;
	PMEMobjpool *pop = lane->pop;

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
		tx_post_commit(pop, layout, 1 /* recovery */);
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
 * lane_transaction_boot -- initializes transaction section
 */
static int
lane_transaction_boot(PMEMobjpool *pop)
{
	/* NOP */
	return 0;
}

static struct section_operations transaction_ops = {
	.construct_rt = lane_transaction_construct_rt,
	.destroy_rt = lane_transaction_destroy_rt,
	.recover = lane_transaction_recovery,
	.check = lane_transaction_check,
	.boot = lane_transaction_boot
};

SECTION_PARM(LANE_SECTION_TRANSACTION, &transaction_ops);
