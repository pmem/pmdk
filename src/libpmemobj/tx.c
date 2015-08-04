/*
 * Copyright (c) 2015, Intel Corporation
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY LOG OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * tx.c -- transactions implementation
 */

#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <stdlib.h>

#include "libpmem.h"
#include "libpmemobj.h"
#include "util.h"
#include "lane.h"
#include "redo.h"
#include "list.h"
#include "obj.h"
#include "out.h"
#include "pmalloc.h"
#include "valgrind_internal.h"

struct tx_data {
	SLIST_ENTRY(tx_data) tx_entry;
	jmp_buf env;
	int errnum;
};

static __thread struct {
	enum pobj_tx_stage stage;
	struct lane_section *section;
} tx;

struct tx_lock_data {
	union {
		PMEMmutex *mutex;
		PMEMrwlock *rwlock;
	} lock;
	enum pobj_tx_lock lock_type;
	SLIST_ENTRY(tx_lock_data) tx_lock;
};

struct lane_tx_runtime {
	PMEMobjpool *pop;
	SLIST_HEAD(txd, tx_data) tx_entries;
	SLIST_HEAD(txl, tx_lock_data) tx_locks;
};

struct tx_alloc_args {
	unsigned int type_num;
	size_t size;
};

struct tx_alloc_copy_args {
	unsigned int type_num;
	size_t size;
	const void *ptr;
	size_t copy_size;
};

struct tx_add_range_args {
	PMEMobjpool *pop;
	uint64_t offset;
	uint64_t size;
};

/*
 * constructor_tx_alloc -- (internal) constructor for normal alloc
 */
static void
constructor_tx_alloc(PMEMobjpool *pop, void *ptr, void *arg)
{
	LOG(3, NULL);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct tx_alloc_args *args = arg;

	struct oob_header *oobh = OOB_HEADER_FROM_PTR(ptr);

	/* temporarily add the OOB header */
	VALGRIND_ADD_TO_TX(oobh, OBJ_OOB_SIZE);

	/*
	 * no need to flush and persist because this
	 * will be done in pre-commit phase
	 */
	oobh->internal_type = TYPE_NONE;
	oobh->user_type = args->type_num;

	VALGRIND_REMOVE_FROM_TX(oobh, OBJ_OOB_SIZE);

	/* do not report changes to the new object */
	VALGRIND_ADD_TO_TX(ptr, args->size);
}

/*
 * constructor_tx_zalloc -- (internal) constructor for zalloc
 */
static void
constructor_tx_zalloc(PMEMobjpool *pop, void *ptr, void *arg)
{
	LOG(3, NULL);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct tx_alloc_args *args = arg;

	struct oob_header *oobh = OOB_HEADER_FROM_PTR(ptr);

	/* temporarily add the OOB header */
	VALGRIND_ADD_TO_TX(oobh, OBJ_OOB_SIZE);

	/*
	 * no need to flush and persist because this
	 * will be done in pre-commit phase
	 */
	oobh->internal_type = TYPE_NONE;
	oobh->user_type = args->type_num;

	VALGRIND_REMOVE_FROM_TX(oobh, OBJ_OOB_SIZE);

	/* do not report changes to the new object */
	VALGRIND_ADD_TO_TX(ptr, args->size);

	memset(ptr, 0, args->size);
}

/*
 * constructor_tx_add_range -- (internal) constructor for add_range
 */
static void
constructor_tx_add_range(PMEMobjpool *pop, void *ptr, void *arg)
{
	LOG(3, NULL);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct tx_add_range_args *args = arg;
	struct tx_range *range = ptr;

	/* temporarily add the object copy to the transaction */
	VALGRIND_ADD_TO_TX(OOB_HEADER_FROM_PTR(ptr),
				sizeof (struct tx_range) + args->size
				+ OBJ_OOB_SIZE);

	range->offset = args->offset;
	range->size = args->size;

	void *src = OBJ_OFF_TO_PTR(args->pop, args->offset);

	/* flush offset and size */
	pop->flush(range, sizeof (struct tx_range));
	/* memcpy data and persist */
	pop->memcpy_persist(range->data, src, args->size);

	VALGRIND_REMOVE_FROM_TX(OOB_HEADER_FROM_PTR(ptr),
				sizeof (struct tx_range) + args->size
				+ OBJ_OOB_SIZE);

	/* do not report changes to the original object */
	VALGRIND_ADD_TO_TX(src, args->size);
}

/*
 * constructor_tx_copy -- (internal) copy constructor
 */
static void
constructor_tx_copy(PMEMobjpool *pop, void *ptr, void *arg)
{
	LOG(3, NULL);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct tx_alloc_copy_args *args = arg;
	struct oob_header *oobh = OOB_HEADER_FROM_PTR(ptr);

	/* temporarily add the OOB header */
	VALGRIND_ADD_TO_TX(oobh, OBJ_OOB_SIZE);

	/*
	 * no need to flush and persist because this
	 * will be done in pre-commit phase
	 */
	oobh->internal_type = TYPE_NONE;
	oobh->user_type = args->type_num;

	VALGRIND_REMOVE_FROM_TX(oobh, OBJ_OOB_SIZE);

	/* do not report changes made to the copy */
	VALGRIND_ADD_TO_TX(ptr, args->size);

	memcpy(ptr, args->ptr, args->copy_size);
}

