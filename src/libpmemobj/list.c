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
 * list.c -- implementation of persistent atomic lists module
 */
#include <inttypes.h>

#include "list.h"
#include "obj.h"
#include "os_thread.h"
#include "out.h"
#include "sync.h"
#include "valgrind_internal.h"

#define PREV_OFF (offsetof(struct list_entry, pe_prev) + offsetof(PMEMoid, off))
#define NEXT_OFF (offsetof(struct list_entry, pe_next) + offsetof(PMEMoid, off))

/*
 * list_args_common -- common arguments for operations on list
 *
 * pe_offset    - offset to list entry relative to user data
 * obj_doffset  - offset to element's data relative to pmemobj pool
 * entry_ptr    - list entry structure of element
 */
struct list_args_common {
	ssize_t pe_offset;
	uint64_t obj_doffset;
	struct list_entry *entry_ptr;
};

/*
 * list_args_insert -- arguments for inserting element to list
 *
 * head           - list head
 * dest           - destination element OID
 * dest_entry_ptr - list entry of destination element
 * before         - insert before or after destination element
 */
struct list_args_insert {
	struct list_head *head;
	PMEMoid dest;
	struct list_entry *dest_entry_ptr;
	int before;
};

/*
 * list_args_reinsert -- arguments for reinserting element on list
 *
 * head         - list head
 * entry_ptr    - list entry of old element
 * obj_doffset  - offset to element's data relative to pmemobj pool
 */
struct list_args_reinsert {
	struct list_head *head;
	struct list_entry *entry_ptr;
	uint64_t obj_doffset;
};

/*
 * list_args_remove -- arguments for removing element from list
 *
 * pe_offset    - offset to list entry relative to user data
 * obj_doffset  - offset to element's data relative to pmemobj pool
 * head         - list head
 * entry_ptr    - list entry structure of element
 */
struct list_args_remove {
	ssize_t pe_offset;
	uint64_t obj_doffset;
	struct list_head *head;
	struct list_entry *entry_ptr;
};

/*
 * list_mutexes_lock -- (internal) grab one or two locks in ascending
 * address order
 */
static inline int
list_mutexes_lock(PMEMobjpool *pop,
	struct list_head *head1, struct list_head *head2)
{
	ASSERTne(head1, NULL);

	if (!head2 || head1 == head2)
		return pmemobj_mutex_lock(pop, &head1->lock);

	PMEMmutex *lock1;
	PMEMmutex *lock2;
	if ((uintptr_t)&head1->lock < (uintptr_t)&head2->lock) {
		lock1 = &head1->lock;
		lock2 = &head2->lock;
	} else {
		lock1 = &head2->lock;
		lock2 = &head1->lock;
	}

	int ret;
	if ((ret = pmemobj_mutex_lock(pop, lock1)))
		goto err;
	if ((ret = pmemobj_mutex_lock(pop, lock2)))
		goto err_unlock;

	return 0;

err_unlock:
	pmemobj_mutex_unlock(pop, lock1);
err:
	return ret;
}

/*
 * list_mutexes_unlock -- (internal) release one or two locks
 */
static inline void
list_mutexes_unlock(PMEMobjpool *pop,
	struct list_head *head1, struct list_head *head2)
{
	ASSERTne(head1, NULL);

	if (!head2 || head1 == head2) {
		pmemobj_mutex_unlock_nofail(pop, &head1->lock);
		return;
	}

	pmemobj_mutex_unlock_nofail(pop, &head1->lock);
	pmemobj_mutex_unlock_nofail(pop, &head2->lock);
}

/*
 * list_get_dest -- (internal) return destination object ID
 *
 * If the input dest is not OID_NULL returns dest.
 * If the input dest is OID_NULL and before is set returns first element.
 * If the input dest is OID_NULL and before is no set returns last element.
 */
static inline PMEMoid
list_get_dest(PMEMobjpool *pop, struct list_head *head, PMEMoid dest,
		ssize_t pe_offset, int before)
{
	if (dest.off)
		return dest;

	if (head->pe_first.off == 0 || !!before == POBJ_LIST_DEST_HEAD)
		return head->pe_first;

	struct list_entry *first_ptr = (struct list_entry *)OBJ_OFF_TO_PTR(pop,
			(uintptr_t)((ssize_t)head->pe_first.off + pe_offset));

	return first_ptr->pe_prev;
}

