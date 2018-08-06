/*
 * Copyright 2016-2018, Intel Corporation
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
 * memops.c -- aggregated memory operations helper implementation
 *
 * The operation collects all of the required memory modifications that
 * need to happen in an atomic way (all of them or none), and abstracts
 * away the storage type (transient/persistent) and the underlying
 * implementation of how it's actually performed - in some cases using
 * the redo log is unnecessary and the allocation process can be sped up
 * a bit by completely omitting that whole machinery.
 *
 * The modifications are not visible until the context is processed.
 */

#include "memops.h"
#include "obj.h"
#include "out.h"
#include "valgrind_internal.h"
#include "vecq.h"

#define REDO_LOG_BASE_SIZE 1024
#define OP_MERGE_SEARCH 64

struct operation_log {
	size_t capacity; /* capacity of the redo log */
	size_t offset; /* data offset inside of the log */
	struct redo_log *redo; /* DRAM allocated log of modifications */
};

/*
 * operation_context -- context of an ongoing palloc operation
 */
struct operation_context {
	redo_extend_fn extend; /* function to allocate next redo logs */

	const struct pmem_ops *p_ops;
	struct pmem_ops t_ops; /* used for transient data processing */

	struct redo_log *redo; /* pointer to the persistent redo log */
	size_t redo_base_nbytes; /* available bytes in initial redo log */
	size_t redo_capacity; /* sum of capacity, incl all next redo logs */

	struct redo_next next; /* vector of 'next' fields of persistent redo */

	int in_progress; /* operation sanity check */

	struct operation_log pshadow_ops; /* shadow copy of persistent redo */
	struct operation_log transient_ops; /* log of transient changes */

	/* collection used to look for potential merge candidates */
	VECQ(, struct redo_log_entry_val *) merge_entries;
};

/*
 * operation_log_transient_init -- (internal) initialize operation log
 *	containing transient memory resident changes
 */
static int
operation_log_transient_init(struct operation_log *log)
{
	log->capacity = REDO_LOG_BASE_SIZE;
	log->offset = 0;

	struct redo_log *src = Zalloc(sizeof(struct redo_log) +
		REDO_LOG_BASE_SIZE);
	if (src == NULL)
		return -1;

	/* initialize underlying redo log structure */
	src->capacity = REDO_LOG_BASE_SIZE;

	log->redo = src;

	return 0;
}

/*
 * operation_log_persistent_init -- (internal) initialize operation log
 *	containing persistent memory resident changes
 */
static int
operation_log_persistent_init(struct operation_log *log,
	size_t redo_base_nbytes)
{
	log->capacity = REDO_LOG_BASE_SIZE;
	log->offset = 0;

	struct redo_log *src = Zalloc(sizeof(struct redo_log) +
		REDO_LOG_BASE_SIZE);
	if (src == NULL)
		return -1;

	/* initialize underlying redo log structure */
	src->capacity = redo_base_nbytes;
	memset(src->unused, 0, sizeof(src->unused));

	log->redo = src;

	return 0;
}

/*
 * operation_transient_clean -- cleans pmemcheck address state
 */
static int
operation_transient_clean(void *base, const void *addr, size_t len,
	unsigned flags)
{
	VALGRIND_SET_CLEAN(addr, len);

	return 0;
}

/*
 * operation_transient_memcpy -- transient memcpy wrapper
 */
static void *
operation_transient_memcpy(void *base, void *dest, const void *src, size_t len,
	unsigned flags)
{
	return memcpy(dest, src, len);
}

/*
 * operation_new -- creates new operation context
 */
struct operation_context *
operation_new(struct redo_log *redo, size_t redo_base_nbytes,
	redo_extend_fn extend, const struct pmem_ops *p_ops)
{
	struct operation_context *ctx = Zalloc(sizeof(*ctx));
	if (ctx == NULL)
		goto error_ctx_alloc;

	ctx->redo = redo;
	ctx->redo_base_nbytes = redo_base_nbytes;
	ctx->redo_capacity = redo_log_capacity(redo,
		redo_base_nbytes, p_ops);
	ctx->extend = extend;
	ctx->in_progress = 0;
	VEC_INIT(&ctx->next);
	redo_log_rebuild_next_vec(redo, &ctx->next, p_ops);
	ctx->p_ops = p_ops;

	ctx->t_ops.base = p_ops->base;
	ctx->t_ops.flush = operation_transient_clean;
	ctx->t_ops.memcpy = operation_transient_memcpy;

	VECQ_INIT(&ctx->merge_entries);

	if (operation_log_transient_init(&ctx->transient_ops) != 0)
		goto error_redo_alloc;

	if (operation_log_persistent_init(&ctx->pshadow_ops,
	    redo_base_nbytes) != 0)
		goto error_redo_alloc;

	return ctx;

error_redo_alloc:
	operation_delete(ctx);
error_ctx_alloc:
	return NULL;
}

