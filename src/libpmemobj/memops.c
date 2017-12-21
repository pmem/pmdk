/*
 * Copyright 2016-2017, Intel Corporation
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

/*
 * operation_init -- initializes a new palloc operation
 */
void
operation_init(struct operation_context *ctx, const void *base,
	const struct redo_ctx *redo_ctx, struct redo_log *redo)
{
	ctx->base = base;
	ctx->redo_ctx = redo_ctx;
	ctx->redo = redo;
	if (redo_ctx)
		ctx->p_ops = redo_get_pmem_ops(redo_ctx);
	else
		ctx->p_ops = NULL;

	ctx->nentries[ENTRY_PERSISTENT] = 0;
	ctx->nentries[ENTRY_TRANSIENT] = 0;
}

/*
 * operation_perform -- (internal) performs a operation on the field
 */
static inline void
operation_perform(uint64_t *field, uint64_t value,
	enum operation_type op_type)
{
	switch (op_type) {
		case OPERATION_AND:
			*field &= value;
		break;
		case OPERATION_OR:
			*field |= value;
		break;
		case OPERATION_SET: /* do nothing, duplicate entry */
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
void
operation_add_typed_entry(struct operation_context *ctx,
	void *ptr, uint64_t value,
	enum operation_type type, enum operation_entry_type en_type)
{
	ASSERT(ctx->nentries[ENTRY_PERSISTENT] < MAX_MEMOPS_ENTRIES);
	ASSERT(ctx->nentries[ENTRY_TRANSIENT] < MAX_MEMOPS_ENTRIES);

	/*
	 * New entry to be added to the operations, all operations eventually
	 * come down to a set operation regardless.
	 */
	struct operation_entry en = {ptr, value, OPERATION_SET};

	struct operation_entry *e; /* existing entry */
	for (size_t i = 0; i < ctx->nentries[en_type]; ++i) {
		e = &ctx->entries[en_type][i];
		/* update existing and exit, no reason to add new op */
		if (e->ptr == ptr) {
			operation_perform(&e->value, value, type);

			return;
		}
	}

	if (type == OPERATION_AND || type == OPERATION_OR) {
		/* change the new entry to current value and apply logic op */
		en.value = *(uint64_t *)ptr;
		operation_perform(&en.value, value, type);
	}

	ctx->entries[en_type][ctx->nentries[en_type]] = en;

	ctx->nentries[en_type]++;
}

/*
 * operation_add_entry -- adds new entry to the current operation with
 *	entry type autodetected based on the memory location
 */
void
operation_add_entry(struct operation_context *ctx, void *ptr, uint64_t value,
	enum operation_type type)
{
	const struct pmem_ops *p_ops = ctx->p_ops;
	PMEMobjpool *pop = (PMEMobjpool *)p_ops->base;

	int from_pool = OBJ_OFF_IS_VALID(pop,
		(uintptr_t)ptr - (uintptr_t)p_ops->base);

	operation_add_typed_entry(ctx, ptr, value, type,
		from_pool ? ENTRY_PERSISTENT : ENTRY_TRANSIENT);
}

/*
 * operation_process_persistent_redo -- (internal) process using redo
 */
static void
operation_process_persistent_redo(struct operation_context *ctx)
{
	struct operation_entry *e;
	const struct redo_ctx *redo = ctx->redo_ctx;

	size_t i;
	for (i = 0; i < ctx->nentries[ENTRY_PERSISTENT]; ++i) {
		e = &ctx->entries[ENTRY_PERSISTENT][i];

		redo_log_store(redo, ctx->redo, i,
				(uintptr_t)e->ptr - (uintptr_t)ctx->base,
				e->value);
	}

	redo_log_set_last(redo, ctx->redo, i - 1);
	redo_log_process(redo, ctx->redo, i);
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
	struct operation_entry *e;

	/*
	 * If there's exactly one persistent entry there's no need to involve
	 * the redo log. We can simply assign the value, the operation will be
	 * atomic.
	 */
	if (ctx->nentries[ENTRY_PERSISTENT] == 1) {
		e = &ctx->entries[ENTRY_PERSISTENT][0];

		VALGRIND_ADD_TO_TX(e->ptr, sizeof(uint64_t));

		*e->ptr = e->value;
		pmemops_persist(ctx->p_ops, e->ptr,
				sizeof(uint64_t));

		VALGRIND_REMOVE_FROM_TX(e->ptr, sizeof(uint64_t));
	} else if (ctx->nentries[ENTRY_PERSISTENT] != 0) {
		operation_process_persistent_redo(ctx);
	}

	for (size_t i = 0; i < ctx->nentries[ENTRY_TRANSIENT]; ++i) {
		e = &ctx->entries[ENTRY_TRANSIENT][i];
		*e->ptr = e->value;
		/*
		 * Just in case that the entry was transient but in reality
		 * the variable is on persistent memory. This is true for
		 * chunk footers.
		 */
		VALGRIND_SET_CLEAN(e->ptr, sizeof(e->value));
	}
}