/*
 * list_set_oid_redo_log -- (internal) set PMEMoid value using redo log
 */
static size_t
list_set_oid_redo_log(PMEMobjpool *pop,
	struct redo_log *redo, size_t redo_index,
	PMEMoid *oidp, uint64_t obj_doffset, int oidp_inited)
{
	ASSERT(OBJ_PTR_IS_VALID(pop, oidp));

	if (!oidp_inited || oidp->pool_uuid_lo != pop->uuid_lo) {
		if (oidp_inited)
			ASSERTeq(oidp->pool_uuid_lo, 0);
		uint64_t oid_uuid_off = OBJ_PTR_TO_OFF(pop,
				&oidp->pool_uuid_lo);
		redo_log_store(pop->redo, redo, redo_index, oid_uuid_off,
				pop->uuid_lo);
		redo_index += 1;
	}

	uint64_t oid_off_off = OBJ_PTR_TO_OFF(pop, &oidp->off);
	redo_log_store(pop->redo, redo, redo_index, oid_off_off,
			obj_doffset);

	return redo_index + 1;
}


/*
 * list_update_head -- (internal) update pe_first entry in list head
 */
static size_t
list_update_head(PMEMobjpool *pop,
	struct redo_log *redo, size_t redo_index,
	struct list_head *head, uint64_t first_offset)
{
	LOG(15, NULL);

	uint64_t pe_first_off_off = OBJ_PTR_TO_OFF(pop, &head->pe_first.off);

	redo_log_store(pop->redo, redo, redo_index + 0,
			pe_first_off_off, first_offset);

	if (head->pe_first.pool_uuid_lo == 0) {
		uint64_t pe_first_uuid_off = OBJ_PTR_TO_OFF(pop,
				&head->pe_first.pool_uuid_lo);

		redo_log_store(pop->redo, redo, redo_index + 1,
				pe_first_uuid_off, pop->uuid_lo);

		return redo_index + 2;
	} else {
		return redo_index + 1;
	}
}

/*
 * u64_add_offset -- (internal) add signed offset to unsigned integer and check
 * for overflows
 */
static void
u64_add_offset(uint64_t *value, ssize_t off)
{
	uint64_t prev = *value;
	if (off >= 0) {
		*value += (size_t)off;
		ASSERT(*value >= prev); /* detect overflow */
	} else {
		*value -= (size_t)-off;
		ASSERT(*value < prev);
	}
}

/*
 * list_fill_entry_persist -- (internal) fill new entry using persist function
 *
 * Used for newly allocated objects.
 */
static void
list_fill_entry_persist(PMEMobjpool *pop, struct list_entry *entry_ptr,
		uint64_t next_offset, uint64_t prev_offset)
{
	LOG(15, NULL);

	VALGRIND_ADD_TO_TX(entry_ptr, sizeof(*entry_ptr));
	entry_ptr->pe_next.pool_uuid_lo = pop->uuid_lo;
	entry_ptr->pe_next.off = next_offset;

	entry_ptr->pe_prev.pool_uuid_lo = pop->uuid_lo;
	entry_ptr->pe_prev.off = prev_offset;
	VALGRIND_REMOVE_FROM_TX(entry_ptr, sizeof(*entry_ptr));

	pmemops_persist(&pop->p_ops, entry_ptr, sizeof(*entry_ptr));
}

/*
 * list_fill_entry_redo_log -- (internal) fill new entry using redo log
 *
 * Used to update entry in existing object.
 */
