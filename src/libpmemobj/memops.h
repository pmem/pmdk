/*
 * Copyright 2016-2019, Intel Corporation
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

struct operation_context;

struct operation_context *
operation_new(struct ulog *redo, size_t ulog_base_nbytes,
	ulog_extend_fn extend, ulog_free_fn ulog_free,
	const struct pmem_ops *p_ops, enum log_type type);

void operation_init(struct operation_context *ctx);
void operation_start(struct operation_context *ctx);
void operation_resume(struct operation_context *ctx);

void operation_delete(struct operation_context *ctx);

int operation_add_buffer(struct operation_context *ctx,
	void *dest, void *src, size_t size, ulog_operation_type type);

int operation_add_entry(struct operation_context *ctx,
	void *ptr, uint64_t value, ulog_operation_type type);
int operation_add_typed_entry(struct operation_context *ctx,
	void *ptr, uint64_t value,
	ulog_operation_type type, enum operation_log_type log_type);

int operation_reserve(struct operation_context *ctx, size_t new_capacity);
void operation_process(struct operation_context *ctx);
void operation_finish(struct operation_context *ctx, unsigned flags);
void operation_cancel(struct operation_context *ctx);

#ifdef __cplusplus
}
#endif

#endif
