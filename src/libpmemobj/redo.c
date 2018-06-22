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
 * Operation flag at the two least significant bits
 */
#define REDO_OPERATION(op)		((uint64_t)(op))
#define REDO_OPERATION_MASK		((uint64_t)(0b11))
#define REDO_OPERATION_FROM_OFFSET(off)	((off) & REDO_OPERATION_MASK)
#define REDO_OFFSET_MASK		(~(REDO_OPERATION_MASK))

#define CACHELINE_ALIGN(size) ALIGN_UP(size, CACHELINE_SIZE)

struct redo_ctx {
	void *base;

	struct pmem_ops p_ops;

	redo_check_offset_fn check_offset;
	void *check_offset_ctx;
};

typedef int (*redo_entry_cb)(const struct redo_ctx *ctx,
	struct redo_log_entry *e, void *arg);

/*
 * redo_log_config_new -- allocates redo context
 */
struct redo_ctx *
redo_log_config_new(void *base,
		const struct pmem_ops *p_ops,
		redo_check_offset_fn check_offset,
		void *check_offset_ctx)
{
	struct redo_ctx *cfg = Malloc(sizeof(*cfg));
	if (!cfg) {
		ERR("!can't create redo log config");
		return NULL;
	}

	cfg->base = base;
	cfg->p_ops = *p_ops;
	cfg->check_offset = check_offset;
	cfg->check_offset_ctx = check_offset_ctx;

	return cfg;
}

/*
 * redo_log_config_delete -- frees redo context
 */
void
redo_log_config_delete(struct redo_ctx *ctx)
{
	Free(ctx);
}

/*
 * redo_log_next_by_offset -- calculates the next pointer
 */
static struct redo_log *
redo_log_next_by_offset(const struct redo_ctx *ctx, size_t offset)
{
	return offset == 0 ? NULL :
		(struct redo_log *)((char *)ctx->base + offset);
}

/*
 * redo_log_next -- retrieves the pointer to the next redo log
 */
static struct redo_log *
redo_log_next(const struct redo_ctx *ctx, struct redo_log *redo)
{
	return redo_log_next_by_offset(ctx, redo->next);
}

/*
 * redo_log_foreach_entry -- iterates over every existing entry in the redo log
 */
static int
redo_log_foreach_entry(const struct redo_ctx *ctx, struct redo_log *redo,
	redo_entry_cb cb, void *arg)
{
	struct redo_log_entry *e;
	int ret = 0;
	size_t nentries = 0;

	for (struct redo_log *r = redo; r != NULL; r = redo_log_next(ctx, r)) {
		for (size_t i = 0; i < r->capacity; ++i) {
			if (nentries == redo->nentries)
				return ret;

			nentries++;

			e = &r->entries[i];
			if ((ret = cb(ctx, e, arg)) != 0)
				return ret;
		}
	}

	return ret;
}

/*
 * redo_log_nentries -- returns number of entries in the redo log
 */
size_t
redo_log_nentries(struct redo_log *redo)
{
	return redo->nentries;
}

/*
 * redo_log_capacity -- (internal) returns the total capacity of the redo log
 */
size_t
redo_log_capacity(const struct redo_ctx *ctx,
	struct redo_log *redo, size_t redo_base_capacity)
{
	size_t capacity = redo_base_capacity;

	/* skip the first one, we count it in 'redo_base_capacity' */
	while ((redo = redo_log_next(ctx, redo)) != NULL) {
		capacity += redo->capacity;
	}

	return capacity;
}

/*
 * redo_log_rebuild_next_vec -- rebuilds the vector of next entries
 */
void
redo_log_rebuild_next_vec(const struct redo_ctx *ctx,
	struct redo_log *redo, struct redo_next *next)
{
	do {
		if (redo->next != 0)
			VEC_PUSH_BACK(next, redo->next);
	} while ((redo = redo_log_next(ctx, redo)) != NULL);
}