static size_t
list_fill_entry_redo_log(PMEMobjpool *pop,
	struct redo_log *redo, size_t redo_index,
	struct list_args_common *args,
	uint64_t next_offset, uint64_t prev_offset, int set_uuid)
{
	LOG(15, NULL);
	struct pmem_ops *ops = &pop->p_ops;

	ASSERTne(args->entry_ptr, NULL);
	ASSERTne(args->obj_doffset, 0);

	if (set_uuid) {
		VALGRIND_ADD_TO_TX(&(args->entry_ptr->pe_next.pool_uuid_lo),
				sizeof(args->entry_ptr->pe_next.pool_uuid_lo));
		VALGRIND_ADD_TO_TX(&(args->entry_ptr->pe_prev.pool_uuid_lo),
				sizeof(args->entry_ptr->pe_prev.pool_uuid_lo));
		/* don't need to fill pool uuid using redo log */
		args->entry_ptr->pe_next.pool_uuid_lo = pop->uuid_lo;
		args->entry_ptr->pe_prev.pool_uuid_lo = pop->uuid_lo;
		VALGRIND_REMOVE_FROM_TX(
				&(args->entry_ptr->pe_next.pool_uuid_lo),
				sizeof(args->entry_ptr->pe_next.pool_uuid_lo));
		VALGRIND_REMOVE_FROM_TX(
				&(args->entry_ptr->pe_prev.pool_uuid_lo),
				sizeof(args->entry_ptr->pe_prev.pool_uuid_lo));
		pmemops_persist(ops, args->entry_ptr, sizeof(*args->entry_ptr));
	} else {
		ASSERTeq(args->entry_ptr->pe_next.pool_uuid_lo, pop->uuid_lo);
		ASSERTeq(args->entry_ptr->pe_prev.pool_uuid_lo, pop->uuid_lo);
	}

	/* set current->next and current->prev using redo log */
	uint64_t next_off_off = args->obj_doffset + NEXT_OFF;
	uint64_t prev_off_off = args->obj_doffset + PREV_OFF;
	u64_add_offset(&next_off_off, args->pe_offset);
	u64_add_offset(&prev_off_off, args->pe_offset);

	redo_log_store(pop->redo, redo, redo_index + 0, next_off_off,
			next_offset);
	redo_log_store(pop->redo, redo, redo_index + 1, prev_off_off,
			prev_offset);

	return redo_index + 2;
}

/*
 * list_remove_single -- (internal) remove element from single list
 */
static size_t
list_remove_single(PMEMobjpool *pop,
	struct redo_log *redo, size_t redo_index,
	struct list_args_remove *args)
{
	LOG(15, NULL);

	if (args->entry_ptr->pe_next.off == args->obj_doffset) {
		/* only one element on list */
		ASSERTeq(args->head->pe_first.off, args->obj_doffset);
		ASSERTeq(args->entry_ptr->pe_prev.off, args->obj_doffset);

		return list_update_head(pop, redo, redo_index, args->head, 0);
	} else {
		/* set next->prev = prev and prev->next = next */
		uint64_t next_off = args->entry_ptr->pe_next.off;
		uint64_t next_prev_off = next_off + PREV_OFF;
		u64_add_offset(&next_prev_off, args->pe_offset);
		uint64_t prev_off = args->entry_ptr->pe_prev.off;
		uint64_t prev_next_off = prev_off + NEXT_OFF;
		u64_add_offset(&prev_next_off, args->pe_offset);

		redo_log_store(pop->redo, redo, redo_index + 0,
				next_prev_off, prev_off);
		redo_log_store(pop->redo, redo, redo_index + 1,
				prev_next_off, next_off);
		redo_index += 2;

		if (args->head->pe_first.off == args->obj_doffset) {
			/* removing element is the first one */
			return list_update_head(pop, redo, redo_index,
					args->head, next_off);
		} else {
			return redo_index;
		}
	}
}

/*
 * list_insert_before -- (internal) insert element at offset before an element
 */
static size_t
list_insert_before(PMEMobjpool *pop,
	struct redo_log *redo, size_t redo_index,
	struct list_args_insert *args, struct list_args_common *args_common,
	uint64_t *next_offset, uint64_t *prev_offset)
{
	LOG(15, NULL);

	/* current->next = dest and current->prev = dest->prev */
	*next_offset = args->dest.off;
	*prev_offset = args->dest_entry_ptr->pe_prev.off;

	/* dest->prev = current and dest->prev->next = current */
	uint64_t dest_prev_off = args->dest.off + PREV_OFF;
	u64_add_offset(&dest_prev_off, args_common->pe_offset);
	uint64_t dest_prev_next_off = args->dest_entry_ptr->pe_prev.off +
					NEXT_OFF;
	u64_add_offset(&dest_prev_next_off, args_common->pe_offset);

	redo_log_store(pop->redo, redo, redo_index + 0,
			dest_prev_off, args_common->obj_doffset);
	redo_log_store(pop->redo, redo, redo_index + 1,
			dest_prev_next_off, args_common->obj_doffset);

	return redo_index + 2;
}

/*
 * list_insert_after -- (internal) insert element at offset after an element
 */
