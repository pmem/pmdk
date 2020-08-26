/*
 * Copyright 2016-2020, Intel Corporation
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
#include "ravl.h"
#include "valgrind_internal.h"
#include "vecq.h"
#include "sys_util.h"

#define ULOG_BASE_SIZE 1024
#define OP_MERGE_SEARCH 64

enum operation_state {
	OPERATION_IDLE,
	OPERATION_IN_PROGRESS,
	OPERATION_CLEANUP,
};

struct operation_log {
	size_t capacity; /* capacity of the ulog log */
	size_t offset; /* data offset inside of the log */
	struct ulog *ulog; /* DRAM allocated log of modifications */
};

/*
 * operation_context -- context of an ongoing palloc operation
 */
struct operation_context {
	enum log_type type;

	ulog_extend_fn extend; /* function to allocate next ulog */
	ulog_free_fn ulog_free; /* function to free next ulogs */

	const struct pmem_ops *p_ops;
	struct pmem_ops t_ops; /* used for transient data processing */
	struct pmem_ops s_ops; /* used for shadow copy data processing */

	size_t ulog_curr_offset; /* offset in the log for buffer stores */
	size_t ulog_curr_capacity; /* capacity of the current log */
	size_t ulog_curr_gen_num; /* transaction counter in the current log */
	struct ulog *ulog_curr; /* current persistent log */
	size_t total_logged; /* total amount of buffer stores in the logs */

	struct ulog *ulog; /* pointer to the persistent ulog log */
	size_t ulog_base_nbytes; /* available bytes in initial ulog log */
	size_t ulog_capacity; /* sum of capacity, incl all next ulog logs */
	int ulog_auto_reserve; /* allow or do not to auto ulog reservation */
	int ulog_any_user_buffer; /* set if any user buffer is added */

	struct ulog_next next; /* vector of 'next' fields of persistent ulog */

	enum operation_state state; /* operation sanity check */

	struct operation_log pshadow_ops; /* shadow copy of persistent ulog */
	struct operation_log transient_ops; /* log of transient changes */

	/* collection used to look for potential merge candidates */
	VECQ(, struct ulog_entry_val *) merge_entries;
};

/*
 * operation_log_transient_init -- (internal) initialize operation log
 *	containing transient memory resident changes
 */
static int
operation_log_transient_init(struct operation_log *log)
{
	log->capacity = ULOG_BASE_SIZE;
	log->offset = 0;

	struct ulog *src = Zalloc(sizeof(struct ulog) +
		ULOG_BASE_SIZE);
	if (src == NULL) {
		ERR("!Zalloc");
		return -1;
	}

	/* initialize underlying redo log structure */
	src->capacity = ULOG_BASE_SIZE;

	log->ulog = src;

	return 0;
}

/*
 * operation_log_persistent_init -- (internal) initialize operation log
 *	containing persistent memory resident changes
 */