/*
 * redo_log_reserve -- (internal) reserves new capacity in the redo log
 */
int
redo_log_reserve(const struct redo_ctx *ctx, struct redo_log *redo,
	size_t redo_base_capacity, size_t *new_capacity, redo_extend_fn extend,
	struct redo_next *next)
{
	size_t capacity = redo_base_capacity;

	uint64_t offset;
	VEC_FOREACH(offset, next) {
		redo = redo_log_next_by_offset(ctx, offset);
		capacity += redo->capacity;
	}

	while (capacity < *new_capacity) {
		if (extend(ctx->base, &redo->next) != 0)
			return -1;
		VEC_PUSH_BACK(next, redo->next);
		redo = redo_log_next(ctx, redo);
		capacity += redo->capacity;
	}
	*new_capacity = capacity;

	return 0;
}

/*
 * redo_log_checksum -- (internal) calculates redo log checksum
 */
static int
redo_log_checksum(struct redo_log *redo, size_t nentries, int insert)
{
	return util_checksum(redo, SIZEOF_REDO_LOG(nentries),
		&redo->checksum, insert, 0);
}

/*
 * redo_log_store -- (internal) stores the transient src redo log in the
 *	persistent dest redo log
 *
 * The source and destination redo logs must be cacheline aligned.
 */
void
redo_log_store(const struct redo_ctx *ctx, struct redo_log *dest,
	struct redo_log *src, size_t nentries, size_t redo_base_capacity,
	struct redo_next *next)
{
	/*
	 * First, store all entries over the base capacity of the redo log in
	 * the next logs.
	 * Because the checksum is only in the first part, we don't have to
	 * worry about failsafety here.
	 */
	struct redo_log *redo = dest;
	size_t offset = redo_base_capacity;
	size_t dest_ncopy = MIN(nentries, redo_base_capacity);
	size_t next_entries = nentries - dest_ncopy;
	size_t nlog = 0;

	while (next_entries > 0) {
		redo = redo_log_next_by_offset(ctx, VEC_ARR(next)[nlog++]);
		ASSERTne(redo, NULL);

		size_t ncopy = MIN(next_entries, redo->capacity);
		next_entries -= ncopy;

		pmemops_memcpy(&ctx->p_ops,
			redo->entries,
			src->entries + offset,
			CACHELINE_ALIGN(sizeof(struct redo_log_entry) * ncopy),
			PMEMOBJ_F_MEM_WC |
			PMEMOBJ_F_MEM_NODRAIN |
			PMEMOBJ_F_RELAXED);
		offset += ncopy;
	}

	if (nlog != 0)
		pmemops_drain(&ctx->p_ops);

	/*
	 * Then, calculate the checksum and store the first part of the
	 * redo log.
	 */
	src->next = VEC_SIZE(next) == 0 ? 0 : VEC_FRONT(next);
	src->nentries = nentries; /* total number of entries */
	redo_log_checksum(src, dest_ncopy, 1);

	pmemops_memcpy(&ctx->p_ops, dest, src,
		CACHELINE_ALIGN(SIZEOF_REDO_LOG(dest_ncopy)),
		PMEMOBJ_F_MEM_WC);
}

/*
 * redo_log_entry_create -- creates a new transient redo log entry
 */
void
redo_log_entry_create(const void *base,
	struct redo_log_entry *entry, uint64_t *ptr, uint64_t value,
	enum redo_operation_type type)
{
	entry->offset = (uint64_t)(ptr) - (uint64_t)base;
	entry->offset |= REDO_OPERATION(type);
	entry->value = value;
}

/*
 * redo_log_operation -- returns the type of entry operation
 */
enum redo_operation_type
redo_log_operation(const struct redo_log_entry *entry)
{
	return REDO_OPERATION_FROM_OFFSET(entry->offset);
}

/*
 * redo_log_offset -- returns offset
 */
uint64_t
redo_log_offset(const struct redo_log_entry *entry)
{
	return entry->offset & REDO_OFFSET_MASK;
}