static size_t
list_insert_after(PMEMobjpool *pop,
	struct redo_log *redo, size_t redo_index,
	struct list_args_insert *args, struct list_args_common *args_common,
	uint64_t *next_offset, uint64_t *prev_offset)
{
	LOG(15, NULL);

	/* current->next = dest->next and current->prev = dest */
	*next_offset = args->dest_entry_ptr->pe_next.off;
	*prev_offset = args->dest.off;

	/* dest->next = current and dest->next->prev = current */
	uint64_t dest_next_off = args->dest.off + NEXT_OFF;
	u64_add_offset(&dest_next_off, args_common->pe_offset);
	uint64_t dest_next_prev_off = args->dest_entry_ptr->pe_next.off +
					PREV_OFF;
	u64_add_offset(&dest_next_prev_off, args_common->pe_offset);

	redo_log_store(pop->redo, redo, redo_index + 0,
			dest_next_off, args_common->obj_doffset);
	redo_log_store(pop->redo, redo, redo_index + 1,
			dest_next_prev_off, args_common->obj_doffset);

	return redo_index + 2;
}

/*
 * list_insert_user -- (internal) insert element at offset to a user list
 */
static size_t
list_insert_user(PMEMobjpool *pop,
	struct redo_log *redo, size_t redo_index,
	struct list_args_insert *args, struct list_args_common *args_common,
	uint64_t *next_offset, uint64_t *prev_offset)
{
	LOG(15, NULL);
	if (args->dest.off == 0) {
		/* inserting the first element on list */
		ASSERTeq(args->head->pe_first.off, 0);

		/* set loop on current element */
		*next_offset = args_common->obj_doffset;
		*prev_offset = args_common->obj_doffset;

		/* update head */
		redo_index = list_update_head(pop,
			redo, redo_index, args->head,
			args_common->obj_doffset);
	} else {
		if (args->before) {
			/* inserting before dest */
			redo_index = list_insert_before(pop,
				redo, redo_index, args, args_common,
				next_offset, prev_offset);

			if (args->dest.off == args->head->pe_first.off) {
				/* current element at first position */
				redo_index = list_update_head(pop,
					redo, redo_index, args->head,
					args_common->obj_doffset);
			}
		} else {
			/* inserting after dest */
			redo_index = list_insert_after(pop,
				redo, redo_index, args, args_common,
				next_offset, prev_offset);
		}
	}

	return redo_index;
}

/*
 * list_insert_new -- allocate and insert element to oob and user lists
 *
 * pop         - pmemobj pool handle
 * pe_offset   - offset to list entry on user list relative to user data
 * user_head   - user list head, must be locked if not NULL
 * dest        - destination on user list
 * before      - insert before/after destination on user list
 * size        - size of allocation, will be increased by OBJ_OOB_SIZE
 * constructor - object's constructor
 * arg         - argument for object's constructor
 * oidp        - pointer to target object ID
 */