/*
 * constructor_tx_copy_zero -- (internal) copy constructor which zeroes
 * the non-copied area
 */
static void
constructor_tx_copy_zero(PMEMobjpool *pop, void *ptr, void *arg)
{
	LOG(3, NULL);

	ASSERTne(ptr, NULL);
	ASSERTne(arg, NULL);

	struct tx_alloc_copy_args *args = arg;
	struct oob_header *oobh = OOB_HEADER_FROM_PTR(ptr);

	/* temporarily add the OOB header */
	VALGRIND_ADD_TO_TX(oobh, OBJ_OOB_SIZE);

	/*
	 * no need to flush and persist because this
	 * will be done in pre-commit phase
	 */
	oobh->internal_type = TYPE_NONE;
	oobh->user_type = args->type_num;

	VALGRIND_REMOVE_FROM_TX(oobh, OBJ_OOB_SIZE);

	/* do not report changes made to the copy */
	VALGRIND_ADD_TO_TX(ptr, args->size);

	memcpy(ptr, args->ptr, args->copy_size);
	if (args->size > args->copy_size) {
		void *zero_ptr = (void *)((uintptr_t)ptr + args->copy_size);
		size_t zero_size = args->size - args->copy_size;
		memset(zero_ptr, 0, zero_size);
	}
}

/*
 * tx_set_state -- (internal) set transaction state
 */
static inline void
tx_set_state(PMEMobjpool *pop, struct lane_tx_layout *layout, uint64_t state)
{
	layout->state = state;
	pop->persist(&layout->state, sizeof (layout->state));
}

/*
 * tx_clear_undo_log -- (internal) clear undo log pointed by head
 */
static int
tx_clear_undo_log(PMEMobjpool *pop, struct list_head *head)
{
	LOG(3, NULL);

	int ret;
	PMEMoid obj;
	while (!OBJ_LIST_EMPTY(head)) {
		obj = head->pe_first;

#ifdef USE_VG_PMEMCHECK
		struct oob_header *oobh = OOB_HEADER_FROM_OID(pop, obj);
		size_t size = pmalloc_usable_size(pop,
				obj.off - OBJ_OOB_SIZE);

		VALGRIND_SET_CLEAN(oobh, size);
#endif

		/* remove and free all elements from undo log */
		ret = list_remove_free(pop, head,
				0, NULL, &obj);

		ASSERTeq(ret, 0);
		if (ret) {
			LOG(2, "list_remove_free failed");
			return ret;
		}
	}

	return 0;
}

/*
 * tx_abort_alloc -- (internal) abort all allocated objects
 */
static int
tx_abort_alloc(PMEMobjpool *pop, struct lane_tx_layout *layout)
{
	LOG(3, NULL);

	return tx_clear_undo_log(pop, &layout->undo_alloc);
}

/*
 * tx_abort_free -- (internal) abort all freeing objects
 */
static int
tx_abort_free(PMEMobjpool *pop, struct lane_tx_layout *layout)
{
	LOG(3, NULL);

	int ret;
	PMEMoid obj;
	while (!OBJ_LIST_EMPTY(&layout->undo_free)) {
		obj = layout->undo_free.pe_first;

		struct oob_header *oobh = OOB_HEADER_FROM_OID(pop, obj);
		ASSERT(oobh->user_type < PMEMOBJ_NUM_OID_TYPES);

		struct object_store_item *obj_list =
			&pop->store->bytype[oobh->user_type];

		/* move all objects back to object store */
		ret = list_move_oob(pop,
				&layout->undo_free, &obj_list->head, obj);

		ASSERTeq(ret, 0);
		if (ret) {
			LOG(2, "list_move_oob failed");
			return ret;
		}
	}

	return 0;
}

