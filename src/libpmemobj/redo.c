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
 * redo.c -- redo log implementation
 */

#include <inttypes.h>
#include <string.h>

#include "libpmemobj.h"
#include "redo.h"
#include "out.h"
#include "util.h"
#include "valgrind_internal.h"

/*
 * Operation flag at the three least significant bits
 */
#define REDO_OPERATION(op)		((uint64_t)(op))
#define REDO_OPERATION_MASK		((uint64_t)(0b111))
#define REDO_OPERATION_FROM_OFFSET(off)	((off) & REDO_OPERATION_MASK)
#define REDO_OFFSET_MASK		(~(REDO_OPERATION_MASK))

#define CACHELINE_ALIGN(size) ALIGN_UP(size, CACHELINE_SIZE)

/*
 * redo_log_next_by_offset -- (internal) calculates the next pointer
 */
static struct redo_log *
redo_log_next_by_offset(size_t offset, const struct pmem_ops *p_ops)
{
	return offset == 0 ? NULL :
		(struct redo_log *)((char *)p_ops->base + offset);
}

/*
 * redo_log_next -- (internal) retrieves the pointer to the next redo log
 */
static struct redo_log *
redo_log_next(struct redo_log *redo, const struct pmem_ops *p_ops)
{
	return redo_log_next_by_offset(redo->next, p_ops);
}


/*
 * redo_log_operation -- returns the type of entry operation
 */
enum redo_operation_type
redo_log_entry_type(const struct redo_log_entry_base *entry)
{
	return REDO_OPERATION_FROM_OFFSET(entry->offset);
}

/*
 * redo_log_offset -- returns offset
 */
uint64_t
redo_log_entry_offset(const struct redo_log_entry_base *entry)
{
	return entry->offset & REDO_OFFSET_MASK;
}

/*
 * redo_log_entry_size -- returns the size of a redo log entry
 */
size_t
redo_log_entry_size(const struct redo_log_entry_base *entry)
{
	enum redo_operation_type t = redo_log_entry_type(entry);
	struct redo_log_entry_buf *eb;

	switch (t) {
		case REDO_OPERATION_AND:
		case REDO_OPERATION_OR:
		case REDO_OPERATION_SET:
			return sizeof(struct redo_log_entry_val);
		case REDO_OPERATION_BUF_SET:
		case REDO_OPERATION_BUF_CPY:
			eb = (struct redo_log_entry_buf *)entry;
			return CACHELINE_ALIGN(
				sizeof(struct redo_log_entry_buf) + eb->size);
		default:
			ASSERT(0);
	}

	return 0;
}

/*
 * redo_log_entry_valid -- (internal) checks if a redo log entry is valid
 * Returns 1 if the range is valid, otherwise 0 is returned.
 */
static int
redo_log_entry_valid(const struct redo_log_entry_base *entry)
{
	if (entry->offset == 0)
		return 0;

	size_t size;
	struct redo_log_entry_buf *b;

	switch (redo_log_entry_type(entry)) {
		case REDO_OPERATION_BUF_CPY:
		case REDO_OPERATION_BUF_SET:
			size = redo_log_entry_size(entry);
			b = (struct redo_log_entry_buf *)entry;
			if (!util_checksum(b, size, &b->checksum, 0, 0))
				return 0;
			break;
		default:
			break;
	}

	return 1;
}

/*
 * redo_log_foreach_entry -- iterates over every existing entry in the redo log
 */
int
redo_log_foreach_entry(struct redo_log *redo,
	redo_entry_cb cb, void *arg, const struct pmem_ops *ops)
{
	struct redo_log_entry_base *e;
	int ret = 0;

	for (struct redo_log *r = redo; r != NULL; r = redo_log_next(r, ops)) {
		for (size_t offset = 0; offset < r->capacity; ) {
			e = (struct redo_log_entry_base *)(r->data + offset);
			if (!redo_log_entry_valid(e))
				return ret;

			if ((ret = cb(e, arg, ops)) != 0)
				return ret;

			offset += redo_log_entry_size(e);
		}
	}

	return ret;
}