static int
list_insert_new(PMEMobjpool *pop,
	size_t pe_offset, struct list_head *user_head, PMEMoid dest, int before,
	size_t size, int (*constructor)(void *ctx, void *ptr,
	size_t usable_size, void *arg), void *arg, PMEMoid *oidp)
{
	LOG(3, NULL);
	ASSERT(user_head != NULL);

	int ret;

	struct lane_section *lane_section;

#ifdef DEBUG
	int r = pmemobj_mutex_assert_locked(pop, &user_head->lock);
	ASSERTeq(r, 0);
#endif

	lane_hold(pop, &lane_section, LANE_SECTION_LIST);

	ASSERTne(lane_section, NULL);
	ASSERTne(lane_section->layout, NULL);

	struct lane_list_layout *section =
		(struct lane_list_layout *)lane_section->layout;
	struct redo_log *redo = section->redo;
	size_t redo_index = 0;
	uint64_t sec_off_off = OBJ_PTR_TO_OFF(pop, &section->obj_offset);

	if (constructor) {
		if ((ret = pmalloc_construct(pop,
				&section->obj_offset, size,
				constructor, arg, 0, 0, 0))) {
			ERR("!pmalloc_construct");
			goto err_pmalloc;
		}
	} else {
		ret = pmalloc(pop, &section->obj_offset, size, 0, 0);
		if (ret) {
			ERR("!pmalloc");
			goto err_pmalloc;
		}
	}

	uint64_t obj_doffset = section->obj_offset;

	ASSERT((ssize_t)pe_offset >= 0);

	dest = list_get_dest(pop, user_head, dest,
		(ssize_t)pe_offset, before);

	struct list_entry *entry_ptr =
		(struct list_entry *)OBJ_OFF_TO_PTR(pop,
				obj_doffset + pe_offset);

	struct list_entry *dest_entry_ptr =
		(struct list_entry *)OBJ_OFF_TO_PTR(pop,
				dest.off + pe_offset);

	struct list_args_insert args = {
		.dest = dest,
		.dest_entry_ptr = dest_entry_ptr,
		.head = user_head,
		.before = before,
	};

	struct list_args_common args_common = {
		.obj_doffset = obj_doffset,
		.entry_ptr = entry_ptr,
		.pe_offset = (ssize_t)pe_offset,
	};

	uint64_t next_offset;
	uint64_t prev_offset;

	/* insert element to user list */
	redo_index = list_insert_user(pop,
		redo, redo_index, &args, &args_common,
		&next_offset, &prev_offset);

	/* don't need to use redo log for filling new element */
	list_fill_entry_persist(pop, entry_ptr,
			next_offset, prev_offset);

	if (oidp != NULL) {
		if (OBJ_PTR_IS_VALID(pop, oidp)) {
			redo_index = list_set_oid_redo_log(pop, redo,
					redo_index, oidp, obj_doffset, 0);
		} else {
			oidp->off = obj_doffset;
			oidp->pool_uuid_lo = pop->uuid_lo;
		}
	}

	/* clear the obj_offset in lane section */
	redo_log_store_last(pop->redo, redo, redo_index, sec_off_off, 0);

	redo_log_process(pop->redo, redo, REDO_NUM_ENTRIES);

	ret = 0;

err_pmalloc:
	lane_release(pop);

	ASSERT(ret == 0 || ret == -1);
	return ret;
}

/*
 * list_insert_new_user -- allocate and insert element to oob and user lists
 *
 * pop         - pmemobj pool handle
 * oob_head    - oob list head
 * pe_offset   - offset to list entry on user list relative to user data
 * user_head   - user list head
 * dest        - destination on user list
 * before      - insert before/after destination on user list
 * size        - size of allocation, will be increased by OBJ_OOB_SIZE
 * constructor - object's constructor
 * arg         - argument for object's constructor
 * oidp        - pointer to target object ID
 */
int
list_insert_new_user(PMEMobjpool *pop,
	size_t pe_offset, struct list_head *user_head, PMEMoid dest, int before,
	size_t size, int (*constructor)(void *ctx, void *ptr,
	size_t usable_size, void *arg), void *arg, PMEMoid *oidp)
{
	int ret;
	if ((ret = pmemobj_mutex_lock(pop, &user_head->lock))) {
		errno = ret;
		LOG(2, "pmemobj_mutex_lock failed");
		return -1;
	}

	ret = list_insert_new(pop, pe_offset, user_head,
			dest, before, size, constructor, arg, oidp);

	pmemobj_mutex_unlock_nofail(pop, &user_head->lock);

	ASSERT(ret == 0 || ret == -1);
	return ret;
}

/*
 * list_insert -- insert object to a single list
 *
 * pop          - pmemobj handle
 * pe_offset    - offset to list entry on user list relative to user data
 * head         - list head
 * dest         - destination object ID
 * before       - before/after destination
 * oid          - target object ID
 */