struct tx_range_data {
	void *begin;
	void *end;
	SLIST_ENTRY(tx_range_data) tx_range;
};

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
	/* XXX - change to compile-time check */
	ASSERTeq(sizeof (PMEMmutex), _POBJ_CL_ALIGNMENT);
	ASSERTeq(sizeof (PMEMrwlock), _POBJ_CL_ALIGNMENT);
	ASSERTeq(sizeof (PMEMcond), _POBJ_CL_ALIGNMENT);

	struct lane_tx_runtime *runtime =
			(struct lane_tx_runtime *)tx.section->runtime;
	ASSERTne(runtime, NULL);

	SLIST_HEAD(txr, tx_range_data) tx_ranges;
	SLIST_INIT(&tx_ranges);

	struct tx_range_data *txr;
	txr = Malloc(sizeof (*txr));
	if (txr == NULL) {
		FATAL("!Malloc");
	}

	txr->begin = OBJ_OFF_TO_PTR(pop, range->offset);
	txr->end = txr->begin + range->size;
	SLIST_INSERT_HEAD(&tx_ranges, txr, tx_range);

	struct tx_lock_data *txl;
	struct tx_range_data *txrn;

	/* check if there are any locks within given memory range */
	SLIST_FOREACH(txl, &(runtime->tx_locks), tx_lock) {
		void *lock_begin = txl->lock.mutex;
		/* all PMEM locks have the same size */
		void *lock_end = lock_begin + _POBJ_CL_ALIGNMENT;

		SLIST_FOREACH(txr, &tx_ranges, tx_range) {
			if ((lock_begin >= txr->begin &&
				lock_begin < txr->end) ||
				(lock_end >= txr->begin &&
				lock_end < txr->end)) {
				LOG(4, "detected PMEM lock"
					"in undo log; "
					"range %p-%p, lock %p-%p",
					txr->begin, txr->end,
					lock_begin, lock_end);

				/* split the range into new ones */
				if (lock_begin > txr->begin) {
					txrn = Malloc(sizeof (*txrn));
					if (txrn == NULL) {
						FATAL("!Malloc");
					}
					txrn->begin = txr->begin;
					txrn->end = lock_begin;
					LOG(4, "range split; %p-%p",
						txrn->begin, txrn->end);
					SLIST_INSERT_HEAD(&tx_ranges,
							txrn, tx_range);
				}

				if (lock_end < txr->end) {
					txrn = Malloc(sizeof (*txrn));
					if (txrn == NULL) {
						FATAL("!Malloc");
					}
					txrn->begin = lock_end;
					txrn->end = txr->end;
					LOG(4, "range split; %p-%p",
						txrn->begin, txrn->end);
					SLIST_INSERT_HEAD(&tx_ranges,
							txrn, tx_range);
				}

				/*
				 * remove the original range
				 * from the list
				 */
				SLIST_REMOVE(&tx_ranges, txr,
						tx_range_data, tx_range);
				Free(txr);
				break;
			}
		}
	}

	ASSERT(!SLIST_EMPTY(&tx_ranges));

	void *dst_ptr = OBJ_OFF_TO_PTR(pop, range->offset);

	while (!SLIST_EMPTY(&tx_ranges)) {
		struct tx_range_data *txr = SLIST_FIRST(&tx_ranges);
		SLIST_REMOVE_HEAD(&tx_ranges, tx_range);
		/* restore partial range data from snapshot */
		pop->memcpy_persist(txr->begin,
				&range->data[txr->begin - dst_ptr],
				txr->end - txr->begin);
		Free(txr);
	}
}

/*
 * tx_abort_set -- (internal) abort all set operations
 */
static int
tx_abort_set(PMEMobjpool *pop, struct lane_tx_layout *layout, int recovery)
{
	LOG(3, NULL);

	int ret;
	PMEMoid obj;

	while (!OBJ_OID_IS_NULL((obj = oob_list_last(pop,
					&layout->undo_set)))) {
		struct tx_range *range = OBJ_OFF_TO_PTR(pop, obj.off);

		if (recovery) {
			/* lane recovery */
			pop->memcpy_persist(OBJ_OFF_TO_PTR(pop, range->offset),
					range->data, range->size);
		} else {
			/* aborted transaction */
			tx_restore_range(pop, range);
		}

		/* remove snapshot from undo log */
		ret = list_remove_free(pop, &layout->undo_set, 0, NULL, &obj);

		ASSERTeq(ret, 0);
		if (ret) {
			LOG(2, "list_remove_free failed");
			return ret;
		}
	}

	return 0;
}

/*
 * tx_pre_commit_alloc -- (internal) do pre-commit operations for
 * allocated objects
 */
static void
tx_pre_commit_alloc(PMEMobjpool *pop, struct lane_tx_layout *layout)
{
	LOG(3, NULL);

	PMEMoid iter;
	for (iter = layout->undo_alloc.pe_first; !OBJ_OID_IS_NULL(iter);
		iter = oob_list_next(pop,
			&layout->undo_alloc, iter)) {

		struct oob_header *oobh = OOB_HEADER_FROM_OID(pop, iter);

		VALGRIND_ADD_TO_TX(oobh, OBJ_OOB_SIZE);

		/*
		 * Set object as allocated.
		 * This must be done in pre-commit phase instead of at
		 * allocation time in order to handle properly the case when
		 * the object is allocated and freed in the same transaction.
		 * In such case we need to know that the object
		 * is on undo log list and not in object store.
		 */
		oobh->internal_type = TYPE_ALLOCATED;

		VALGRIND_REMOVE_FROM_TX(oobh, OBJ_OOB_SIZE);

		size_t size = pmalloc_usable_size(pop,
				iter.off - OBJ_OOB_SIZE);

		/* flush and persist the whole allocated area and oob header */
		pop->persist(oobh, size);
	}
}

/*
 * tx_pre_commit_set -- (internal) do pre-commit operations for
 * set operations
 */
static void
tx_pre_commit_set(PMEMobjpool *pop, struct lane_tx_layout *layout)
{
	LOG(3, NULL);

	PMEMoid iter;
	for (iter = layout->undo_set.pe_first; !OBJ_OID_IS_NULL(iter);
		iter = oob_list_next(pop, &layout->undo_set, iter)) {

		struct tx_range *range = OBJ_OFF_TO_PTR(pop, iter.off);
		void *ptr = OBJ_OFF_TO_PTR(pop, range->offset);

		/* flush and persist modified area */
		pop->persist(ptr, range->size);
	}
}

/*
 * tx_post_commit_alloc -- (internal) do post commit operations for
 * allocated objects
 */
