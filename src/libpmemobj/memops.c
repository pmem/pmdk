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

#define REDO_LOG_BASE_ENTRIES 128
#define OP_MERGE_SEARCH 64

struct operation_log {
	size_t capacity; /* capacity of the redo log */
	size_t size; /* number of entries currently in the redo log */
	struct redo_log *redo; /* DRAM allocated log of modifications */
};

/*
 * operation_context -- context of an ongoing palloc operation
 */
struct operation_context {
	void *base; /* pool address */
	redo_extend_fn extend; /* function to allocate next redo logs */

	const struct redo_ctx *redo_ctx;
	const struct pmem_ops *p_ops;

	struct redo_log *redo; /* pointer to the persistent redo log */
	size_t redo_base_capacity; /* number of entries in initial redo log */
	size_t redo_capacity; /* sum of capacity, incl all next redo logs */

	struct redo_next next; /* vector of 'next' fields of persistent redo */

	int in_progress; /* operation sanity check */

	struct operation_log pshadow_ops; /* shadow copy of persistent redo */
	struct operation_log transient_ops; /* log of transient changes */
};

/*
 * operation_log_transient_init -- (internal) initialize operation log
 *	containing transient memory resident changes
 */
static int
operation_log_transient_init(struct operation_log *log)
{
	log->capacity = REDO_LOG_BASE_ENTRIES;
	log->size = 0;

	struct redo_log *src = Zalloc(sizeof(struct redo_log) +
	(sizeof(struct redo_log_entry) * REDO_LOG_BASE_ENTRIES));
	if (src == NULL)
		return -1;

	/* initialize underlying redo log structure */
	src->capacity = REDO_LOG_BASE_ENTRIES;

	log->redo = src;

	return 0;
}

/*
 * operation_log_persistent_init -- (internal) initialize operation log
 *	containing persistent memory resident changes
 */
static int
operation_log_persistent_init(struct operation_log *log,
	size_t redo_base_capacity)
{
	log->capacity = REDO_LOG_BASE_ENTRIES;
	log->size = 0;

	struct redo_log *src = Zalloc(sizeof(struct redo_log) +
	(sizeof(struct redo_log_entry) * REDO_LOG_BASE_ENTRIES));
	if (src == NULL)
		return -1;

	/* initialize underlying redo log structure */
	src->capacity = redo_base_capacity;
	memset(src->unused, 0, sizeof(src->unused));

	log->redo = src;

	return 0;
}

/*
 * operation_new -- creates new operation context
 */
struct operation_context *
operation_new(void *base, const struct redo_ctx *redo_ctx,
	struct redo_log *redo, size_t redo_base_capacity, redo_extend_fn extend)
{
	struct operation_context *ctx = Zalloc(sizeof(*ctx));
	if (ctx == NULL)
		goto error_ctx_alloc;

	ctx->base = base;
	ctx->redo_ctx = redo_ctx;
	ctx->redo = redo;
	ctx->redo_base_capacity = redo_base_capacity;
	ctx->redo_capacity = redo_log_capacity(redo_ctx, redo,
		redo_base_capacity);
	ctx->extend = extend;
	ctx->in_progress = 0;
	VEC_INIT(&ctx->next);
	redo_log_rebuild_next_vec(redo_ctx, redo, &ctx->next);

	if (redo_ctx)
		ctx->p_ops = redo_get_pmem_ops(redo_ctx);
	else
		ctx->p_ops = NULL;

	if (operation_log_transient_init(&ctx->transient_ops) != 0)
		goto error_redo_alloc;

	if (operation_log_persistent_init(&ctx->pshadow_ops,
	    redo_base_capacity) != 0)
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
	VEC_DELETE(&ctx->next);
	Free(ctx->pshadow_ops.redo);
	Free(ctx->transient_ops.redo);
	Free(ctx);
}

/*
 * operation_apply -- (internal) performs operation on a field
 */