int
list_insert(PMEMobjpool *pop,
	ssize_t pe_offset, struct list_head *head,
	PMEMoid dest, int before,
	PMEMoid oid)
{
	LOG(3, NULL);
	ASSERTne(head, NULL);

	int ret;

	struct lane_section *lane_section;

	lane_hold(pop, &lane_section, LANE_SECTION_LIST);

	if ((ret = pmemobj_mutex_lock(pop, &head->lock))) {
		errno = ret;
		LOG(2, "pmemobj_mutex_lock failed");
		ret = -1;
		goto err;
	}

	ASSERTne(lane_section, NULL);
	ASSERTne(lane_section->layout, NULL);

	struct lane_list_layout *section =
		(struct lane_list_layout *)lane_section->layout;
	struct redo_log *redo = section->redo;
	size_t redo_index = 0;

	dest = list_get_dest(pop, head, dest, pe_offset, before);

	struct list_entry *entry_ptr =
		(struct list_entry *)OBJ_OFF_TO_PTR(pop,
			(uintptr_t)((ssize_t)oid.off + pe_offset));

	struct list_entry *dest_entry_ptr =
		(struct list_entry *)OBJ_OFF_TO_PTR(pop,
			(uintptr_t)((ssize_t)dest.off + pe_offset));

	struct list_args_insert args = {
		.dest = dest,
		.dest_entry_ptr = dest_entry_ptr,
		.head = head,
		.before = before,
	};

	struct list_args_common args_common = {
		.obj_doffset = oid.off,
		.entry_ptr = entry_ptr,
		.pe_offset = (ssize_t)pe_offset,
	};

	uint64_t next_offset;
	uint64_t prev_offset;

	/* insert element to user list */
	redo_index = list_insert_user(pop, redo, redo_index,
			&args, &args_common, &next_offset, &prev_offset);

	/* fill entry of existing element using redo log */
	redo_index = list_fill_entry_redo_log(pop, redo, redo_index,
			&args_common, next_offset, prev_offset, 1);

	redo_log_set_last(pop->redo, redo, redo_index - 1);

	redo_log_process(pop->redo, redo, REDO_NUM_ENTRIES);

	pmemobj_mutex_unlock_nofail(pop, &head->lock);
err:
	lane_release(pop);

	ASSERT(ret == 0 || ret == -1);
	return ret;
}

/*
 * list_remove_free -- remove from two lists and free an object
 *
 * pop         - pmemobj pool handle
 * oob_head    - oob list head
 * pe_offset   - offset to list entry on user list relative to user data
 * user_head   - user list head, *must* be locked if not NULL
 * oidp        - pointer to target object ID
 */
static void
list_remove_free(PMEMobjpool *pop, size_t pe_offset,
	struct list_head *user_head, PMEMoid *oidp)
{
	LOG(3, NULL);
	ASSERT(user_head != NULL);

#ifdef DEBUG
	int r = pmemobj_mutex_assert_locked(pop, &user_head->lock);
	ASSERTeq(r, 0);
#endif

	struct lane_section *lane_section;

	lane_hold(pop, &lane_section, LANE_SECTION_LIST);

	ASSERTne(lane_section, NULL);
	ASSERTne(lane_section->layout, NULL);

	struct lane_list_layout *section =
		(struct lane_list_layout *)lane_section->layout;
	uint64_t sec_off_off = OBJ_PTR_TO_OFF(pop, &section->obj_offset);
	struct redo_log *redo = section->redo;
	size_t redo_index = 0;

	uint64_t obj_doffset = oidp->off;

	ASSERT((ssize_t)pe_offset >= 0);

	struct list_entry *entry_ptr =
		(struct list_entry *)OBJ_OFF_TO_PTR(pop,
				obj_doffset + pe_offset);

	struct list_args_remove args = {
		.pe_offset = (ssize_t)pe_offset,
		.head = user_head,
		.entry_ptr = entry_ptr,
		.obj_doffset = obj_doffset
	};

	/* remove from user list */
	redo_index = list_remove_single(pop, redo, redo_index, &args);

	/* clear the oid */
	if (OBJ_PTR_IS_VALID(pop, oidp))
		redo_index = list_set_oid_redo_log(pop, redo, redo_index,
				oidp, 0, 1);
	else
		oidp->off = 0;

	redo_log_store_last(pop->redo, redo, redo_index, sec_off_off,
			obj_doffset);

	redo_log_process(pop->redo, redo, REDO_NUM_ENTRIES);

	/*
	 * Don't need to fill next and prev offsets of removing element
	 * because the element is freed.
	 */
	pfree(pop, &section->obj_offset);

	lane_release(pop);
}

/*
 * list_remove_free_user -- remove from two lists and free an object
 *
 * pop         - pmemobj pool handle
 * oob_head    - oob list head
 * pe_offset   - offset to list entry on user list relative to user data
 * user_head   - user list head
 * oidp        - pointer to target object ID
 */
int
list_remove_free_user(PMEMobjpool *pop, size_t pe_offset,
	struct list_head *user_head, PMEMoid *oidp)
{
	LOG(3, NULL);

	int ret;
	if ((ret = pmemobj_mutex_lock(pop, &user_head->lock))) {
		errno = ret;
		LOG(2, "pmemobj_mutex_lock failed");
		return -1;
	}

	list_remove_free(pop, pe_offset, user_head, oidp);

	pmemobj_mutex_unlock_nofail(pop, &user_head->lock);

