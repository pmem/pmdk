/*
 * Copyright 2015-2016, Intel Corporation
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

#ifndef LIBPMEMOBJ_REDO_H
#define LIBPMEMOBJ_REDO_H

/*
 * redo.h -- redo log public interface
 */

#include <stddef.h>
#include <stdint.h>

struct redo_ctx;

/*
 * redo_log -- redo log entry
 */
struct redo_log {
	uint64_t offset;	/* offset with finish flag */
	uint64_t value;
};

typedef void (*redo_persist_fn)(void *ctx, const void *, size_t);
typedef void (*redo_flush_fn)(void *ctx, const void *, size_t);
typedef int  (*redo_check_offset_fn)(void *ctx, uint64_t offset);

struct redo_ctx *redo_log_config_new(void *base,
		redo_persist_fn persist,
		redo_flush_fn flush,
		redo_check_offset_fn check_offset,
		void *flush_ctx,
		void *check_offset_ctx,
		unsigned redo_num_entries);

void redo_log_config_delete(struct redo_ctx *ctx);

void redo_log_store(struct redo_ctx *ctx, struct redo_log *redo, size_t index,
		uint64_t offset, uint64_t value);
void redo_log_store_last(struct redo_ctx *ctx, struct redo_log *redo,
		size_t index, uint64_t offset, uint64_t value);
void redo_log_set_last(struct redo_ctx *ctx, struct redo_log *redo,
		size_t index);
void redo_log_process(struct redo_ctx *ctx, struct redo_log *redo,
		size_t nentries);
void redo_log_recover(struct redo_ctx *ctx, struct redo_log *redo,
		size_t nentries);
int redo_log_check(struct redo_ctx *ctx, struct redo_log *redo,
		size_t nentries);

size_t redo_log_nflags(struct redo_log *redo, size_t nentries);
uint64_t redo_log_offset(struct redo_log *redo);
int redo_log_is_last(struct redo_log *redo);

#endif