static int
tx_post_commit_alloc(PMEMobjpool *pop, struct lane_tx_layout *layout)
{
	LOG(3, NULL);

	PMEMoid obj;
	int ret;
	while (!OBJ_LIST_EMPTY(&layout->undo_alloc)) {
		obj = layout->undo_alloc.pe_first;

		struct oob_header *oobh = OOB_HEADER_FROM_OID(pop, obj);
		ASSERT(oobh->user_type < PMEMOBJ_NUM_OID_TYPES);

		struct object_store_item *obj_list =
			&pop->store->bytype[oobh->user_type];

		/* move object to object store */
		ret = list_move_oob(pop,
				&layout->undo_alloc, &obj_list->head, obj);

		ASSERTeq(ret, 0);
		if (ret) {
			LOG(2, "list_move_oob failed");
			return ret;
		}
	}

	return 0;
}

/*
 * tx_post_commit_free -- (internal) do post commit operations for
 * freeing objects
 */
static int
tx_post_commit_free(PMEMobjpool *pop, struct lane_tx_layout *layout)
{
	LOG(3, NULL);

	return tx_clear_undo_log(pop, &layout->undo_free);
}

/*
 * tx_post_commit_set -- (internal) do post commit operations for
 * add range
 */
static int
tx_post_commit_set(PMEMobjpool *pop, struct lane_tx_layout *layout)
{
	LOG(3, NULL);

	return tx_clear_undo_log(pop, &layout->undo_set);
}

/*
 * tx_pre_commit -- (internal) do pre-commit operations
 */
static void
tx_pre_commit(PMEMobjpool *pop, struct lane_tx_layout *layout)
{
	LOG(3, NULL);

	tx_pre_commit_set(pop, layout);
	tx_pre_commit_alloc(pop, layout);
}

/*
 * tx_post_commit -- (internal) do post commit operations
 */
static int
tx_post_commit(PMEMobjpool *pop, struct lane_tx_layout *layout)
{
	LOG(3, NULL);

	int ret;

	ret = tx_post_commit_set(pop, layout);
	ASSERTeq(ret, 0);
	if (ret) {
		LOG(2, "tx_post_commit_set failed");
		return ret;
	}

	ret = tx_post_commit_alloc(pop, layout);
	ASSERTeq(ret, 0);
	if (ret) {
		LOG(2, "tx_post_commit_alloc failed");
		return ret;
	}

	ret = tx_post_commit_free(pop, layout);
	ASSERTeq(ret, 0);
	if (ret) {
		LOG(2, "tx_post_commit_free failed");
		return ret;
	}

	return 0;
}

/*
 * tx_abort -- (internal) abort all allocated objects
 */
static int
tx_abort(PMEMobjpool *pop, struct lane_tx_layout *layout, int recovery)
{
	LOG(3, NULL);

	int ret;

	ret = tx_abort_set(pop, layout, recovery);
	ASSERTeq(ret, 0);
	if (ret) {
		LOG(2, "tx_abort_set failed");
		return ret;
	}

	ret = tx_abort_alloc(pop, layout);
	ASSERTeq(ret, 0);
	if (ret) {
		LOG(2, "tx_abort_alloc failed");
		return ret;
	}

	ret = tx_abort_free(pop, layout);
	ASSERTeq(ret, 0);
	if (ret) {
		LOG(2, "tx_abort_free failed");
		return ret;
	}

	return 0;
}

/*
 * add_to_tx_and_lock -- (internal) add lock to the transaction and acquire it
 */
