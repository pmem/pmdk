// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * check_blk.c -- check pmemblk
 */

#include <inttypes.h>
#include <sys/param.h>
#include <endian.h>

#include "out.h"
#include "btt.h"
#include "libpmempool.h"
#include "pmempool.h"
#include "pool.h"
#include "check_util.h"

enum question {
	Q_BLK_BSIZE,
};

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
	return (bsize >= max_bsize);
}

/*
 * blk_hdr_check -- (internal) check pmemblk header
 */
static int
blk_hdr_check(PMEMpoolcheck *ppc, location *loc)
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
				"pmemblk.bsize to %u from BTT Info?",
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
blk_hdr_fix(PMEMpoolcheck *ppc, location *loc, uint32_t question, void *ctx)
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
	int (*check)(PMEMpoolcheck *, location *);
	int (*fix)(PMEMpoolcheck *, location *, uint32_t, void *);
	enum pool_type type;
};

static const struct step steps[] = {
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
		.fix	= NULL,
	},
};

/*
 * step_exe -- (internal) perform single step according to its parameters
 */
static inline int
step_exe(PMEMpoolcheck *ppc, location *loc)
{
	ASSERT(loc->step < ARRAY_SIZE(steps));
	ASSERTeq(ppc->pool->params.type, POOL_TYPE_BLK);

	const struct step *step = &steps[loc->step++];

	if (!(step->type & ppc->pool->params.type))
		return 0;

	if (!step->fix)
		return step->check(ppc, loc);

	if (blk_read(ppc)) {
		ppc->result = CHECK_RESULT_ERROR;
		return -1;
	}

	return check_answer_loop(ppc, loc, NULL, 1, step->fix);
}

/*
 * check_blk -- entry point for pmemblk checks
 */
void
check_blk(PMEMpoolcheck *ppc)
{
	LOG(3, NULL);

	location *loc = check_get_step_data(ppc->data);

	/* do all checks */
	while (CHECK_NOT_COMPLETE(loc, steps)) {
		if (step_exe(ppc, loc))
			break;
	}
}
