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
 * redo.h -- redo log public interface
 */

#ifndef LIBPMEMOBJ_REDO_H
#define LIBPMEMOBJ_REDO_H 1

#include <stddef.h>
#include <stdint.h>

#include "vec.h"
#include "pmemops.h"

struct redo_log_entry_base {
	uint64_t offset; /* offset with operation type flag */
};

/*
 * redo_log_entry_val -- redo log entry
 */
struct redo_log_entry_val {
	struct redo_log_entry_base base;
	uint64_t value; /* value to be applied */
};

#define REDO_LOG(capacity_bytes) {\
	/* 64 bytes of metadata */\
	uint64_t checksum; /* checksum of redo header and its entries */\
	uint64_t next; /* offset of redo log extension */\
	uint64_t capacity; /* capacity of this redo in bytes */\
	uint64_t unused[5]; /* must be 0 */\
	/* N bytes of data */\
	uint8_t data[capacity_bytes];\
}\

#define SIZEOF_REDO_LOG(base_capacity)\
(sizeof(struct redo_log) + base_capacity)

struct redo_log REDO_LOG(0);

VEC(redo_next, uint64_t);

enum redo_operation_type {
	REDO_OPERATION_SET	=	0b000,
	REDO_OPERATION_AND	=	0b001,
	REDO_OPERATION_OR	=	0b010,
};

#define REDO_BIT_OPERATIONS (REDO_OPERATION_AND | REDO_OPERATION_OR)
#define REDO_VAL_OPERATIONS (REDO_BIT_OPERATIONS | REDO_OPERATION_SET)

typedef int (*redo_check_offset_fn)(void *ctx, uint64_t offset);
typedef int (*redo_extend_fn)(void *, uint64_t *);
typedef int (*redo_entry_cb)(struct redo_log_entry_base *e, void *arg,
	const struct pmem_ops *p_ops);

size_t redo_log_capacity(struct redo_log *redo, size_t redo_base_bytes,
	const struct pmem_ops *p_ops);
void redo_log_rebuild_next_vec(struct redo_log *redo, struct redo_next *next,
	const struct pmem_ops *p_ops);

int redo_log_foreach_entry(struct redo_log *redo,
	redo_entry_cb cb, void *arg, const struct pmem_ops *ops);

int redo_log_reserve(struct redo_log *redo,
	size_t redo_base_nbytes, size_t *new_capacity_bytes,
	redo_extend_fn extend, struct redo_next *next,
	const struct pmem_ops *p_ops);

void redo_log_store(struct redo_log *dest,
	struct redo_log *src, size_t nbytes, size_t redo_base_nbytes,
	struct redo_next *next, const struct pmem_ops *p_ops);

void redo_log_clobber(struct redo_log *dest, struct redo_next *next,
	const struct pmem_ops *p_ops);
void redo_log_process(struct redo_log *redo, redo_check_offset_fn check,
	const struct pmem_ops *p_ops);

int redo_log_recovery_needed(struct redo_log *redo,
	const struct pmem_ops *p_ops);

uint64_t redo_log_entry_offset(const struct redo_log_entry_base *entry);
enum redo_operation_type redo_log_entry_type(
	const struct redo_log_entry_base *entry);

struct redo_log_entry_val *redo_log_entry_val_create(struct redo_log *redo,
	size_t offset, uint64_t *dest, uint64_t value,
	enum redo_operation_type type,
	const struct pmem_ops *p_ops);

void redo_log_entry_apply(const struct redo_log_entry_base *e, int persist,
	const struct pmem_ops *p_ops);

size_t redo_log_entry_size(const struct redo_log_entry_base *entry);

void redo_log_recover(struct redo_log *redo, redo_check_offset_fn check,
	const struct pmem_ops *p_ops);
int redo_log_check(struct redo_log *redo, redo_check_offset_fn check,
	const struct pmem_ops *p_ops);

#endif