static int
operation_log_persistent_init(struct operation_log *log,
	size_t ulog_base_nbytes)
{
	log->capacity = ULOG_BASE_SIZE;
	log->offset = 0;

	struct ulog *src = Zalloc(sizeof(struct ulog) +
		ULOG_BASE_SIZE);
	if (src == NULL) {
		ERR("!Zalloc");
		return -1;
	}

	/* initialize underlying redo log structure */
	src->capacity = ulog_base_nbytes;
	memset(src->unused, 0, sizeof(src->unused));

	log->ulog = src;

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
 * operation_transient_drain -- noop
 */
static void
operation_transient_drain(void *base)
{
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
operation_new(struct ulog *ulog, size_t ulog_base_nbytes,
	ulog_extend_fn extend, ulog_free_fn ulog_free,
	const struct pmem_ops *p_ops, enum log_type type)
{
	struct operation_context *ctx = Zalloc(sizeof(*ctx));
	if (ctx == NULL) {
		ERR("!Zalloc");
		goto error_ctx_alloc;
	}

	ctx->ulog = ulog;
	ctx->ulog_base_nbytes = ulog_base_nbytes;
	ctx->ulog_capacity = ulog_capacity(ulog,
		ulog_base_nbytes, p_ops);
	ctx->extend = extend;
	ctx->ulog_free = ulog_free;
	ctx->state = OPERATION_IDLE;
	VEC_INIT(&ctx->next);
	ulog_rebuild_next_vec(ulog, &ctx->next, p_ops);
	ctx->p_ops = p_ops;
	ctx->type = type;
	ctx->ulog_any_user_buffer = 0;

	ctx->ulog_curr_offset = 0;
	ctx->ulog_curr_capacity = 0;
	ctx->ulog_curr = NULL;

	ctx->t_ops.base = NULL;
	ctx->t_ops.flush = operation_transient_clean;
	ctx->t_ops.memcpy = operation_transient_memcpy;
	ctx->t_ops.drain = operation_transient_drain;

	ctx->s_ops.base = p_ops->base;
	ctx->s_ops.flush = operation_transient_clean;
	ctx->s_ops.memcpy = operation_transient_memcpy;
	ctx->s_ops.drain = operation_transient_drain;

	VECQ_INIT(&ctx->merge_entries);

	if (operation_log_transient_init(&ctx->transient_ops) != 0)
		goto error_ulog_alloc;

	if (operation_log_persistent_init(&ctx->pshadow_ops,
	    ulog_base_nbytes) != 0)
		goto error_ulog_alloc;

	return ctx;

error_ulog_alloc:
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
	Free(ctx->pshadow_ops.ulog);
	Free(ctx->transient_ops.ulog);
	Free(ctx);
}

/*
 * operation_user_buffer_remove -- removes range from the tree and returns 0
 */
static int
operation_user_buffer_remove(void *base, void *addr)
{
	PMEMobjpool *pop = base;
	if (!pop->ulog_user_buffers.verify)
		return 0;

	util_mutex_lock(&pop->ulog_user_buffers.lock);

	struct ravl *ravl = pop->ulog_user_buffers.map;
	enum ravl_predicate predict = RAVL_PREDICATE_EQUAL;

	struct user_buffer_def range;
	range.addr = addr;
	range.size = 0;

	struct ravl_node *n = ravl_find(ravl, &range, predict);
	ASSERTne(n, NULL);
	ravl_remove(ravl, n);

	util_mutex_unlock(&pop->ulog_user_buffers.lock);

	return 0;
}

/*
 * operation_free_logs -- free all logs except first
 */
void
operation_free_logs(struct operation_context *ctx, uint64_t flags)
{
	int freed = ulog_free_next(ctx->ulog, ctx->p_ops, ctx->ulog_free,
			operation_user_buffer_remove, flags);
	if (freed) {
		ctx->ulog_capacity = ulog_capacity(ctx->ulog,
			ctx->ulog_base_nbytes, ctx->p_ops);
		VEC_CLEAR(&ctx->next);
		ulog_rebuild_next_vec(ctx->ulog, &ctx->next, ctx->p_ops);
	}

	ASSERTeq(VEC_SIZE(&ctx->next), 0);
}

/*
 * operation_merge -- (internal) performs operation on a field
 */
static inline void
operation_merge(struct ulog_entry_base *entry, uint64_t value,
	ulog_operation_type type)
{
	struct ulog_entry_val *e = (struct ulog_entry_val *)entry;

	switch (type) {
		case ULOG_OPERATION_AND:
			e->value &= value;
			break;
		case ULOG_OPERATION_OR:
			e->value |= value;
			break;
		case ULOG_OPERATION_SET:
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
 * the on-media ulog log structure since there's no way to find what's
 * the previous entry in the log. Instead, the last N entries are stored
 * in a collection and traversed backwards.
 */
static int
operation_try_merge_entry(struct operation_context *ctx,
	void *ptr, uint64_t value, ulog_operation_type type)
{
	int ret = 0;
	uint64_t offset = OBJ_PTR_TO_OFF(ctx->p_ops->base, ptr);

	struct ulog_entry_val *e;
	VECQ_FOREACH_REVERSE(e, &ctx->merge_entries) {
		if (ulog_entry_offset(&e->base) == offset) {
			if (ulog_entry_type(&e->base) == type) {
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
	struct ulog_entry_val *entry)
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
	ulog_operation_type type, enum operation_log_type log_type)
{
	struct operation_log *oplog = log_type == LOG_PERSISTENT ?
		&ctx->pshadow_ops : &ctx->transient_ops;

	/*
	 * Always make sure to have one extra spare cacheline so that the
	 * ulog log entry creation has enough room for zeroing.
	 */
	if (oplog->offset + CACHELINE_SIZE == oplog->capacity) {
		size_t ncapacity = oplog->capacity + ULOG_BASE_SIZE;
		struct ulog *ulog = Realloc(oplog->ulog,
			SIZEOF_ULOG(ncapacity));
		if (ulog == NULL)
			return -1;
		oplog->capacity += ULOG_BASE_SIZE;
		oplog->ulog = ulog;
		oplog->ulog->capacity = oplog->capacity;

		/*
		 * Realloc invalidated the ulog entries that are inside of this
		 * vector, need to clear it to avoid use after free.
		 */
		VECQ_CLEAR(&ctx->merge_entries);
	}

	if (log_type == LOG_PERSISTENT &&
		operation_try_merge_entry(ctx, ptr, value, type) != 0)
		return 0;

	struct ulog_entry_val *entry = ulog_entry_val_create(
		oplog->ulog, oplog->offset, ptr, value, type,
		log_type == LOG_TRANSIENT ? &ctx->t_ops : &ctx->s_ops);

	if (log_type == LOG_PERSISTENT)
		operation_merge_entry_add(ctx, entry);

	oplog->offset += ulog_entry_size(&entry->base);

	return 0;
}

/*
 * operation_add_value -- adds new entry to the current operation with
 *	entry type autodetected based on the memory location
 */
int
operation_add_entry(struct operation_context *ctx, void *ptr, uint64_t value,
	ulog_operation_type type)
{
	const struct pmem_ops *p_ops = ctx->p_ops;
	PMEMobjpool *pop = (PMEMobjpool *)p_ops->base;

	int from_pool = OBJ_OFF_IS_VALID(pop,
		(uintptr_t)ptr - (uintptr_t)p_ops->base);

	return operation_add_typed_entry(ctx, ptr, value, type,
		from_pool ? LOG_PERSISTENT : LOG_TRANSIENT);
}

/*
 * operation_add_buffer -- adds a buffer operation to the log
 */
int
operation_add_buffer(struct operation_context *ctx,
	void *dest, void *src, size_t size, ulog_operation_type type)
{
	size_t real_size = size + sizeof(struct ulog_entry_buf);

	/* if there's no space left in the log, reserve some more */
	if (ctx->ulog_curr_capacity == 0) {
		ctx->ulog_curr_gen_num = ctx->ulog->gen_num;
		if (operation_reserve(ctx, ctx->total_logged + real_size) != 0)
			return -1;

		ctx->ulog_curr = ctx->ulog_curr == NULL ? ctx->ulog :
			ulog_next(ctx->ulog_curr, ctx->p_ops);
		ASSERTne(ctx->ulog_curr, NULL);
		ctx->ulog_curr_offset = 0;
		ctx->ulog_curr_capacity = ctx->ulog_curr->capacity;
	}

	size_t curr_size = MIN(real_size, ctx->ulog_curr_capacity);
	size_t data_size = curr_size - sizeof(struct ulog_entry_buf);
	size_t entry_size = ALIGN_UP(curr_size, CACHELINE_SIZE);

	/*
	 * To make sure that the log is consistent and contiguous, we need
	 * make sure that the header of the entry that would be located
	 * immediately after this one is zeroed.
	 */
	struct ulog_entry_base *next_entry = NULL;
	if (entry_size == ctx->ulog_curr_capacity) {
		struct ulog *u = ulog_next(ctx->ulog_curr, ctx->p_ops);
		if (u != NULL)
			next_entry = (struct ulog_entry_base *)u->data;
	} else {
		size_t next_entry_offset = ctx->ulog_curr_offset + entry_size;
		next_entry = (struct ulog_entry_base *)(ctx->ulog_curr->data +
			next_entry_offset);
	}
	if (next_entry != NULL)
		ulog_clobber_entry(next_entry, ctx->p_ops);

	/* create a persistent log entry */
	struct ulog_entry_buf *e = ulog_entry_buf_create(ctx->ulog_curr,
		ctx->ulog_curr_offset,
		ctx->ulog_curr_gen_num,
		dest, src, data_size,
		type, ctx->p_ops);
	ASSERT(entry_size == ulog_entry_size(&e->base));
	ASSERT(entry_size <= ctx->ulog_curr_capacity);

	ctx->total_logged += entry_size;
	ctx->ulog_curr_offset += entry_size;
	ctx->ulog_curr_capacity -= entry_size;

	/*
	 * Recursively add the data to the log until the entire buffer is
	 * processed.
	 */
	return size - data_size == 0 ? 0 : operation_add_buffer(ctx,
			(char *)dest + data_size,
			(char *)src + data_size,
			size - data_size, type);
}

/*
 * operation_user_buffer_range_cmp -- compares addresses of
 * user buffers
 */
int
operation_user_buffer_range_cmp(const void *lhs, const void *rhs)
{
	const struct user_buffer_def *l = lhs;
	const struct user_buffer_def *r = rhs;

	if (l->addr > r->addr)
		return 1;
	else if (l->addr < r->addr)
		return -1;

	return 0;
}

/*
 * operation_user_buffer_try_insert -- adds a user buffer range to the tree,
 * if the buffer already exists in the tree function returns -1, otherwise
 * it returns 0
 */
static int
operation_user_buffer_try_insert(PMEMobjpool *pop,
	struct user_buffer_def *userbuf)
{
	int ret = 0;

	if (!pop->ulog_user_buffers.verify)
		return ret;

	util_mutex_lock(&pop->ulog_user_buffers.lock);

	void *addr_end = (char *)userbuf->addr + userbuf->size;
	struct user_buffer_def search;
	search.addr = addr_end;
	struct ravl_node *n = ravl_find(pop->ulog_user_buffers.map,
		&search, RAVL_PREDICATE_LESS_EQUAL);
	if (n != NULL) {
		struct user_buffer_def *r = ravl_data(n);
		void *r_end = (char *)r->addr + r->size;

		if (r_end > userbuf->addr && r->addr < addr_end) {
			/* what was found overlaps with what is being added */
			ret = -1;
			goto out;
		}
	}

	if (ravl_emplace_copy(pop->ulog_user_buffers.map, userbuf) == -1) {
		ASSERTne(errno, EEXIST);
		ret = -1;
	}

out:
	util_mutex_unlock(&pop->ulog_user_buffers.lock);
	return ret;
}

/*
 * operation_user_buffer_verify_align -- verify if the provided buffer can be
 *	used as a transaction log, and if so - perform necessary alignments
 */
int
operation_user_buffer_verify_align(struct operation_context *ctx,
		struct user_buffer_def *userbuf)
{
	/*
	 * Address of the buffer has to be aligned up, and the size
	 * has to be aligned down, taking into account the number of bytes
	 * the address was incremented by. The remaining size has to be large
	 * enough to contain the header and at least one ulog entry.
	 */
	uint64_t buffer_offset = OBJ_PTR_TO_OFF(ctx->p_ops->base,
		userbuf->addr);
	ptrdiff_t size_diff = (intptr_t)ulog_by_offset(buffer_offset,
		ctx->p_ops) - (intptr_t)userbuf->addr;
	ssize_t capacity_unaligned = (ssize_t)userbuf->size - size_diff
		- (ssize_t)sizeof(struct ulog);
	if (capacity_unaligned < (ssize_t)CACHELINE_SIZE) {
		ERR("Capacity insufficient");
		return -1;
	}

	size_t capacity_aligned = ALIGN_DOWN((size_t)capacity_unaligned,
		CACHELINE_SIZE);

	userbuf->addr = ulog_by_offset(buffer_offset, ctx->p_ops);
	userbuf->size = capacity_aligned + sizeof(struct ulog);

	if (operation_user_buffer_try_insert(ctx->p_ops->base, userbuf)) {
		ERR("Buffer currently used");
		return -1;
	}

	return 0;
}

/*
 * operation_add_user_buffer -- add user buffer to the ulog
 */
void
operation_add_user_buffer(struct operation_context *ctx,
		struct user_buffer_def *userbuf)
{
	uint64_t buffer_offset = OBJ_PTR_TO_OFF(ctx->p_ops->base,
		userbuf->addr);
	size_t capacity = userbuf->size - sizeof(struct ulog);

	ulog_construct(buffer_offset, capacity, ctx->ulog->gen_num,
			1, ULOG_USER_OWNED, ctx->p_ops);

	struct ulog *last_log;
	/* if there is only one log */
	if (!VEC_SIZE(&ctx->next))
		last_log = ctx->ulog;
	else /* get last element from vector */
		last_log = ulog_by_offset(VEC_BACK(&ctx->next), ctx->p_ops);

	ASSERTne(last_log, NULL);
	size_t next_size = sizeof(last_log->next);
	VALGRIND_ADD_TO_TX(&last_log->next, next_size);
	last_log->next = buffer_offset;
	pmemops_persist(ctx->p_ops, &last_log->next, next_size);

	VEC_PUSH_BACK(&ctx->next, buffer_offset);
	ctx->ulog_capacity += capacity;
	operation_set_any_user_buffer(ctx, 1);
}

/*
 * operation_set_auto_reserve -- set auto reserve value for context
 */
void
operation_set_auto_reserve(struct operation_context *ctx, int auto_reserve)
{
	ctx->ulog_auto_reserve = auto_reserve;
}

/*
 * operation_set_any_user_buffer -- set ulog_any_user_buffer value for context
 */
void
operation_set_any_user_buffer(struct operation_context *ctx,
	int any_user_buffer)
{
	ctx->ulog_any_user_buffer = any_user_buffer;
}

/*
 * operation_get_any_user_buffer -- get ulog_any_user_buffer value from context
 */
int
operation_get_any_user_buffer(struct operation_context *ctx)
{
	return ctx->ulog_any_user_buffer;
}

/*
 * operation_process_persistent_redo -- (internal) process using ulog
 */
static void
operation_process_persistent_redo(struct operation_context *ctx)
{
	ASSERTeq(ctx->pshadow_ops.capacity % CACHELINE_SIZE, 0);

	ulog_store(ctx->ulog, ctx->pshadow_ops.ulog,
		ctx->pshadow_ops.offset, ctx->ulog_base_nbytes,
		ctx->ulog_capacity,
		&ctx->next, ctx->p_ops);

	ulog_process(ctx->pshadow_ops.ulog, OBJ_OFF_IS_VALID_FROM_CTX,
		ctx->p_ops);

	ulog_clobber(ctx->ulog, &ctx->next, ctx->p_ops);
}

/*
 * operation_process_persistent_undo -- (internal) process using ulog
 */
static void
operation_process_persistent_undo(struct operation_context *ctx)
{
	ASSERTeq(ctx->pshadow_ops.capacity % CACHELINE_SIZE, 0);

	ulog_process(ctx->ulog, OBJ_OFF_IS_VALID_FROM_CTX, ctx->p_ops);
}

/*
 * operation_reserve -- (internal) reserves new capacity in persistent ulog log
 */
int
operation_reserve(struct operation_context *ctx, size_t new_capacity)
{
	if (new_capacity > ctx->ulog_capacity) {
		if (ctx->extend == NULL) {
			ERR("no extend function present");
			return -1;
		}

		if (ulog_reserve(ctx->ulog,
		    ctx->ulog_base_nbytes,
		    ctx->ulog_curr_gen_num,
		    ctx->ulog_auto_reserve,
		    &new_capacity, ctx->extend,
		    &ctx->next, ctx->p_ops) != 0)
			return -1;
		ctx->ulog_capacity = new_capacity;
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
	VALGRIND_ANNOTATE_NEW_MEMORY(tlog->ulog, sizeof(struct ulog) +
		tlog->capacity);
	VALGRIND_ANNOTATE_NEW_MEMORY(plog->ulog, sizeof(struct ulog) +
		plog->capacity);
	tlog->offset = 0;
	plog->offset = 0;
	VECQ_REINIT(&ctx->merge_entries);

	ctx->ulog_curr_offset = 0;
	ctx->ulog_curr_capacity = 0;
	ctx->ulog_curr_gen_num = 0;
	ctx->ulog_curr = NULL;
	ctx->total_logged = 0;
	ctx->ulog_auto_reserve = 1;
	ctx->ulog_any_user_buffer = 0;
}

/*
 * operation_start -- initializes and starts a new operation
 */
void
operation_start(struct operation_context *ctx)
{
	operation_init(ctx);
	ASSERTeq(ctx->state, OPERATION_IDLE);
	ctx->state = OPERATION_IN_PROGRESS;
}

void
operation_resume(struct operation_context *ctx)
{
	operation_start(ctx);
	ctx->total_logged = ulog_base_nbytes(ctx->ulog);
}

/*
 * operation_cancel -- cancels a running operation
 */
void
operation_cancel(struct operation_context *ctx)
{
	ASSERTeq(ctx->state, OPERATION_IN_PROGRESS);
	ctx->state = OPERATION_IDLE;
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
	int redo_process = ctx->type == LOG_TYPE_REDO &&
		ctx->pshadow_ops.offset != 0;
	if (redo_process &&
	    ctx->pshadow_ops.offset == sizeof(struct ulog_entry_val)) {
		struct ulog_entry_base *e = (struct ulog_entry_base *)
			ctx->pshadow_ops.ulog->data;
		ulog_operation_type t = ulog_entry_type(e);
		if (t == ULOG_OPERATION_SET || t == ULOG_OPERATION_AND ||
		    t == ULOG_OPERATION_OR) {
			ulog_entry_apply(e, 1, ctx->p_ops);
			redo_process = 0;
		}
	}

	if (redo_process) {
		operation_process_persistent_redo(ctx);
		ctx->state = OPERATION_CLEANUP;
	} else if (ctx->type == LOG_TYPE_UNDO && ctx->total_logged != 0) {
		operation_process_persistent_undo(ctx);
		ctx->state = OPERATION_CLEANUP;
	}

	/* process transient entries with transient memory ops */
	if (ctx->transient_ops.offset != 0)
		ulog_process(ctx->transient_ops.ulog, NULL, &ctx->t_ops);
}

/*
 * operation_finish -- finalizes the operation
 */
void
operation_finish(struct operation_context *ctx, unsigned flags)
{
	ASSERTne(ctx->state, OPERATION_IDLE);

	if (ctx->type == LOG_TYPE_UNDO && ctx->total_logged != 0)
		ctx->state = OPERATION_CLEANUP;

	if (ctx->ulog_any_user_buffer) {
		flags |= ULOG_ANY_USER_BUFFER;
		ctx->state = OPERATION_CLEANUP;
	}

	if (ctx->state != OPERATION_CLEANUP)
		goto out;

	if (ctx->type == LOG_TYPE_UNDO) {
		int ret = ulog_clobber_data(ctx->ulog,
			ctx->total_logged, ctx->ulog_base_nbytes,
			&ctx->next, ctx->ulog_free,
			operation_user_buffer_remove,
			ctx->p_ops, flags);
		if (ret == 0)
			goto out;
	} else if (ctx->type == LOG_TYPE_REDO) {
		int ret = ulog_free_next(ctx->ulog, ctx->p_ops,
			ctx->ulog_free, operation_user_buffer_remove,
			flags);
		if (ret == 0)
			goto out;
	}

	/* clobbering shrunk the ulog */
	ctx->ulog_capacity = ulog_capacity(ctx->ulog,
		ctx->ulog_base_nbytes, ctx->p_ops);
	VEC_CLEAR(&ctx->next);
	ulog_rebuild_next_vec(ctx->ulog, &ctx->next, ctx->p_ops);

out:
	ctx->state = OPERATION_IDLE;
}
