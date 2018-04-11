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
 * redo.c -- redo log implementation
 */

#include <inttypes.h>

#include "libpmem.h"
#include "redo.h"
#include "out.h"
#include "util.h"
#include "valgrind_internal.h"

/*
 * Finish flag at the least significant bit
 */
#define REDO_FINISH_FLAG	((uint64_t)1<<0)
#define REDO_FLAG_MASK		(~REDO_FINISH_FLAG)

struct redo_ctx {
	void *base;

	struct pmem_ops p_ops;

	redo_check_offset_fn check_offset;
	void *check_offset_ctx;

	unsigned redo_num_entries;
};

/*
 * redo_log_config_new -- allocates redo context
 */
struct redo_ctx *
redo_log_config_new(void *base,
		const struct pmem_ops *p_ops,
		redo_check_offset_fn check_offset,
		void *check_offset_ctx,
		unsigned redo_num_entries)
{
	struct redo_ctx *cfg = Malloc(sizeof(*cfg));
	if (!cfg) {
		ERR("!can't create redo log config");
		return NULL;
	}

	cfg->base = base;
	cfg->p_ops = *p_ops;
	cfg->check_offset = check_offset;
	cfg->check_offset_ctx = check_offset_ctx;
	cfg->redo_num_entries = redo_num_entries;

	return cfg;
}

/*
 * redo_log_config_delete -- frees redo context
 */
void
redo_log_config_delete(struct redo_ctx *ctx)
{
	Free(ctx);
}

/*
 * redo_log_nflags -- (internal) get number of finish flags set
 */
size_t
redo_log_nflags(const struct redo_log *redo, size_t nentries)
{
	size_t ret = 0;
	size_t i;

	for (i = 0; i < nentries; i++) {
		if (redo[i].offset & REDO_FINISH_FLAG)
			ret++;
	}

	LOG(15, "redo %p nentries %zu nflags %zu", redo, nentries, ret);

	return ret;
}

/*
 * redo_log_store -- (internal) store redo log entry at specified index
 */
void
redo_log_store(const struct redo_ctx *ctx, struct redo_log *redo, size_t index,
		uint64_t offset, uint64_t value)
{
	LOG(15, "redo %p index %zu offset %" PRIu64 " value %" PRIu64,
			redo, index, offset, value);

	ASSERTeq(offset & REDO_FINISH_FLAG, 0);
	ASSERT(index < ctx->redo_num_entries);

	redo[index].offset = offset;
	redo[index].value = value;
}

/*
 * redo_log_store_last -- (internal) store last entry at specified index
 */
void
redo_log_store_last(const struct redo_ctx *ctx, struct redo_log *redo,
		size_t index, uint64_t offset, uint64_t value)
{
	LOG(15, "redo %p index %zu offset %" PRIu64 " value %" PRIu64,
			redo, index, offset, value);

	ASSERTeq(offset & REDO_FINISH_FLAG, 0);
	ASSERT(index < ctx->redo_num_entries);
	const struct pmem_ops *p_ops = &ctx->p_ops;

	/* store value of last entry */
	redo[index].value = value;

	/*
	 * persist all redo log entries
	 *
	 * It's safe to use PMEM_F_RELAXED flag because setting finish flag
	 * determines whether all redo log entires are meaningfull or not.
	 *
	 * The finish flag _must_ be persisted without PMEM_F_RELAXED flag.
	 */
	pmemops_xpersist(p_ops, redo, (index + 1) * sizeof(struct redo_log),
			PMEM_F_RELAXED);

	/* store and persist offset of last entry */
	redo[index].offset = offset | REDO_FINISH_FLAG;
	pmemops_persist(p_ops, &redo[index].offset, sizeof(redo[index].offset));
}

/*
 * redo_log_set_last -- (internal) set finish flag in specified entry
 */