static inline void
operation_apply(struct redo_log_entry *oentry, struct redo_log_entry *nentry,
	enum redo_operation_type op_type)
{
	switch (op_type) {
		case REDO_OPERATION_AND:
			oentry->value &= nentry->value;
		break;
		case REDO_OPERATION_OR:
			oentry->value |= nentry->value;
		break;
		case REDO_OPERATION_SET: /* do nothing, duplicate entry */
		break;
		default:
			ASSERT(0); /* unreachable */
	}
}

/*
 * operation_add_typed_entry -- adds new entry to the current operation, if the
 *	same ptr address already exists and the operation type is set,
 *	the new value is not added and the function has no effect.
 */
int
operation_add_typed_entry(struct operation_context *ctx,
	void *ptr, uint64_t value,
	enum redo_operation_type type, enum operation_log_type log_type)
{
	/*
	 * New entry to be added to the operations, all operations eventually
	 * come down to a set operation regardless.
	 */
	struct redo_log_entry entry;
	redo_log_entry_create(ctx->base, &entry, ptr, value, type);

	struct operation_log *oplog = log_type == LOG_PERSISTENT ?
		&ctx->pshadow_ops : &ctx->transient_ops;

	struct redo_log_entry *e; /* existing entry */

	/* search last few entries to see if we could merge ops */
	for (size_t i = 1; i <= OP_MERGE_SEARCH && oplog->size >= i; ++i) {
		e = &oplog->redo->entries[oplog->size - i];
		enum redo_operation_type e_type = redo_log_operation(e);
		if (redo_log_offset(e) == redo_log_offset(&entry)) {
			if (e_type == type) {
				operation_apply(e, &entry, type);
				return 0;
			} else {
				break;
			}
		}
	}

	if (oplog->size == oplog->capacity) {
		size_t ncapacity = oplog->capacity + REDO_LOG_BASE_ENTRIES;
		struct redo_log *redo = Realloc(oplog->redo,
			SIZEOF_REDO_LOG(ncapacity));
		if (redo == NULL)
			return -1;
		oplog->capacity += REDO_LOG_BASE_ENTRIES;
		oplog->redo = redo;
	}

	size_t pos = oplog->size++;
	oplog->redo->entries[pos] = entry;

	return 0;
}

/*
 * operation_add_entry -- adds new entry to the current operation with
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
	const struct redo_ctx *redo = ctx->redo_ctx;

	ASSERTeq((ctx->pshadow_ops.capacity *
		sizeof(*ctx->pshadow_ops.redo->entries)) % CACHELINE_SIZE, 0);

	redo_log_store(ctx->redo_ctx, ctx->redo,
		ctx->pshadow_ops.redo, ctx->pshadow_ops.size,
		ctx->redo_base_capacity, &ctx->next);

	redo_log_process(redo, ctx->pshadow_ops.redo);

	redo_log_clobber(ctx->redo_ctx, ctx->redo, &ctx->next);
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

		if (redo_log_reserve(ctx->redo_ctx, ctx->redo,
		    ctx->redo_base_capacity, &new_capacity, ctx->extend,
		    &ctx->next) != 0)
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
		(sizeof(struct redo_log_entry) * tlog->capacity));
	VALGRIND_ANNOTATE_NEW_MEMORY(plog->redo, sizeof(struct redo_log) +
		(sizeof(struct redo_log_entry) * plog->capacity));
	tlog->size = 0;
	plog->size = 0;
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
	struct redo_log_entry *e;

	struct operation_log *plog = &ctx->pshadow_ops;
	struct operation_log *tlog = &ctx->transient_ops;

	/*
	 * If there's exactly one persistent entry there's no need to involve
	 * the redo log. We can simply assign the value, the operation will be
	 * atomic.
	 */
	if (plog->size == 1) {
		e = &plog->redo->entries[0];
		redo_log_entry_apply(ctx->base, e, ctx->p_ops->persist);
	} else if (plog->size != 0) {
		operation_process_persistent_redo(ctx);
	}

	for (size_t i = 0; i < tlog->size; ++i) {
		e = &tlog->redo->entries[i];
		redo_log_entry_apply(ctx->base, e, operation_transient_clean);
	}

	ASSERTeq(ctx->in_progress, 1);
	ctx->in_progress = 0;
}