/*
 * operation_delete -- deletes operation context
 */
void
operation_delete(struct operation_context *ctx)
{
	VECQ_DELETE(&ctx->merge_entries);
	VEC_DELETE(&ctx->next);
	Free(ctx->pshadow_ops.redo);
	Free(ctx->transient_ops.redo);
	Free(ctx);
}

/*
 * operation_merge -- (internal) performs operation on a field
 */
static inline void
operation_merge(struct redo_log_entry_base *entry, uint64_t value,
	enum redo_operation_type type)
{
	struct redo_log_entry_val *e = (struct redo_log_entry_val *)entry;

	switch (type) {
		case REDO_OPERATION_AND:
			e->value &= value;
			break;
		case REDO_OPERATION_OR:
			e->value |= value;
			break;
		case REDO_OPERATION_SET:
			e->value = value;
			break;
		default:
			ASSERT(0); /* unreachable */
	}
}

/*
 * operation_try_merge_entry -- tries to merge the incoming log entry with
 *	existing entries
 *
 * Because this requires a reverse foreach, it cannot be implemented using
 * the on-media redo log structure since there's no way to find what's
 * the previous entry in the log. Instead, the last N entries are stored
 * in a collection and traversed backwards.
 */
static int
operation_try_merge_entry(struct operation_context *ctx,
	void *ptr, uint64_t value, enum redo_operation_type type)
{
	int ret = 0;
	uint64_t offset = OBJ_PTR_TO_OFF(ctx->p_ops->base, ptr);

	struct redo_log_entry_val *e;
	VECQ_FOREACH_REVERSE(e, &ctx->merge_entries) {
		if (redo_log_entry_offset(&e->base) == offset) {
			if (redo_log_entry_type(&e->base) == type) {
				operation_merge(&e->base, value, type);
				return 1;
			} else {
				break;
			}
		}
	}

	return ret;
}

/*
 * operation_merge_entry_add -- adds a new entry to the merge collection,
 *	keeps capacity at OP_MERGE_SEARCH. Removes old entries in FIFO fashion.
 */
static void
operation_merge_entry_add(struct operation_context *ctx,
	struct redo_log_entry_val *entry)
{
	if (VECQ_SIZE(&ctx->merge_entries) == OP_MERGE_SEARCH)
		(void) VECQ_DEQUEUE(&ctx->merge_entries);

	if (VECQ_ENQUEUE(&ctx->merge_entries, entry) != 0) {
		/* this is fine, only runtime perf will get slower */
		LOG(2, "out of memory - unable to track entries");
	}
}

/*
 * operation_add_typed_value -- adds new entry to the current operation, if the
 *	same ptr address already exists and the operation type is set,
 *	the new value is not added and the function has no effect.
 */
int
operation_add_typed_entry(struct operation_context *ctx,
	void *ptr, uint64_t value,
	enum redo_operation_type type, enum operation_log_type log_type)
{
	ASSERTeq(type & REDO_VAL_OPERATIONS, type);

	struct operation_log *oplog = log_type == LOG_PERSISTENT ?
		&ctx->pshadow_ops : &ctx->transient_ops;

	/*
	 * Always make sure to have one extra spare cacheline so that the
	 * redo log entry creation has enough room for zeroing.
	 */
	if (oplog->offset + CACHELINE_SIZE == oplog->capacity) {
		size_t ncapacity = oplog->capacity + REDO_LOG_BASE_SIZE;
		struct redo_log *redo = Realloc(oplog->redo,
			SIZEOF_REDO_LOG(ncapacity));
		if (redo == NULL)
			return -1;
		oplog->capacity += REDO_LOG_BASE_SIZE;
		oplog->redo = redo;
	}

	if (operation_try_merge_entry(ctx, ptr, value, type) != 0)
		return 0;

	struct redo_log_entry_val *entry = redo_log_entry_val_create(
		oplog->redo, oplog->offset, ptr, value, type, &ctx->t_ops);

	operation_merge_entry_add(ctx, entry);

	oplog->offset += redo_log_entry_size(&entry->base);

	return 0;
}

/*
 * operation_add_value -- adds new entry to the current operation with
 *	entry type autodetected based on the memory location
 */