/*
 * redo_log_entry_apply -- applies modifications of a single redo log entry
 */
void
redo_log_entry_apply(void *base, const struct redo_log_entry *e,
	flush_fn flush)
{
	enum redo_operation_type t = redo_log_operation(e);
	uint64_t offset = redo_log_offset(e);

	uint64_t *val = (uint64_t *)((uintptr_t)base + offset);
	VALGRIND_ADD_TO_TX(val, sizeof(*val));
	switch (t) {
		case REDO_OPERATION_AND:
			*val &= e->value;
		break;
		case REDO_OPERATION_OR:
			*val |= e->value;
		break;
		case REDO_OPERATION_SET:
			*val = e->value;
		break;
		default:
			ASSERT(0);
	}
	VALGRIND_REMOVE_FROM_TX(val, sizeof(*val));

	flush(base, val, sizeof(uint64_t), PMEMOBJ_F_RELAXED);
}

/*
 * redo_log_process_entry -- processes a single redo log entry
 */
static int
redo_log_process_entry(const struct redo_ctx *ctx,
	struct redo_log_entry *e, void *arg)
{
	redo_log_entry_apply(ctx->base, e, ctx->p_ops.flush);

	return 0;
}

/*
 * redo_log_clobber -- zeroes the metadata of the redo log
 */
void
redo_log_clobber(const struct redo_ctx *ctx, struct redo_log *dest,
	struct redo_next *next)
{
	struct redo_log empty;
	memset(&empty, 0, sizeof(empty));

	if (next != NULL)
		empty.next = VEC_SIZE(next) == 0 ? 0 : VEC_FRONT(next);
	else
		empty.next = dest->next;

	pmemops_memcpy(&ctx->p_ops, dest, &empty, sizeof(empty),
		PMEMOBJ_F_MEM_WC);
}

/*
 * redo_log_process -- (internal) process redo log entries
 */
void
redo_log_process(const struct redo_ctx *ctx, struct redo_log *redo)
{
	LOG(15, "redo %p", redo);

#ifdef DEBUG
	ASSERTeq(redo_log_check(ctx, redo), 0);
#endif
	redo_log_foreach_entry(ctx, redo, redo_log_process_entry, NULL);
}

/*
 * redo_log_recover -- (internal) recovery of redo log
 *
 * The redo_log_recover shall be preceded by redo_log_check call.
 */
void
redo_log_recover(const struct redo_ctx *ctx, struct redo_log *redo)
{
	LOG(15, "redo %p", redo);
	ASSERTne(ctx, NULL);

	size_t nentries = MIN(redo->nentries, redo->capacity);
	if (nentries != 0 && redo_log_checksum(redo, nentries, 0)) {
		redo_log_process(ctx, redo);
		redo_log_clobber(ctx, redo, NULL);
	}
}

/*
 * redo_log_check_entry -- checks consistency of a single redo log entry
 */
static int
redo_log_check_entry(const struct redo_ctx *ctx, struct redo_log_entry *e,
	void *arg)
{
	uint64_t offset = redo_log_offset(e);
	if (!ctx->check_offset(ctx->check_offset_ctx, offset)) {
		LOG(15, "redo %p invalid offset %" PRIu64,
				e, e->offset);
		return -1;
	}
	return 0;
}

/*
 * redo_log_check -- (internal) check consistency of redo log entries
 */
int
redo_log_check(const struct redo_ctx *ctx, struct redo_log *redo)
{
	LOG(15, "redo %p", redo);
	ASSERTne(ctx, NULL);

	if (redo->nentries != 0)
		return redo_log_foreach_entry(ctx, redo,
			redo_log_check_entry, NULL);

	return 0;
}

/*
 * redo_get_pmem_ops -- returns pmem_ops
 */
const struct pmem_ops *
redo_get_pmem_ops(const struct redo_ctx *ctx)
{
	return &ctx->p_ops;
}
