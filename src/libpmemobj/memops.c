/*
 * Copyright 2016, Intel Corporation
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
 */

#include "libpmemobj.h"
#include "util.h"
#include "out.h"
#include "redo.h"
#include "memops.h"
#include "lane.h"
#include "pmalloc.h"
#include "list.h"
#include "obj.h"
#include "valgrind_internal.h"
#include "heap_layout.h"

enum operation_entry_type {
	ENTRY_PERSISTENT,
	ENTRY_TRANSIENT,

	MAX_OPERATION_ENTRY_TYPE
};

#define MAX_TRANSIENT_ENTRIES 10
#define MAX_PERSITENT_ENTRIES 10

/*
 * operation_context -- context of an ongoing palloc operation
 */
struct operation_context {
	PMEMobjpool *pop;
	struct redo_log *redo;

	size_t nentries[MAX_OPERATION_ENTRY_TYPE];
	struct operation_entry
		entries[MAX_OPERATION_ENTRY_TYPE][MAX_PERSITENT_ENTRIES];
};

/*
 * operation_init -- initializes a new palloc operation
 */
struct operation_context *
operation_init(PMEMobjpool *pop, struct redo_log *redo)
{
	struct operation_context *ctx = Malloc(sizeof(*ctx));

	if (ctx == NULL)
		goto out;

	ctx->pop = pop;
	ctx->redo = redo;
	ctx->nentries[ENTRY_PERSISTENT] = 0;
	ctx->nentries[ENTRY_TRANSIENT] = 0;

out:
	return ctx;
}

/*
 * operation_perform -- (internal) performs a logic operation on the field
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
		default:
			ASSERT(0); /* unreachable */
	}
}

/*
 * operation_add_entry -- adds new entry to the current operation
 */
void
operation_add_entry(struct operation_context *ctx, void *ptr, uint64_t value,
	enum operation_type type)
{
	ASSERT(ctx->nentries[ENTRY_PERSISTENT] <= MAX_PERSITENT_ENTRIES);
	ASSERT(ctx->nentries[ENTRY_TRANSIENT] <= MAX_TRANSIENT_ENTRIES);

	enum operation_entry_type entry_type =
		OBJ_PTR_IS_VALID(ctx->pop, ptr) ?
		ENTRY_PERSISTENT : ENTRY_TRANSIENT;

	/* new entry to be added to the operations */
	struct operation_entry en = {ptr, value, OPERATION_SET};

	if (type == OPERATION_AND || type == OPERATION_OR) {
		struct operation_entry *e; /* existing entry */
		for (size_t i = 0; i < ctx->nentries[entry_type]; ++i) {
			e = &ctx->entries[entry_type][i];
			/* update existing and exit, no reason to add new op */
			if (e->ptr == ptr) {
				operation_perform(&e->value, value, type);

				return;
			}
		}

		/* change the new entry to current value and apply logic op */
		en.value = *(uint64_t *)ptr;
		operation_perform(&en.value, value, type);

	}

	ctx->entries[entry_type][ctx->nentries[entry_type]] = en;

	ctx->nentries[entry_type]++;
}

/*
 * operation_add_entries -- adds new entries to the current operation
 */
void
operation_add_entries(struct operation_context *ctx,
	struct operation_entry *entries, size_t nentries)
{
	for (size_t i = 0; i < nentries; ++i) {
		operation_add_entry(ctx, entries[i].ptr,
			entries[i].value, entries[i].type);
	}
}

/*
 * operation_process_persistent_redo -- (internal) process using redo
 */
static void
operation_process_persistent_redo(struct operation_context *ctx)
{
	struct operation_entry *e;

	size_t i;
	for (i = 0; i < ctx->nentries[ENTRY_PERSISTENT]; ++i) {
		e = &ctx->entries[ENTRY_PERSISTENT][i];

		redo_log_store(ctx->pop, ctx->redo, i,
			OBJ_PTR_TO_OFF(ctx->pop, e->ptr), e->value);
	}

	redo_log_set_last(ctx->pop, ctx->redo, i - 1);
	redo_log_process(ctx->pop, ctx->redo, i);
}

/*
 * operation_process -- processes registered operations
 */
void
operation_process(struct operation_context *ctx)
{
	struct operation_entry *e;

	for (size_t i = 0; i < ctx->nentries[ENTRY_TRANSIENT]; ++i) {
		e = &ctx->entries[ENTRY_TRANSIENT][i];
		*e->ptr = e->value;
	}

	/*
	 * If there's exactly one persistent entry there's no need to involve
	 * the redo log. We can simply assign the value, the operation will be
	 * atomic.
	 */
	if (ctx->nentries[ENTRY_PERSISTENT] == 1) {
		e = &ctx->entries[ENTRY_PERSISTENT][0];

		VALGRIND_ADD_TO_TX(e->ptr, sizeof(uint64_t));

		*e->ptr = e->value;
		pmemobj_persist(ctx->pop, e->ptr, sizeof(uint64_t));

		VALGRIND_REMOVE_FROM_TX(e->ptr, sizeof(uint64_t));
	} else if (ctx->nentries[ENTRY_PERSISTENT] != 0) {
		operation_process_persistent_redo(ctx);
	}
}

/*
 * operation_delete -- deletes current operation context
 */
void
operation_delete(struct operation_context *ctx)
{
	Free(ctx);
}
