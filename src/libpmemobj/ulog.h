/*
 * Copyright 2015-2019, Intel Corporation
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
 * ulog.h -- unified log public interface
 */

#ifndef LIBPMEMOBJ_ULOG_H
#define LIBPMEMOBJ_ULOG_H 1

#include <stddef.h>
#include <stdint.h>

#include "vec.h"
#include "pmemops.h"

struct ulog_entry_base {
	uint64_t offset; /* offset with operation type flag */
};

/*
 * ulog_entry_val -- log entry
 */
struct ulog_entry_val {
	struct ulog_entry_base base;
	uint64_t value; /* value to be applied */
};

/*
 * ulog_entry_buf - ulog buffer entry
 */
struct ulog_entry_buf {
	struct ulog_entry_base base; /* offset with operation type flag */
	uint64_t checksum; /* checksum of the entire log entry */
	uint64_t size; /* size of the buffer to be modified */
	uint8_t data[]; /* content to fill in */
};

/*
 * This structure *must* be located at a cacheline boundary. To achieve this,
 * the next field is always allocated with extra padding, and then the offset
 * is additionally aligned.
 */
#define ULOG(capacity_bytes) {\
	/* 64 bytes of metadata */\
	uint64_t checksum; /* checksum of ulog header and its entries */\
	uint64_t next; /* offset of ulog extension */\
	uint64_t capacity; /* capacity of this ulog in bytes */\
	uint64_t gen_num; /* generation counter */\
	uint64_t flags; /* ulog flags */\
	uint64_t unused[3]; /* must be 0 */\
	uint8_t data[capacity_bytes]; /* N bytes of data */\
}\

#define SIZEOF_ULOG(base_capacity)\
(sizeof(struct ulog) + base_capacity)

/*
 * Ulog buffer allocated by the user must be marked by this flag.
 * It is important to not free it at the end:
 * what user has allocated - user should free himself.
 */
#define ULOG_USER_OWNED (1U << 0)

/* use this for allocations of aligned ulog extensions */
#define SIZEOF_ALIGNED_ULOG(base_capacity)\
(SIZEOF_ULOG(base_capacity) + CACHELINE_SIZE)

struct ulog ULOG(0);

VEC(ulog_next, uint64_t);

typedef uint64_t ulog_operation_type;

#define ULOG_OPERATION_SET		(0b000ULL << 61ULL)
#define ULOG_OPERATION_AND		(0b001ULL << 61ULL)
#define ULOG_OPERATION_OR		(0b010ULL << 61ULL)
#define ULOG_OPERATION_BUF_SET		(0b101ULL << 61ULL)
#define ULOG_OPERATION_BUF_CPY		(0b110ULL << 61ULL)

#define ULOG_BIT_OPERATIONS (ULOG_OPERATION_AND | ULOG_OPERATION_OR)

/* immediately frees all associated ulog structures */
#define ULOG_FREE_AFTER_FIRST (1U << 0)
/* increments gen_num of the first, preallocated, ulog */
#define ULOG_INC_FIRST_GEN_NUM (1U << 1)
/* informs if there was any buffer allocated by user in the tx  */
#define ULOG_ANY_USER_BUFFER (1U << 2)

typedef int (*ulog_check_offset_fn)(void *ctx, uint64_t offset);
typedef int (*ulog_extend_fn)(void *, uint64_t *, uint64_t);
typedef int (*ulog_entry_cb)(struct ulog_entry_base *e, void *arg,
	const struct pmem_ops *p_ops);
typedef void (*ulog_free_fn)(void *base, uint64_t *next);
typedef int (*ulog_rm_user_buffer_fn)(void *, void *addr);

struct ulog *ulog_next(struct ulog *ulog, const struct pmem_ops *p_ops);

void ulog_construct(uint64_t offset, size_t capacity, uint64_t gen_num,
		int flush, uint64_t flags, const struct pmem_ops *p_ops);

size_t ulog_capacity(struct ulog *ulog, size_t ulog_base_bytes,
	const struct pmem_ops *p_ops);
void ulog_rebuild_next_vec(struct ulog *ulog, struct ulog_next *next,
	const struct pmem_ops *p_ops);

int ulog_foreach_entry(struct ulog *ulog,
	ulog_entry_cb cb, void *arg, const struct pmem_ops *ops);

int ulog_reserve(struct ulog *ulog,
	size_t ulog_base_nbytes, size_t gen_num,
	int auto_reserve, size_t *new_capacity_bytes,
	ulog_extend_fn extend, struct ulog_next *next,
	const struct pmem_ops *p_ops);

void ulog_store(struct ulog *dest,
	struct ulog *src, size_t nbytes, size_t ulog_base_nbytes,
	struct ulog_next *next, const struct pmem_ops *p_ops);

int ulog_free_next(struct ulog *u, const struct pmem_ops *p_ops,
		ulog_free_fn ulog_free, ulog_rm_user_buffer_fn user_buff_remove,
		uint64_t flags);
void ulog_clobber(struct ulog *dest, struct ulog_next *next,
	const struct pmem_ops *p_ops);
int ulog_clobber_data(struct ulog *dest,
	size_t nbytes, size_t ulog_base_nbytes,
	struct ulog_next *next, ulog_free_fn ulog_free,
	ulog_rm_user_buffer_fn user_buff_remove,
	const struct pmem_ops *p_ops, unsigned flags);
void ulog_clobber_entry(const struct ulog_entry_base *e,
	const struct pmem_ops *p_ops);

void ulog_process(struct ulog *ulog, ulog_check_offset_fn check,
	const struct pmem_ops *p_ops);

size_t ulog_base_nbytes(struct ulog *ulog);
int ulog_recovery_needed(struct ulog *ulog, int verify_checksum);
struct ulog *ulog_by_offset(size_t offset, const struct pmem_ops *p_ops);

uint64_t ulog_entry_offset(const struct ulog_entry_base *entry);
ulog_operation_type ulog_entry_type(
	const struct ulog_entry_base *entry);

struct ulog_entry_val *ulog_entry_val_create(struct ulog *ulog,
	size_t offset, uint64_t *dest, uint64_t value,
	ulog_operation_type type,
	const struct pmem_ops *p_ops);

struct ulog_entry_buf *
ulog_entry_buf_create(struct ulog *ulog, size_t offset,
	uint64_t gen_num, uint64_t *dest, const void *src, uint64_t size,
	ulog_operation_type type, const struct pmem_ops *p_ops);

void ulog_entry_apply(const struct ulog_entry_base *e, int persist,
	const struct pmem_ops *p_ops);

size_t ulog_entry_size(const struct ulog_entry_base *entry);

void ulog_recover(struct ulog *ulog, ulog_check_offset_fn check,
	const struct pmem_ops *p_ops);
int ulog_check(struct ulog *ulog, ulog_check_offset_fn check,
	const struct pmem_ops *p_ops);

#endif