	return 0;
}

/*
 * list_remove -- remove object from list
 *
 * pop          - pmemobj handle
 * pe_offset    - offset to list entry on user list relative to user data
 * head         - list head
 * oid          - target object ID
 */
int
list_remove(PMEMobjpool *pop,
	ssize_t pe_offset, struct list_head *head,
	PMEMoid oid)
{
	LOG(3, NULL);
	ASSERTne(head, NULL);

	int ret;

	struct lane_section *lane_section;

	lane_hold(pop, &lane_section, LANE_SECTION_LIST);

	ASSERTne(lane_section, NULL);
	ASSERTne(lane_section->layout, NULL);

	if ((ret = pmemobj_mutex_lock(pop, &head->lock))) {
		errno = ret;
		LOG(2, "pmemobj_mutex_lock failed");
		ret = -1;
		goto err;
	}

	struct lane_list_layout *section =
		(struct lane_list_layout *)lane_section->layout;
	struct redo_log *redo = section->redo;
	size_t redo_index = 0;

	struct list_entry *entry_ptr =
		(struct list_entry *)OBJ_OFF_TO_PTR(pop,
				oid.off + (size_t)pe_offset);

	struct list_args_remove args = {
		.pe_offset = (ssize_t)pe_offset,
		.head = head,
		.entry_ptr = entry_ptr,
		.obj_doffset = oid.off,
	};

	struct list_args_common args_common = {
		.obj_doffset = oid.off,
		.entry_ptr = entry_ptr,
		.pe_offset = (ssize_t)pe_offset,
	};

	/* remove element from user list */
	redo_index = list_remove_single(pop, redo, redo_index, &args);

	/* clear next and prev offsets in removing element using redo log */
	redo_index = list_fill_entry_redo_log(pop, redo, redo_index,
			&args_common, 0, 0, 0);

	redo_log_set_last(pop->redo, redo, redo_index - 1);

	redo_log_process(pop->redo, redo, REDO_NUM_ENTRIES);

	pmemobj_mutex_unlock_nofail(pop, &head->lock);
err:
	lane_release(pop);

	ASSERT(ret == 0 || ret == -1);
	return ret;
}

/*
 * list_move -- move object between two lists
 *
 * pop           - pmemobj handle
 * pe_offset_old - offset to old list entry relative to user data
 * head_old      - old list head
 * pe_offset_new - offset to new list entry relative to user data
 * head_new      - new list head
 * dest          - destination object ID
 * before        - before/after destination
 * oid           - target object ID
 */