void
redo_log_set_last(const struct redo_ctx *ctx, struct redo_log *redo,
		size_t index)
{
	LOG(15, "redo %p index %zu", redo, index);

	ASSERT(index < ctx->redo_num_entries);
	const struct pmem_ops *p_ops = &ctx->p_ops;

	/*
	 * persist all redo log entries
	 *
	 * It's safe to use PMEM_F_RELAXED flag because setting finish flag
	 * determines whether all redo log entires are meaningfull or not.
	 *
	 * The finish flag _must_ be persisted without PMEM_F_RELAXED flag.
	 */
	pmemops_xpersist(p_ops, redo, (index + 1) * sizeof(struct redo_log),
			PMEM_F_RELAXED);

	/* set finish flag of last entry and persist */
	redo[index].offset |= REDO_FINISH_FLAG;
	pmemops_persist(p_ops, &redo[index].offset, sizeof(redo[index].offset));
}

/*
 * redo_log_process -- (internal) process redo log entries
 */
void
redo_log_process(const struct redo_ctx *ctx, struct redo_log *redo,
		size_t nentries)
{
	LOG(15, "redo %p nentries %zu", redo, nentries);

#ifdef DEBUG
	ASSERTeq(redo_log_check(ctx, redo, nentries), 0);
#endif
	const struct pmem_ops *p_ops = &ctx->p_ops;

	uint64_t *val;
	while ((redo->offset & REDO_FINISH_FLAG) == 0) {
		val = (uint64_t *)((uintptr_t)ctx->base + redo->offset);
		VALGRIND_ADD_TO_TX(val, sizeof(*val));
		*val = redo->value;
		VALGRIND_REMOVE_FROM_TX(val, sizeof(*val));

		pmemops_flush(p_ops, val, sizeof(uint64_t));

		redo++;
	}

	uint64_t offset = redo->offset & REDO_FLAG_MASK;
	val = (uint64_t *)((uintptr_t)ctx->base + offset);
	VALGRIND_ADD_TO_TX(val, sizeof(*val));
	*val = redo->value;
	VALGRIND_REMOVE_FROM_TX(val, sizeof(*val));

	pmemops_persist(p_ops, val, sizeof(uint64_t));

	redo->offset = 0;

	pmemops_persist(p_ops, &redo->offset, sizeof(redo->offset));
}

/*
 * redo_log_recover -- (internal) recovery of redo log
 *
 * The redo_log_recover shall be preceded by redo_log_check call.
 */
void
redo_log_recover(const struct redo_ctx *ctx, struct redo_log *redo,
		size_t nentries)
{
	LOG(15, "redo %p nentries %zu", redo, nentries);
	ASSERTne(ctx, NULL);

	size_t nflags = redo_log_nflags(redo, nentries);
	ASSERT(nflags < 2);

	if (nflags == 1)
		redo_log_process(ctx, redo, nentries);
}

/*
 * redo_log_check -- (internal) check consistency of redo log entries
 */
int
redo_log_check(const struct redo_ctx *ctx, struct redo_log *redo,
		size_t nentries)
{
	LOG(15, "redo %p nentries %zu", redo, nentries);
	ASSERTne(ctx, NULL);

	size_t nflags = redo_log_nflags(redo, nentries);

	if (nflags > 1) {
		LOG(15, "redo %p too many finish flags", redo);
		return -1;
	}

	if (nflags == 1) {
		void *cctx = ctx->check_offset_ctx;

		while ((redo->offset & REDO_FINISH_FLAG) == 0) {
			if (!ctx->check_offset(cctx, redo->offset)) {
				LOG(15, "redo %p invalid offset %" PRIu64,
						redo, redo->offset);
				return -1;
			}
			redo++;
		}

		uint64_t offset = redo->offset & REDO_FLAG_MASK;
		if (!ctx->check_offset(cctx, offset)) {
			LOG(15, "redo %p invalid offset %" PRIu64,
			    redo, offset);
			return -1;
		}
	}

	return 0;
}

/*
 * redo_log_offset -- returns offset
 */
uint64_t
redo_log_offset(const struct redo_log *redo)
{
	return redo->offset & REDO_FLAG_MASK;
}

/*
 * redo_log_is_last -- returns 1/0
 */
int
redo_log_is_last(const struct redo_log *redo)
{
	return redo->offset & REDO_FINISH_FLAG;
}

/*
 * redo_get_pmem_ops -- returns pmem_ops
 */
const struct pmem_ops *
redo_get_pmem_ops(const struct redo_ctx *ctx)
{
	return &ctx->p_ops;
}
