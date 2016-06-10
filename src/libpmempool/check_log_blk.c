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
 * check_log_blk.c -- check pmemlog and pmemblk
 */

#include <inttypes.h>
#include <sys/param.h>

#include "out.h"
#include "btt.h"
#include "libpmempool.h"
#include "pmempool.h"
#include "pool.h"
#include "check_util.h"

/* assure size match between global and internal check step data */
union location {
	/* internal check step data */
	struct {
		unsigned step;
	};
	/* global check step data */
	struct check_step_data step_data;
};

enum question {
	Q_LOG_START_OFFSET,
	Q_LOG_END_OFFSET,
	Q_LOG_WRITE_OFFSET,
	Q_BLK_BSIZE,
};

/*
 * log_read -- (internal) read pmemlog header
 */
static int
log_read(PMEMpoolcheck *ppc)
{
	/*
	 * Here we want to read the pmemlog header without the pool_hdr as we've
	 * already done it before.
	 *
	 * Take the pointer to fields right after pool_hdr, compute the size and
	 * offset of remaining fields.
	 */
	uint8_t *ptr = (uint8_t *)&ppc->pool->hdr.log;
	ptr += sizeof(ppc->pool->hdr.log.hdr);

	size_t size = sizeof(ppc->pool->hdr.log) -
		sizeof(ppc->pool->hdr.log.hdr);
	uint64_t offset = sizeof(ppc->pool->hdr.log.hdr);

	if (pool_read(ppc->pool, ptr, size, offset))
		return CHECK_ERR(ppc, "cannot read pmemlog structure");

	/* endianness conversion */
	pmemlog_convert2h(&ppc->pool->hdr.log);
	return 0;
}

/*
 * log_hdr_check -- (internal) check pmemlog header
 */
static int
log_hdr_check(PMEMpoolcheck *ppc, union location *loc)
{
	LOG(3, NULL);

	CHECK_INFO(ppc, "checking pmemlog header");

	if (log_read(ppc)) {
		ppc->result = CHECK_RESULT_ERROR;
		return -1;
	}

	/* determine constant values for pmemlog */
	const uint64_t d_start_offset =
		roundup(sizeof(ppc->pool->hdr.log), LOG_FORMAT_DATA_ALIGN);

	if (ppc->pool->hdr.log.start_offset != d_start_offset) {
		if (CHECK_ASK(ppc, Q_LOG_START_OFFSET,
				"invalid pmemlog.start_offset: 0x%jx.|Do you "
				"want to set pmemlog.start_offset to default "
				"0x%jx?",
				ppc->pool->hdr.log.start_offset,
				d_start_offset))
			goto error;
	}

	if (ppc->pool->hdr.log.end_offset != ppc->pool->set_file->size) {
		if (CHECK_ASK(ppc, Q_LOG_END_OFFSET,
				"invalid pmemlog.end_offset: 0x%jx.|Do you "
				"want to set pmemlog.end_offset to 0x%jx?",
				ppc->pool->hdr.log.end_offset,
				ppc->pool->set_file->size))
			goto error;
	}

	if (ppc->pool->hdr.log.write_offset < d_start_offset ||
		ppc->pool->hdr.log.write_offset > ppc->pool->set_file->size) {
		if (CHECK_ASK(ppc, Q_LOG_WRITE_OFFSET,
				"invalid pmemlog.write_offset: 0x%jx.|Do you "
				"want to set pmemlog.write_offset to "
				"pmemlog.end_offset?",
				ppc->pool->hdr.log.write_offset))
			goto error;
	}

	if (ppc->result == CHECK_RESULT_CONSISTENT ||
		ppc->result == CHECK_RESULT_REPAIRED)
		CHECK_INFO(ppc, "pmemlog header correct");

	return check_questions_sequence_validate(ppc);

error:
	ppc->result = CHECK_RESULT_NOT_CONSISTENT;
	check_end(ppc->data);
	return -1;
}

/*
 * log_hdr_fix -- (internal) fix pmemlog header
 */