/*
 * redo_log_capacity -- (internal) returns the total capacity of the redo log
 */
size_t
redo_log_capacity(struct redo_log *redo, size_t redo_base_bytes,
	const struct pmem_ops *p_ops)
{
	size_t capacity = redo_base_bytes;

	/* skip the first one, we count it in 'redo_base_bytes' */
	while ((redo = redo_log_next(redo, p_ops)) != NULL) {
		capacity += redo->capacity;
	}

	return capacity;
}

/*
 * redo_log_rebuild_next_vec -- rebuilds the vector of next entries
 */
void
redo_log_rebuild_next_vec(struct redo_log *redo, struct redo_next *next,
	const struct pmem_ops *p_ops)
{
	do {
		if (redo->next != 0)
			VEC_PUSH_BACK(next, redo->next);
	} while ((redo = redo_log_next(redo, p_ops)) != NULL);
}

/*
 * redo_log_reserve -- reserves new capacity in the redo log
 */
int
redo_log_reserve(struct redo_log *redo,
	size_t redo_base_nbytes, size_t *new_capacity, redo_extend_fn extend,
	struct redo_next *next,
	const struct pmem_ops *p_ops)
{
	size_t capacity = redo_base_nbytes;

	uint64_t offset;
	VEC_FOREACH(offset, next) {
		redo = redo_log_next_by_offset(offset, p_ops);
		capacity += redo->capacity;
	}

	while (capacity < *new_capacity) {
		if (extend(p_ops->base, &redo->next) != 0)
			return -1;
		VEC_PUSH_BACK(next, redo->next);
		redo = redo_log_next(redo, p_ops);
		capacity += redo->capacity;
	}
	*new_capacity = capacity;

	return 0;
}

/*
 * redo_log_checksum -- (internal) calculates redo log checksum
 */
static int
redo_log_checksum(struct redo_log *redo, size_t redo_base_bytes, int insert)
{
	return util_checksum(redo, SIZEOF_REDO_LOG(redo_base_bytes),
		&redo->checksum, insert, 0);
}

/*
 * redo_log_store -- stores the transient src redo log in the
 *	persistent dest redo log
 *
 * The source and destination redo logs must be cacheline aligned.
 */
void
redo_log_store(struct redo_log *dest, struct redo_log *src, size_t nbytes,
	size_t redo_base_nbytes, struct redo_next *next,
	const struct pmem_ops *p_ops)
{
	/*
	 * First, store all entries over the base capacity of the redo log in
	 * the next logs.
	 * Because the checksum is only in the first part, we don't have to
	 * worry about failsafety here.
	 */
	struct redo_log *redo = dest;
	size_t offset = redo_base_nbytes;

	/*
	 * Copy at least 8 bytes more than needed. If the user always
	 * properly uses entry creation functions, this will zero-out the
	 * potential leftovers of the previous log. Since all we really need
	 * to zero is the offset, sizeof(struct redo_log_entry_base) is enough.
	 * If the nbytes is aligned, an entire cacheline needs to be addtionally
	 * zeroed.
	 * But the checksum must be calculated based solely on actual data.
	 */
	size_t checksum_nbytes = MIN(redo_base_nbytes, nbytes);
	nbytes = CACHELINE_ALIGN(nbytes + sizeof(struct redo_log_entry_base));

	size_t base_nbytes = MIN(redo_base_nbytes, nbytes);
	size_t next_nbytes = nbytes - base_nbytes;

	size_t nlog = 0;

	while (next_nbytes > 0) {
		redo = redo_log_next_by_offset(VEC_ARR(next)[nlog++], p_ops);
		ASSERTne(redo, NULL);

		size_t copy_nbytes = MIN(next_nbytes, redo->capacity);
		next_nbytes -= copy_nbytes;

		VALGRIND_ADD_TO_TX(redo->data, copy_nbytes);
		pmemops_memcpy(p_ops,
			redo->data,
			src->data + offset,
			copy_nbytes,
			PMEMOBJ_F_MEM_WC |
			PMEMOBJ_F_MEM_NODRAIN |
			PMEMOBJ_F_RELAXED);
		VALGRIND_REMOVE_FROM_TX(redo->data, copy_nbytes);
		offset += copy_nbytes;
	}

	if (nlog != 0)
		pmemops_drain(p_ops);

	/*
	 * Then, calculate the checksum and store the first part of the
	 * redo log.
	 */
	src->next = VEC_SIZE(next) == 0 ? 0 : VEC_FRONT(next);
	redo_log_checksum(src, checksum_nbytes, 1);

	pmemops_memcpy(p_ops, dest, src,
		SIZEOF_REDO_LOG(base_nbytes),
		PMEMOBJ_F_MEM_WC);
}