int
list_move(PMEMobjpool *pop,
	size_t pe_offset_old, struct list_head *head_old,
	size_t pe_offset_new, struct list_head *head_new,
	PMEMoid dest, int before, PMEMoid oid)
{
	LOG(3, NULL);
	ASSERTne(head_old, NULL);
	ASSERTne(head_new, NULL);

	int ret;

	struct lane_section *lane_section;

	lane_hold(pop, &lane_section, LANE_SECTION_LIST);

	ASSERTne(lane_section, NULL);
	ASSERTne(lane_section->layout, NULL);

	/*
	 * Grab locks in specified order to avoid dead-locks.
	 *
	 * XXX performance improvement: initialize oob locks at pool opening
	 */
	if ((ret = list_mutexes_lock(pop, head_new, head_old))) {
		errno = ret;
		LOG(2, "list_mutexes_lock failed");
		ret = -1;
		goto err;
	}

	struct lane_list_layout *section =
		(struct lane_list_layout *)lane_section->layout;
	struct redo_log *redo = section->redo;
	size_t redo_index = 0;

	dest = list_get_dest(pop, head_new, dest,
		(ssize_t)pe_offset_new, before);

	struct list_entry *entry_ptr_old =
		(struct list_entry *)OBJ_OFF_TO_PTR(pop,
				oid.off + pe_offset_old);

	struct list_entry *entry_ptr_new =
		(struct list_entry *)OBJ_OFF_TO_PTR(pop,
				oid.off + pe_offset_new);

	struct list_entry *dest_entry_ptr =
		(struct list_entry *)OBJ_OFF_TO_PTR(pop,
				dest.off + pe_offset_new);

	if (head_old == head_new) {
		/* moving within the same list */

		if (dest.off == oid.off)
			goto unlock;

		if (before && dest_entry_ptr->pe_prev.off == oid.off) {
			if (head_old->pe_first.off != dest.off)
				goto unlock;

			redo_index = list_update_head(pop, redo, redo_index,
					head_old, oid.off);

			goto redo_last;
		}

		if (!before && dest_entry_ptr->pe_next.off == oid.off) {
			if (head_old->pe_first.off != oid.off)
				goto unlock;

			redo_index = list_update_head(pop, redo, redo_index,
					head_old, entry_ptr_old->pe_next.off);

			goto redo_last;
		}
	}

	ASSERT((ssize_t)pe_offset_old >= 0);
	struct list_args_remove args_remove = {
		.pe_offset = (ssize_t)pe_offset_old,
		.head = head_old,
		.entry_ptr = entry_ptr_old,
		.obj_doffset = oid.off,
	};

	struct list_args_insert args_insert = {
		.head = head_new,
		.dest = dest,
		.dest_entry_ptr = dest_entry_ptr,
		.before = before,
	};

	ASSERT((ssize_t)pe_offset_new >= 0);
	struct list_args_common args_common = {
		.obj_doffset = oid.off,
		.entry_ptr = entry_ptr_new,
		.pe_offset = (ssize_t)pe_offset_new,
	};

	uint64_t next_offset;
	uint64_t prev_offset;

	/* remove element from user list */
	redo_index = list_remove_single(pop, redo, redo_index, &args_remove);

	/* insert element to user list */
	redo_index = list_insert_user(pop, redo, redo_index, &args_insert,
			&args_common, &next_offset, &prev_offset);

	/* offsets differ, move is between different list entries - set uuid */
	int set_uuid = pe_offset_new != pe_offset_old ? 1 : 0;

	/* fill next and prev offsets of moving element using redo log */
	redo_index = list_fill_entry_redo_log(pop, redo, redo_index,
			&args_common, next_offset, prev_offset, set_uuid);

redo_last:
	redo_log_set_last(pop->redo, redo, redo_index - 1);

	redo_log_process(pop->redo, redo, REDO_NUM_ENTRIES);

unlock:
	list_mutexes_unlock(pop, head_new, head_old);
err:
	lane_release(pop);

	ASSERT(ret == 0 || ret == -1);
	return ret;
}

/*
 * lane_list_recovery -- (internal) recover the list section of the lane
 */
static int
lane_list_recovery(PMEMobjpool *pop, void *data, unsigned length)
{
	LOG(3, "list lane %p", data);

	struct lane_list_layout *section = data;
	ASSERT(sizeof(*section) <= length);

	redo_log_recover(pop->redo, section->redo, REDO_NUM_ENTRIES);

	if (section->obj_offset) {
		/* alloc or free recovery */
		pfree(pop, &section->obj_offset);
	}

	return 0;
}

/*
 * lane_list_check -- (internal) check consistency of lane
 */
static int
lane_list_check(PMEMobjpool *pop, void *data, unsigned length)
{
	LOG(3, "list lane %p", data);

	struct lane_list_layout *section = data;

	int ret = 0;
	if ((ret = redo_log_check(pop->redo,
			section->redo, REDO_NUM_ENTRIES)) != 0) {
		ERR("list lane: redo log check failed");
		ASSERT(ret == 0 || ret == -1);
		return ret;
	}

	if (section->obj_offset &&
	    !OBJ_OFF_FROM_HEAP(pop, section->obj_offset)) {
		ERR("list lane: invalid offset 0x%" PRIx64,
				section->obj_offset);

		return -1;
	}

	return 0;
}

/*
 * lane_list_construct_rt -- (internal) construct runtime part of list section
 */
static void *
lane_list_construct_rt(PMEMobjpool *pop)
{
	return NULL;
}

/*
 * lane_list_destroy_rt -- (internal) destroy runtime part of list section
 */
static void
lane_list_destroy_rt(PMEMobjpool *pop, void *rt)
{
	/* NOP */
}

/*
 * lane_list_boot -- initializes list section
 */
static int
lane_list_boot(PMEMobjpool *pop)
{
	/* NOP */
	return 0;
}

static struct section_operations list_ops = {
	.construct_rt = lane_list_construct_rt,
	.destroy_rt = lane_list_destroy_rt,
	.recover = lane_list_recovery,
	.check = lane_list_check,
	.boot = lane_list_boot
};

SECTION_PARM(LANE_SECTION_LIST, &list_ops);
