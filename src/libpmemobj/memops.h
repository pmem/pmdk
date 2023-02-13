/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

/*
 * memops.h -- aggregated memory operations helper definitions
 */

#ifndef LIBPMEMOBJ_MEMOPS_H
#define LIBPMEMOBJ_MEMOPS_H 1

#include <stddef.h>
#include <stdint.h>

#include "vec.h"
#include "pmemops.h"
#include "ulog.h"
#include "lane.h"
#include "vecq.h"

#ifdef __cplusplus
extern "C" {
#endif

enum operation_log_type {
	LOG_PERSISTENT, /* log of persistent modifications */
	LOG_TRANSIENT, /* log of transient memory modifications */

	MAX_OPERATION_LOG_TYPE
};

enum log_type {
	LOG_TYPE_UNDO,
	LOG_TYPE_REDO,

	MAX_LOG_TYPE,
};

struct user_buffer_def {
	void *addr;
	size_t size;
};

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

struct operation_context *
operation_new(struct ulog *redo, size_t ulog_base_nbytes,
	ulog_extend_fn extend, ulog_free_fn ulog_free,
	const struct pmem_ops *p_ops, enum log_type type);

void operation_init(struct operation_context *ctx);
void operation_start(struct operation_context *ctx);
void operation_resume(struct operation_context *ctx);

void operation_delete(struct operation_context *ctx);
void operation_free_logs(struct operation_context *ctx, uint64_t flags);

int operation_add_buffer(struct operation_context *ctx,
	void *dest, void *src, size_t size, ulog_operation_type type);

int operation_add_entry(struct operation_context *ctx,
	void *ptr, uint64_t value, ulog_operation_type type);
int operation_add_typed_entry(struct operation_context *ctx,
	void *ptr, uint64_t value,
	ulog_operation_type type, enum operation_log_type log_type);
int operation_user_buffer_verify_align(struct operation_context *ctx,
		struct user_buffer_def *userbuf);
void operation_add_user_buffer(struct operation_context *ctx,
		struct user_buffer_def *userbuf);
void operation_set_auto_reserve(struct operation_context *ctx,
		int auto_reserve);
void operation_set_any_user_buffer(struct operation_context *ctx,
	int any_user_buffer);
int operation_get_any_user_buffer(struct operation_context *ctx);
int operation_user_buffer_range_cmp(const void *lhs, const void *rhs);

int operation_reserve(struct operation_context *ctx, size_t new_capacity);
void operation_process(struct operation_context *ctx);
void operation_finish(struct operation_context *ctx, unsigned flags);
void operation_cancel(struct operation_context *ctx);

#ifdef __cplusplus
}
#endif

#endif