/*
 * redo_log_entry_val_create -- creates a new log value entry in the redo
 *
 * This function requires at least a cacheline of space to be available in the
 * redo log.
 */
struct redo_log_entry_val *
redo_log_entry_val_create(struct redo_log *redo, size_t offset, uint64_t *dest,
	uint64_t value, enum redo_operation_type type,
	const struct pmem_ops *p_ops)
{
	struct redo_log_entry_val *e =
		(struct redo_log_entry_val *)(redo->data + offset);

	struct {
		struct redo_log_entry_val v;
		struct redo_log_entry_base zeroes;
	} data;
	COMPILE_ERROR_ON(sizeof(data) != sizeof(data.v) + sizeof(data.zeroes));

	/*
	 * Write a little bit more to the buffer so that the next entry that
	 * resides in the log is erased. This will prevent leftovers from
	 * a previous, clobbered, log from being incorrectly applied.
	 */
	data.zeroes.offset = 0;
	data.v.base.offset = (uint64_t)(dest) - (uint64_t)p_ops->base;
	data.v.base.offset |= REDO_OPERATION(type);
	data.v.value = value;

	pmemops_memcpy(p_ops, e, &data, sizeof(data),
		PMEMOBJ_F_MEM_NOFLUSH | PMEMOBJ_F_RELAXED);

	return e;
}

/*
 * redo_log_entry_buf_create -- atomically creates a buffer entry in the log
 */
struct redo_log_entry_buf *
redo_log_entry_buf_create(struct redo_log *redo, size_t offset, uint64_t *dest,
	const void *src, uint64_t size,
	enum redo_operation_type type, const struct pmem_ops *p_ops)
{
	struct redo_log_entry_buf *e =
		(struct redo_log_entry_buf *)(redo->data + offset);

	/*
	 * Depending on the size of the source buffer, we might need to perform
	 * up to three separate copies:
	 *	1. The first cacheline, 24b of metadata and 40b of data
	 * If there's still data to be logged:
	 *	2. The entire remainder of data data aligned down to cacheline,
	 *	for example, if there's 150b left, this step will copy only
	 *	128b.
	 * Now, we are left with between 0 to 63 bytes. If nonzero:
	 *	3. Create a stack allocated cacheline-sized buffer, fill in the
	 *	remainder of the data, and copy the entire cacheline.
	 *
	 * This is done so that we avoid a cache-miss on misaligned writes.
	 */

	struct redo_log_entry_buf *b = alloca(CACHELINE_SIZE);
	b->base.offset = (uint64_t)(dest) - (uint64_t)p_ops->base;
	b->base.offset |= REDO_OPERATION(type);
	b->size = size;

	size_t ncopy = MIN(size,
		CACHELINE_SIZE - sizeof(struct redo_log_entry_buf));
	memcpy(b->data, src, ncopy);

	size_t remaining_size = ncopy > size ? 0 : size - ncopy;

	char *srcof = (char *)src + ncopy;
	size_t rcopy = ALIGN_DOWN(remaining_size, CACHELINE_SIZE);
	size_t lcopy = remaining_size - rcopy;

	uint8_t last_cacheline[CACHELINE_SIZE];
	if (lcopy != 0)
		memcpy(last_cacheline, srcof + rcopy, lcopy);