int
operation_add_entry(struct operation_context *ctx, void *ptr, uint64_t value,
	enum redo_operation_type type)
{
	const struct pmem_ops *p_ops = ctx->p_ops;
	PMEMobjpool *pop = (PMEMobjpool *)p_ops->base;

	int from_pool = OBJ_OFF_IS_VALID(pop,
		(uintptr_t)ptr - (uintptr_t)p_ops->base);

	return operation_add_typed_entry(ctx, ptr, value, type,
		from_pool ? LOG_PERSISTENT : LOG_TRANSIENT);
}

/*
 * operation_process_persistent_redo -- (internal) process using redo
 */
static void
operation_process_persistent_redo(struct operation_context *ctx)
{
	ASSERTeq(ctx->pshadow_ops.capacity % CACHELINE_SIZE, 0);

	redo_log_store(ctx->redo, ctx->pshadow_ops.redo,
		ctx->pshadow_ops.offset, ctx->redo_base_nbytes, &ctx->next,
		ctx->p_ops);

	redo_log_process(ctx->pshadow_ops.redo,
		OBJ_OFF_IS_VALID_FROM_CTX, ctx->p_ops);

	redo_log_clobber(ctx->redo, &ctx->next, ctx->p_ops);
}

/*
 * operation_reserve -- (internal) reserves new capacity in persistent redo log
 */
int
operation_reserve(struct operation_context *ctx, size_t new_capacity)
{
	if (new_capacity > ctx->redo_capacity) {
		if (ctx->extend == NULL) {
			ERR("no extend function present");
			return -1;
		}

		if (redo_log_reserve(ctx->redo,
		    ctx->redo_base_nbytes, &new_capacity, ctx->extend,
		    &ctx->next, ctx->p_ops) != 0)
			return -1;
		ctx->redo_capacity = new_capacity;
	}

	return 0;
}

/*
 * operation_init -- initializes runtime state of an operation
 */
void
operation_init(struct operation_context *ctx)
{
	struct operation_log *plog = &ctx->pshadow_ops;
	struct operation_log *tlog = &ctx->transient_ops;

	VALGRIND_ANNOTATE_NEW_MEMORY(ctx, sizeof(*ctx));
	VALGRIND_ANNOTATE_NEW_MEMORY(tlog->redo, sizeof(struct redo_log) +
		tlog->capacity);
	VALGRIND_ANNOTATE_NEW_MEMORY(plog->redo, sizeof(struct redo_log) +
		plog->capacity);
	tlog->offset = 0;
	plog->offset = 0;
	VECQ_REINIT(&ctx->merge_entries);
}

/*
 * operation_start -- initializes and starts a new operation
 */
void
operation_start(struct operation_context *ctx)
{
	operation_init(ctx);
	ASSERTeq(ctx->in_progress, 0);
	ctx->in_progress = 1;
}

/*
 * operation_cancel -- cancels a running operation
 */
void
operation_cancel(struct operation_context *ctx)
{
	ASSERTeq(ctx->in_progress, 1);
	ctx->in_progress = 0;
}

/*
 * operation_process -- processes registered operations
 *
 * The order of processing is important: persistent, transient.
 * This is because the transient entries that reside on persistent memory might
 * require write to a location that is currently occupied by a valid persistent
 * state but becomes a transient state after operation is processed.
 */
void
operation_process(struct operation_context *ctx)
{
	/*
	 * If there's exactly one persistent entry there's no need to involve
	 * the redo log. We can simply assign the value, the operation will be
	 * atomic.
	 */
	int pmem_processed = 0;
	if (ctx->pshadow_ops.offset == sizeof(struct redo_log_entry_val)) {
		struct redo_log_entry_base *e = (struct redo_log_entry_base *)
			ctx->pshadow_ops.redo->data;
		enum redo_operation_type t = redo_log_entry_type(e);
		if ((t & REDO_VAL_OPERATIONS) == t) {
			redo_log_entry_apply(e, 1, ctx->p_ops);
			pmem_processed = 1;
		}
	}

	if (!pmem_processed && ctx->pshadow_ops.offset != 0)
		operation_process_persistent_redo(ctx);

	/* process transient entries with transient memory ops */
	if (ctx->transient_ops.offset != 0)
		redo_log_process(ctx->transient_ops.redo,
			OBJ_OFF_IS_VALID_FROM_CTX, &ctx->t_ops);

	ASSERTeq(ctx->in_progress, 1);
	ctx->in_progress = 0;
}