static int
log_hdr_fix(PMEMpoolcheck *ppc, struct check_step_data *location,
	uint32_t question, void *ctx)
{
	LOG(3, NULL);

	uint64_t d_start_offset;

	switch (question) {
	case Q_LOG_START_OFFSET:
		/* determine constant values for pmemlog */
		d_start_offset = roundup(sizeof(ppc->pool->hdr.log),
			LOG_FORMAT_DATA_ALIGN);
		CHECK_INFO(ppc, "setting pmemlog.start_offset to 0x%jx",
			d_start_offset);
		ppc->pool->hdr.log.start_offset = d_start_offset;
		break;
	case Q_LOG_END_OFFSET:
		CHECK_INFO(ppc, "setting pmemlog.end_offset to 0x%jx",
			ppc->pool->set_file->size);
		ppc->pool->hdr.log.end_offset = ppc->pool->set_file->size;
			break;
	case Q_LOG_WRITE_OFFSET:
		CHECK_INFO(ppc, "setting pmemlog.write_offset to "
			"pmemlog.end_offset");
		ppc->pool->hdr.log.write_offset = ppc->pool->set_file->size;
		break;
	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

/*
 * blk_get_max_bsize -- (internal) return maximum size of block for given file
 *	size
 */
static inline uint32_t
blk_get_max_bsize(uint64_t fsize)
{
	LOG(3, NULL);

	if (fsize == 0)
		return 0;

	/* default nfree */
	uint32_t nfree = BTT_DEFAULT_NFREE;

	/* number of blocks must be at least 2 * nfree */
	uint32_t internal_nlba = 2 * nfree;

	/* compute arena size from file size without pmemblk structure */
	uint64_t arena_size = fsize - sizeof(struct pmemblk);
	if (arena_size > BTT_MAX_ARENA)
		arena_size = BTT_MAX_ARENA;
	arena_size = btt_arena_datasize(arena_size, nfree);

	/* compute maximum internal LBA size */
	uint64_t internal_lbasize = (arena_size - BTT_ALIGNMENT) /
			internal_nlba - BTT_MAP_ENTRY_SIZE;
	ASSERT(internal_lbasize <= UINT32_MAX);

	if (internal_lbasize < BTT_MIN_LBA_SIZE)
		internal_lbasize = BTT_MIN_LBA_SIZE;

	internal_lbasize = roundup(internal_lbasize, BTT_INTERNAL_LBA_ALIGNMENT)
		- BTT_INTERNAL_LBA_ALIGNMENT;

	return (uint32_t)internal_lbasize;
}

/*
 * blk_read -- (internal) read pmemblk header
 */
static int
blk_read(PMEMpoolcheck *ppc)
{
	/*
	 * Here we want to read the pmemblk header without the pool_hdr as we've
	 * already done it before.
	 *
	 * Take the pointer to fields right after pool_hdr, compute the size and
	 * offset of remaining fields.
	 */
	uint8_t *ptr = (uint8_t *)&ppc->pool->hdr.blk;
	ptr += sizeof(ppc->pool->hdr.blk.hdr);

	size_t size = sizeof(ppc->pool->hdr.blk) -
		sizeof(ppc->pool->hdr.blk.hdr);
	uint64_t offset = sizeof(ppc->pool->hdr.blk.hdr);

	if (pool_read(ppc->pool, ptr, size, offset)) {
		return CHECK_ERR(ppc, "cannot read pmemblk structure");
	}

	/* endianness conversion */
	ppc->pool->hdr.blk.bsize = le32toh(ppc->pool->hdr.blk.bsize);

	return 0;
}

/*
 * blk_bsize_valid -- (internal) check if block size is valid for given file
 *	size
 */
static int
blk_bsize_valid(uint32_t bsize, uint64_t fsize)
{
	uint32_t max_bsize = blk_get_max_bsize(fsize);
	return !(bsize < max_bsize);
}

/*
 * blk_hdr_check -- (internal) check pmemblk header
 */
static int
blk_hdr_check(PMEMpoolcheck *ppc, union location *loc)
{
	LOG(3, NULL);

	CHECK_INFO(ppc, "checking pmemblk header");

	if (blk_read(ppc)) {
		ppc->result = CHECK_RESULT_ERROR;
		return -1;
	}

	/* check for valid BTT Info arena as we can take bsize from it */
	if (!ppc->pool->bttc.valid)
		pool_blk_get_first_valid_arena(ppc->pool, &ppc->pool->bttc);

	if (ppc->pool->bttc.valid) {
		const uint32_t btt_bsize =
			ppc->pool->bttc.btt_info.external_lbasize;

		if (ppc->pool->hdr.blk.bsize != btt_bsize) {
			CHECK_ASK(ppc, Q_BLK_BSIZE,
				"invalid pmemblk.bsize.|Do you want to set "
				"pmemblk.bsize to %lu from BTT Info?",
				btt_bsize);
		}
	} else if (!ppc->pool->bttc.zeroed) {
		if (ppc->pool->hdr.blk.bsize < BTT_MIN_LBA_SIZE ||
				blk_bsize_valid(ppc->pool->hdr.blk.bsize,
				ppc->pool->set_file->size)) {
			ppc->result = CHECK_RESULT_CANNOT_REPAIR;
			return CHECK_ERR(ppc, "invalid pmemblk.bsize");
		}
	}

	if (ppc->result == CHECK_RESULT_CONSISTENT ||
			ppc->result == CHECK_RESULT_REPAIRED)
		CHECK_INFO(ppc, "pmemblk header correct");

	return check_questions_sequence_validate(ppc);
}

/*
 * blk_hdr_fix -- (internal) fix pmemblk header
 */
static int
blk_hdr_fix(PMEMpoolcheck *ppc, struct check_step_data *location,
	uint32_t question, void *ctx)
{
	LOG(3, NULL);

	uint32_t btt_bsize;

	switch (question) {
	case Q_BLK_BSIZE:
		/*
		 * check for valid BTT Info arena as we can take bsize from it
		 */
		if (!ppc->pool->bttc.valid)
			pool_blk_get_first_valid_arena(ppc->pool,
				&ppc->pool->bttc);
		btt_bsize = ppc->pool->bttc.btt_info.external_lbasize;
		CHECK_INFO(ppc, "setting pmemblk.b_size to 0x%x", btt_bsize);
		ppc->pool->hdr.blk.bsize = btt_bsize;
		break;
	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

struct step {
	int (*check)(PMEMpoolcheck *, union location *);
	int (*fix)(PMEMpoolcheck *, struct check_step_data *, uint32_t, void *);
	enum pool_type type;
};

static const struct step steps[] = {
	{
		.check	= log_hdr_check,
		.type	= POOL_TYPE_LOG
	},
	{
		.fix	= log_hdr_fix,
		.type	= POOL_TYPE_LOG
	},
	{
		.check	= blk_hdr_check,
		.type	= POOL_TYPE_BLK
	},
	{
		.fix	= blk_hdr_fix,
		.type	= POOL_TYPE_BLK
	},
	{
		.check	= NULL,
	},
};

/*
 * step_exe -- (internal) perform single step according to its parameters
 */
static inline int
step_exe(PMEMpoolcheck *ppc, union location *loc)
{
	const struct step *step = &steps[loc->step++];

	if (!(step->type & ppc->pool->params.type))
		return 0;

	if (!step->fix)
		return step->check(ppc, loc);

	if (step->type == POOL_TYPE_LOG) {
		if (log_read(ppc)) {
			ppc->result = CHECK_RESULT_ERROR;
			return -1;
		}
	} else if (step->type == POOL_TYPE_BLK) {
		if (blk_read(ppc)) {
			ppc->result = CHECK_RESULT_ERROR;
			return -1;
		}
	}

	return check_answer_loop(ppc, &loc->step_data, NULL, step->fix);
}

/*
 * check_log_blk -- entry point for pmemlog and pmemblk checks
 */
void
check_log_blk(PMEMpoolcheck *ppc)
{
	LOG(3, NULL);

	COMPILE_ERROR_ON(sizeof(union location) !=
		sizeof(struct check_step_data));

	union location *loc = (union location *)check_get_step_data(ppc->data);

	/* do all checks */
	while (CHECK_NOT_COMPLETE(loc, steps)) {
		if (step_exe(ppc, loc))
			break;
	}
}