	b->checksum = util_checksum_seq(b, CACHELINE_SIZE, 0);
	if (rcopy != 0)
		b->checksum = util_checksum_seq(srcof, rcopy, b->checksum);
	if (lcopy != 0)
		b->checksum = util_checksum_seq(last_cacheline,
			CACHELINE_SIZE, b->checksum);

	pmemops_memcpy(p_ops, e, b, CACHELINE_SIZE,
		PMEMOBJ_F_MEM_NODRAIN | PMEMOBJ_F_MEM_NONTEMPORAL);

	if (rcopy != 0)
		pmemops_memcpy(p_ops, e->data + ncopy, srcof, rcopy,
			PMEMOBJ_F_MEM_NODRAIN | PMEMOBJ_F_MEM_NONTEMPORAL);

	if (lcopy != 0)
		pmemops_memcpy(p_ops, e->data + ncopy + rcopy, last_cacheline,
			CACHELINE_SIZE,
			PMEMOBJ_F_MEM_NODRAIN | PMEMOBJ_F_MEM_NONTEMPORAL);

	pmemops_drain(p_ops);

	return e;
}

/*
 * redo_log_entry_apply -- applies modifications of a single redo log entry
 */
void
redo_log_entry_apply(const struct redo_log_entry_base *e, int persist,
	const struct pmem_ops *p_ops)
{
	enum redo_operation_type t = redo_log_entry_type(e);
	uint64_t offset = redo_log_entry_offset(e);

	size_t dst_size = sizeof(uint64_t);
	uint64_t *dst = (uint64_t *)((uintptr_t)p_ops->base + offset);

	struct redo_log_entry_val *ev;
	struct redo_log_entry_buf *eb;

	flush_fn f = persist ? p_ops->persist : p_ops->flush;

	switch (t) {
		case REDO_OPERATION_AND:
			ev = (struct redo_log_entry_val *)e;

			VALGRIND_ADD_TO_TX(dst, dst_size);
			*dst &= ev->value;
			f(p_ops->base, dst, sizeof(uint64_t),
				PMEMOBJ_F_RELAXED);
		break;
		case REDO_OPERATION_OR:
			ev = (struct redo_log_entry_val *)e;

			VALGRIND_ADD_TO_TX(dst, dst_size);
			*dst |= ev->value;
			f(p_ops->base, dst, sizeof(uint64_t),
				PMEMOBJ_F_RELAXED);
		break;
		case REDO_OPERATION_SET:
			ev = (struct redo_log_entry_val *)e;

			VALGRIND_ADD_TO_TX(dst, dst_size);
			*dst = ev->value;
			f(p_ops->base, dst, sizeof(uint64_t),
				PMEMOBJ_F_RELAXED);
		break;
		case REDO_OPERATION_BUF_SET:
			eb = (struct redo_log_entry_buf *)e;

			dst_size = eb->size;
			VALGRIND_ADD_TO_TX(dst, dst_size);
			pmemops_memset(p_ops, dst, *eb->data, eb->size,
				PMEMOBJ_F_RELAXED | PMEMOBJ_F_MEM_NODRAIN);
		break;
		case REDO_OPERATION_BUF_CPY:
			eb = (struct redo_log_entry_buf *)e;

			dst_size = eb->size;
			VALGRIND_ADD_TO_TX(dst, dst_size);
			pmemops_memcpy(p_ops, dst, eb->data, eb->size,
				PMEMOBJ_F_RELAXED | PMEMOBJ_F_MEM_NODRAIN);
		break;
		default:
			ASSERT(0);
	}
	VALGRIND_REMOVE_FROM_TX(dst, dst_size);
}

/*
 * redo_log_process_entry -- (internal) processes a single redo log entry
 */
static int
redo_log_process_entry(struct redo_log_entry_base *e, void *arg,
	const struct pmem_ops *p_ops)
{
	redo_log_entry_apply(e, 0, p_ops);

	return 0;
}

/*
 * redo_log_clobber -- zeroes the metadata of the redo log
 */