static int
add_to_tx_and_lock(struct lane_tx_runtime *lane, enum pobj_tx_lock type,
	void *lock)
{
	LOG(15, NULL);
	int retval = 0;
	struct tx_lock_data *txl;
	/* check if the lock is already on the list */
	SLIST_FOREACH(txl, &(lane->tx_locks), tx_lock) {
		if (memcmp(&txl->lock, &lock, sizeof (lock)) == 0)
			return retval;
	}

	txl = Malloc(sizeof (*txl));
	if (txl == NULL)
		return ENOMEM;

	txl->lock_type = type;
	switch (txl->lock_type) {
		case TX_LOCK_MUTEX:
			txl->lock.mutex = lock;
			retval = pmemobj_mutex_lock(lane->pop,
				txl->lock.mutex);
			break;
		case TX_LOCK_RWLOCK:
			txl->lock.rwlock = lock;
			retval = pmemobj_rwlock_wrlock(lane->pop,
				txl->lock.rwlock);
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
			case TX_LOCK_MUTEX:
				pmemobj_mutex_unlock(lane->pop,
					tx_lock->lock.mutex);
				break;
			case TX_LOCK_RWLOCK:
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
tx_alloc_common(size_t size, unsigned int type_num,
	void (*constructor)(PMEMobjpool *pop, void *ptr, void *arg))
{
	LOG(3, NULL);

	if (tx.stage != TX_STAGE_WORK) {
		ERR("invalid tx stage");
		errno = EINVAL;
		return OID_NULL;
	}

	if (type_num >= PMEMOBJ_NUM_OID_TYPES) {
		ERR("invalid type_num %d", type_num);
		errno = EINVAL;
		pmemobj_tx_abort(EINVAL);
		return OID_NULL;
	}

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx.section->runtime;

	struct lane_tx_layout *layout =
		(struct lane_tx_layout *)tx.section->layout;

	struct tx_alloc_args args = {
		.type_num = type_num,
		.size = size,
	};

	/* allocate object to undo log */
	PMEMoid retoid = OID_NULL;
	list_insert_new(lane->pop, &layout->undo_alloc,
			0, NULL, OID_NULL, 0,
			size, constructor, &args, &retoid);

	if (OBJ_OID_IS_NULL(retoid)) {
		ERR("out of memory");
		errno = ENOMEM;
		pmemobj_tx_abort(ENOMEM);
	}

	return retoid;
}

/*
 * tx_alloc_copy_common -- (internal) common function for alloc with data copy
 */
static PMEMoid
tx_alloc_copy_common(size_t size, unsigned int type_num, const void *ptr,
	size_t copy_size, void (*constructor)(PMEMobjpool *pop, void *ptr,
	void *arg))
{
	LOG(3, NULL);

	if (type_num >= PMEMOBJ_NUM_OID_TYPES) {
		ERR("invalid type_num %d", type_num);
		errno = EINVAL;
		pmemobj_tx_abort(EINVAL);
		return OID_NULL;
	}

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx.section->runtime;

	struct lane_tx_layout *layout =
		(struct lane_tx_layout *)tx.section->layout;

	struct tx_alloc_copy_args args = {
		.type_num = type_num,
		.size = size,
		.ptr = ptr,
		.copy_size = copy_size,
	};

	/* allocate object to undo log */
	PMEMoid retoid;
	int ret = list_insert_new(lane->pop, &layout->undo_alloc,
			0, NULL, OID_NULL, 0,
			size, constructor, &args, &retoid);

	if (ret || OBJ_OID_IS_NULL(retoid)) {
		ERR("out of memory");
		errno = ENOMEM;
		pmemobj_tx_abort(ENOMEM);
	}

	return retoid;
}

/*
 * tx_realloc_common -- (internal) common function for tx realloc
 */
static PMEMoid
tx_realloc_common(PMEMoid oid, size_t size, unsigned int type_num,
	void (*constructor_alloc)(PMEMobjpool *pop, void *ptr, void *arg),
	void (*constructor_realloc)(PMEMobjpool *pop, void *ptr, void *arg))
{
	LOG(3, NULL);

	if (tx.stage != TX_STAGE_WORK) {
		ERR("invalid tx stage");
		errno = EINVAL;
		return OID_NULL;
	}

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx.section->runtime;

	/* if oid is NULL just alloc */
	if (OBJ_OID_IS_NULL(oid))
		return tx_alloc_common(size, type_num, constructor_alloc);

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
	size_t old_size = pmalloc_usable_size(lane->pop,
			oid.off - OBJ_OOB_SIZE) - OBJ_OOB_SIZE;

	size_t copy_size = old_size < size ? old_size : size;

	PMEMoid new_obj = tx_alloc_copy_common(size, type_num,
			ptr, copy_size, constructor_realloc);

	if (!OBJ_OID_IS_NULL(new_obj)) {
		if (pmemobj_tx_free(oid)) {
			ERR("pmemobj_tx_free failed");
			struct lane_tx_layout *layout =
				(struct lane_tx_layout *)tx.section->layout;
			int ret = list_remove_free(lane->pop,
					&layout->undo_alloc,
					0, NULL, &new_obj);
			/* XXX fatal error */
			ASSERTeq(ret, 0);
			if (ret)
				ERR("list_remove_free failed");

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
	VALGRIND_START_TX;

	int err = 0;

	struct lane_tx_runtime *lane = NULL;
	if (tx.stage == TX_STAGE_WORK) {
		lane = tx.section->runtime;
	} else if (tx.stage == TX_STAGE_NONE) {
		if ((err = lane_hold(pop, &tx.section,
			LANE_SECTION_TRANSACTION)) != 0)
			goto err_abort;

		lane = tx.section->runtime;
		SLIST_INIT(&lane->tx_entries);
		SLIST_INIT(&lane->tx_locks);

		lane->pop = pop;
	} else {
		err = EINVAL;
		goto err_abort;
	}

	struct tx_data *txd = Malloc(sizeof (*txd));
	if (txd == NULL) {
		err = ENOMEM;
		goto err_abort;
	}

	txd->errnum = 0;
	if (env != NULL)
		memcpy(txd->env, env, sizeof (jmp_buf));
	else
		memset(txd->env, 0, sizeof (jmp_buf));

	SLIST_INSERT_HEAD(&lane->tx_entries, txd, tx_entry);

	/* handle locks */
	va_list argp;
	va_start(argp, env);
	enum pobj_tx_lock lock_type;

	while ((lock_type = va_arg(argp, enum pobj_tx_lock)) != TX_LOCK_NONE) {
		if ((err = add_to_tx_and_lock(lane,
				lock_type, va_arg(argp, void *))) != 0) {
			va_end(argp);
			goto err_abort;
		}
	}
	va_end(argp);

	tx.stage = TX_STAGE_WORK;
	ASSERT(err == 0);
	return 0;

err_abort:
	tx.stage = TX_STAGE_ONABORT;
	return err;
}

/*
 * pmemobj_tx_stage -- returns current transaction stage
 */
enum pobj_tx_stage
pmemobj_tx_stage()
{
	LOG(3, NULL);

	return tx.stage;
}

/*
 * pmemobj_tx_abort -- aborts current transaction
 */
void
pmemobj_tx_abort(int errnum)
{
	LOG(3, NULL);

	ASSERT(tx.section != NULL);
	ASSERT(tx.stage == TX_STAGE_WORK);

	tx.stage = TX_STAGE_ONABORT;
	struct lane_tx_runtime *lane = tx.section->runtime;
	struct tx_data *txd = SLIST_FIRST(&lane->tx_entries);

	if (SLIST_NEXT(txd, tx_entry) == NULL) {
		/* this is the outermost transaction */

		struct lane_tx_layout *layout =
				(struct lane_tx_layout *)tx.section->layout;

		/* process the undo log */
		tx_abort(lane->pop, layout, 0 /* abort */);
	}

	txd->errnum = errnum;
	if (!util_is_zeroed(txd->env, sizeof (jmp_buf)))
		longjmp(txd->env, errnum);
}

/*
 * pmemobj_tx_commit -- commits current transaction
 */
int
pmemobj_tx_commit()
{
	LOG(3, NULL);
	int ret = 0;

	ASSERT(tx.section != NULL);
	ASSERT(tx.stage == TX_STAGE_WORK);

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx.section->runtime;
	struct tx_data *txd = SLIST_FIRST(&lane->tx_entries);

	if (SLIST_NEXT(txd, tx_entry) == NULL) {
		/* this is the outermost transaction */

		struct lane_tx_layout *layout =
			(struct lane_tx_layout *)tx.section->layout;

		/* pre-commit phase */
		tx_pre_commit(lane->pop, layout);

		/* set transaction state as committed */
		tx_set_state(lane->pop, layout, TX_STATE_COMMITTED);

		/* post commit phase */
		ret = tx_post_commit(lane->pop, layout);
		ASSERTeq(ret, 0);

		if (!ret) {
			/* clear transaction state */
			tx_set_state(lane->pop, layout, TX_STATE_NONE);
		} else {
			/* XXX need to handle this case somehow */
			LOG(2, "tx_post_commit failed");
		}
	}

	tx.stage = TX_STAGE_ONCOMMIT;

	return ret;
}

/*
 * pmemobj_tx_end -- ends current transaction
 */
void
pmemobj_tx_end()
{
	LOG(3, NULL);
	ASSERT(tx.stage != TX_STAGE_WORK);

	if (tx.section == NULL) {
		tx.stage = TX_STAGE_NONE;
		return;
	}

	struct lane_tx_runtime *lane = tx.section->runtime;
	struct tx_data *txd = SLIST_FIRST(&lane->tx_entries);
	SLIST_REMOVE_HEAD(&lane->tx_entries, tx_entry);
	int errnum = txd->errnum;
	Free(txd);

	VALGRIND_END_TX;

	if (SLIST_EMPTY(&lane->tx_entries)) {
		/* this is the outermost transaction */
		struct lane_tx_layout *layout =
			(struct lane_tx_layout *)tx.section->layout;

		/* the transaction state and undo log should be clear */
		ASSERTeq(layout->state, TX_STATE_NONE);
		if (layout->state != TX_STATE_NONE)
			LOG(2, "invalid transaction state");

		ASSERT(OBJ_LIST_EMPTY(&layout->undo_alloc));
		if (!OBJ_LIST_EMPTY(&layout->undo_alloc))
			LOG(2, "allocations undo log is not empty");

		tx.stage = TX_STAGE_NONE;
		release_and_free_tx_locks(lane);
		lane_release(lane->pop);
		tx.section = NULL;
	} else {
		/* resume the next transaction */
		tx.stage = TX_STAGE_WORK;

		/* abort called within inner transaction, waterfall the error */
		if (errnum)
			pmemobj_tx_abort(errnum);
	}
}

/*
 * pmemobj_tx_process -- processes current transaction stage
 */
int
pmemobj_tx_process()
{
	LOG(3, NULL);

	ASSERT(tx.section != NULL);
	ASSERT(tx.stage != TX_STAGE_NONE);

	switch (tx.stage) {
	case TX_STAGE_NONE:
		break;
	case TX_STAGE_WORK:
		return pmemobj_tx_commit();
	case TX_STAGE_ONABORT:
	case TX_STAGE_ONCOMMIT:
		tx.stage = TX_STAGE_FINALLY;
		break;
	case TX_STAGE_FINALLY:
		tx.stage = TX_STAGE_NONE;
		break;
	case MAX_TX_STAGE:
		ASSERT(1);
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

	struct lane_tx_layout *layout =
			(struct lane_tx_layout *)tx.section->layout;

	if (args->offset < args->pop->heap_offset ||
		(args->offset + args->size) >
		(args->pop->heap_offset + args->pop->heap_size)) {
		ERR("object outside of heap");
		return EINVAL;
	}

#ifdef DEBUG
	/* verify if the range is already added to the undo log */
	PMEMoid iter = layout->undo_set.pe_first;
	while (!OBJ_OID_IS_NULL(iter)) {
		struct tx_range *range =
			OBJ_OFF_TO_PTR(args->pop, iter.off);

		if (args->offset >= range->offset &&
				args->offset + args->size <=
				range->offset + range->size) {
			LOG(4, "Notice: range: offset = 0x%jx"
			    " size = %zu is already in undo log"
			    " as range: offset = 0x%jx size = %zu",
			    args->offset, args->size,
			    range->offset, range->size);
			break;
		} else if (args->offset <= range->offset + range->size &&
				args->offset + args->size >=
				range->offset) {
			LOG(4, "Notice: a part of range: offset = 0x%jx"
			    " size = %zu is already in undo log"
			    " as range: offset = 0x%jx size = %zu",
			    args->offset, args->size,
			    range->offset, range->size);
			break;
		}
		iter = oob_list_next(args->pop, &layout->undo_set, iter);
	}

#endif /* DEBUG */

	/* insert snapshot to undo log */
	PMEMoid snapshot;
	int ret = list_insert_new(args->pop, &layout->undo_set, 0,
			NULL, OID_NULL, 0,
			args->size + sizeof (struct tx_range),
			constructor_tx_add_range, args, &snapshot);

	ASSERTeq(ret, 0);

	return ret;
}

/*
 * pmemobj_tx_add_range_direct -- adds persistent memory range into the
 *					transaction
 */
int
pmemobj_tx_add_range_direct(void *ptr, size_t size)
{
	LOG(3, NULL);

	if (tx.stage != TX_STAGE_WORK) {
		ERR("invalid tx stage");
		return EINVAL;
	}

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx.section->runtime;

	struct tx_add_range_args args = {
		.pop = lane->pop,
		.offset = ptr - (void *)lane->pop,
		.size = size
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

	if (tx.stage != TX_STAGE_WORK) {
		ERR("invalid tx stage");
		return EINVAL;
	}

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx.section->runtime;

	if (oid.pool_uuid_lo != lane->pop->uuid_lo) {
		ERR("invalid pool uuid");
		pmemobj_tx_abort(EINVAL);

		return EINVAL;
	}
	ASSERT(OBJ_OID_IS_VALID(lane->pop, oid));

	struct oob_header *oobh = OOB_HEADER_FROM_OID(lane->pop, oid);

	struct tx_add_range_args args = {
		.pop = lane->pop,
		.offset = oid.off + hoff,
		.size = size
	};

	/*
	 * If internal type is not equal to TYPE_ALLOCATED it means
	 * the object was allocated within this transaction
	 * and there is no need to create a snapshot.
	 */
	if (oobh->internal_type == TYPE_ALLOCATED)
		return pmemobj_tx_add_common(&args);

	return 0;
}

/*
 * pmemobj_tx_alloc -- allocates a new object
 */
PMEMoid
pmemobj_tx_alloc(size_t size, unsigned int type_num)
{
	LOG(3, NULL);

	if (size == 0) {
		ERR("allocation with size 0");
		errno = EINVAL;
		return OID_NULL;
	}

	return tx_alloc_common(size, type_num, constructor_tx_alloc);
}

/*
 * pmemobj_tx_zalloc -- allocates a new zeroed object
 */
PMEMoid
pmemobj_tx_zalloc(size_t size, unsigned int type_num)
{
	LOG(3, NULL);

	if (size == 0) {
		ERR("allocation with size 0");
		errno = EINVAL;
		return OID_NULL;
	}

	return tx_alloc_common(size, type_num, constructor_tx_zalloc);
}

/*
 * pmemobj_tx_realloc -- resizes an existing object
 */
PMEMoid
pmemobj_tx_realloc(PMEMoid oid, size_t size, unsigned int type_num)
{
	LOG(3, NULL);

	return tx_realloc_common(oid, size, type_num,
			constructor_tx_alloc, constructor_tx_copy);
}


/*
 * pmemobj_zrealloc -- resizes an existing object, any new space is zeroed.
 */
PMEMoid
pmemobj_tx_zrealloc(PMEMoid oid, size_t size, unsigned int type_num)
{
	LOG(3, NULL);

	return tx_realloc_common(oid, size, type_num,
			constructor_tx_zalloc, constructor_tx_copy_zero);
}

/*
 * pmemobj_tx_strdup -- allocates a new object with duplicate of the string s.
 */
PMEMoid
pmemobj_tx_strdup(const char *s, unsigned int type_num)
{
	LOG(3, NULL);

	if (tx.stage != TX_STAGE_WORK) {
		ERR("invalid tx stage");
		errno = EINVAL;
		return OID_NULL;
	}

	if (NULL == s) {
		errno = EINVAL;
		pmemobj_tx_abort(EINVAL);
		return OID_NULL;
	}

	size_t len = strlen(s);

	if (len == 0)
		return tx_alloc_common(sizeof (char), type_num,
				constructor_tx_zalloc);

	size_t size = (len + 1) * sizeof (char);

	return tx_alloc_copy_common(size, type_num, s, size,
			constructor_tx_copy);
}

/*
 * pmemobj_tx_free -- frees an existing object
 */
int
pmemobj_tx_free(PMEMoid oid)
{
	LOG(3, NULL);

	if (tx.stage != TX_STAGE_WORK) {
		ERR("invalid tx stage");
		errno = EINVAL;
		return EINVAL;
	}

	if (OBJ_OID_IS_NULL(oid))
		return 0;

	struct lane_tx_runtime *lane =
		(struct lane_tx_runtime *)tx.section->runtime;

	if (lane->pop->uuid_lo != oid.pool_uuid_lo) {
		ERR("invalid pool uuid");
		errno = EINVAL;
		pmemobj_tx_abort(EINVAL);
		return EINVAL;
	}
	ASSERT(OBJ_OID_IS_VALID(lane->pop, oid));

	struct lane_tx_layout *layout =
		(struct lane_tx_layout *)tx.section->layout;

	struct oob_header *oobh = OOB_HEADER_FROM_OID(lane->pop, oid);
	ASSERT(oobh->user_type < PMEMOBJ_NUM_OID_TYPES);

	if (oobh->internal_type == TYPE_ALLOCATED) {
		/* the object is in object store */
		struct object_store_item *obj_list =
			&lane->pop->store->bytype[oobh->user_type];

		return list_move_oob(lane->pop, &obj_list->head,
				&layout->undo_free, oid);
	} else {
		ASSERTeq(oobh->internal_type, TYPE_NONE);
#ifdef USE_VG_PMEMCHECK
		size_t size = pmalloc_usable_size(lane->pop,
				oid.off - OBJ_OOB_SIZE);

		VALGRIND_SET_CLEAN(oobh, size);
#endif
		VALGRIND_REMOVE_FROM_TX(oobh, pmalloc_usable_size(lane->pop,
				oid.off - OBJ_OOB_SIZE));
		/*
		 * The object has been allocated within the same transaction
		 * so we can just remove and free the object from undo log.
		 */
		return list_remove_free(lane->pop, &layout->undo_alloc,
				0, NULL, &oid);
	}
}

/*
 * lane_transaction_construct -- create transaction lane section
 */
static int
lane_transaction_construct(struct lane_section *section)
{
	section->runtime = Malloc(sizeof (struct lane_tx_runtime));
	if (section->runtime == NULL)
		return ENOMEM;
	memset(section->runtime, 0, sizeof (struct lane_tx_runtime));

	return 0;
}

/*
 * lane_transaction_destruct -- destroy transaction lane section
 */
static int
lane_transaction_destruct(struct lane_section *section)
{
	Free(section->runtime);

	return 0;
}

/*
 * lane_transaction_recovery -- recovery of transaction lane section
 */
static int
lane_transaction_recovery(PMEMobjpool *pop,
	struct lane_section_layout *section)
{
	struct lane_tx_layout *layout = (struct lane_tx_layout *)section;
	int ret = 0;

	if (layout->state == TX_STATE_COMMITTED) {
		/*
		 * The transaction has been committed so we have to
		 * process the undo log, do the post commit phase
		 * and clear the transaction state.
		 */
		ret = tx_post_commit(pop, layout);
		if (!ret) {
			tx_set_state(pop, layout, TX_STATE_NONE);
		} else {
			ERR("tx_post_commit failed");
		}
	} else {
		/* process undo log and restore all operations */
		tx_abort(pop, layout, 1 /* recovery */);
	}

	return ret;
}

/*
 * lane_transaction_check -- consistency check of transaction lane section
 */
static int
lane_transaction_check(PMEMobjpool *pop,
	struct lane_section_layout *section)
{
	LOG(3, "tx lane %p", section);

	struct lane_tx_layout *tx_sec = (struct lane_tx_layout *)section;

	if (tx_sec->state != TX_STATE_NONE &&
		tx_sec->state != TX_STATE_COMMITTED) {
		ERR("tx lane: invalid transaction state");
		return -1;
	}

	PMEMoid iter;
	/* check undo log for set operation */
	for (iter = tx_sec->undo_set.pe_first; !OBJ_OID_IS_NULL(iter);
		iter = oob_list_next(pop, &tx_sec->undo_set, iter)) {

		struct tx_range *range = OBJ_OFF_TO_PTR(pop, iter.off);

		if (!OBJ_OFF_FROM_HEAP(pop, range->offset) ||
			!OBJ_OFF_FROM_HEAP(pop, range->offset + range->size)) {
			ERR("tx_lane: invalid offset in tx range object");
			return -1;
		}
	}

	/* check undo log for allocations */
	for (iter = tx_sec->undo_alloc.pe_first; !OBJ_OID_IS_NULL(iter);
		iter = oob_list_next(pop, &tx_sec->undo_alloc, iter)) {

		struct oob_header *oobh = OOB_HEADER_FROM_OID(pop, iter);
		if (oobh->internal_type != TYPE_NONE) {
			ERR("tx lane: invalid internal type");
			return -1;
		}

		if (oobh->user_type >= PMEMOBJ_NUM_OID_TYPES) {
			ERR("tx lane: invalid user type");
			return -1;
		}
	}

	/* check undo log for free operation */
	for (iter = tx_sec->undo_free.pe_first; !OBJ_OID_IS_NULL(iter);
		iter = oob_list_next(pop, &tx_sec->undo_free, iter)) {

		struct oob_header *oobh = OOB_HEADER_FROM_OID(pop, iter);
		if (oobh->internal_type != TYPE_ALLOCATED) {
			ERR("tx lane: invalid internal type");
			return -1;
		}

		if (oobh->user_type >= PMEMOBJ_NUM_OID_TYPES) {
			ERR("tx lane: invalid user type");
			return -1;
		}
	}

	return 0;
}

struct section_operations transaction_ops = {
	.construct = lane_transaction_construct,
	.destruct = lane_transaction_destruct,
	.recover = lane_transaction_recovery,
	.check = lane_transaction_check
};

SECTION_PARM(LANE_SECTION_TRANSACTION, &transaction_ops);