void
redo_log_clobber(struct redo_log *dest, struct redo_next *next,
	const struct pmem_ops *p_ops)
{
	struct redo_log empty;
	memset(&empty, 0, sizeof(empty));

	if (next != NULL)
		empty.next = VEC_SIZE(next) == 0 ? 0 : VEC_FRONT(next);
	else
		empty.next = dest->next;

	pmemops_memcpy(p_ops, dest, &empty, sizeof(empty),
		PMEMOBJ_F_MEM_WC);
}

/*
 * redo_log_clobber_data -- zeroes out 'nbytes' of data in the logs
 */
void
redo_log_clobber_data(struct redo_log *dest,
	size_t nbytes, size_t redo_base_nbytes,
	struct redo_next *next, const struct pmem_ops *p_ops)
{
	size_t rcapacity = redo_base_nbytes;
	size_t nlog = 0;

	for (struct redo_log *r = dest; r != NULL; ) {
		size_t nzero = MIN(nbytes, rcapacity);
		pmemops_memset(p_ops, r->data, 0, nzero, PMEMOBJ_F_MEM_WC);
		nbytes -= nzero;

		if (nbytes == 0)
			break;

		r = redo_log_next_by_offset(VEC_ARR(next)[nlog++], p_ops);
		ASSERTne(r, NULL);
		rcapacity = r->capacity;
	}
}

/*
 * redo_log_process -- process redo log entries
 */
void
redo_log_process(struct redo_log *redo, redo_check_offset_fn check,
	const struct pmem_ops *p_ops)
{
	LOG(15, "redo %p", redo);

#ifdef DEBUG
	redo_log_check(redo, check, p_ops);
#endif

	redo_log_foreach_entry(redo, redo_log_process_entry, NULL, p_ops);
}

/*
 * redo_log_base_nbytes -- (internal) counts the actual of number of bytes
 *	occupied by the redo log
 */
static size_t
redo_log_base_nbytes(struct redo_log *redo)
{
	size_t offset = 0;
	struct redo_log_entry_base *e;

	for (offset = 0; offset < redo->capacity; ) {
		e = (struct redo_log_entry_base *)(redo->data + offset);
		if (!redo_log_entry_valid(e))
			break;

		offset += redo_log_entry_size(e);
	}

	return offset;
}

/*
 * redo_log_recovery_needed -- checks if the logs needs recovery
 */
int
redo_log_recovery_needed(struct redo_log *redo,
	const struct pmem_ops *p_ops)
{
	size_t nbytes = MIN(redo_log_base_nbytes(redo), redo->capacity);
	return nbytes != 0 && redo_log_checksum(redo, nbytes, 0);
}

/*
 * redo_log_recover -- recovery of redo log
 *
 * The redo_log_recover shall be preceded by redo_log_check call.
 */
void
redo_log_recover(struct redo_log *redo, redo_check_offset_fn check,
	const struct pmem_ops *p_ops)
{
	LOG(15, "redo %p", redo);

	if (redo_log_recovery_needed(redo, p_ops)) {
		redo_log_process(redo, check, p_ops);
		redo_log_clobber(redo, NULL, p_ops);
	}
}

/*
 * redo_log_check_entry --
 *	(internal) checks consistency of a single redo log entry
 */
static int
redo_log_check_entry(struct redo_log_entry_base *e,
	void *arg, const struct pmem_ops *p_ops)
{
	uint64_t offset = redo_log_entry_offset(e);
	redo_check_offset_fn check = arg;

	if (!check(p_ops->base, offset)) {
		LOG(15, "redo %p invalid offset %" PRIu64,
				e, e->offset);
		return -1;
	}

	return offset == 0 ? -1 : 0;
}

/*
 * redo_log_check -- (internal) check consistency of redo log entries
 */
int
redo_log_check(struct redo_log *redo, redo_check_offset_fn check,
	const struct pmem_ops *p_ops)
{
	LOG(15, "redo %p", redo);

	return redo_log_foreach_entry(redo,
			redo_log_check_entry, check, p_ops);
}
