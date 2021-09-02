// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2021, Intel Corporation */

/*
 * check_btt_info.c -- check BTT Info
 */

#include <stdlib.h>
#include <stdint.h>
#include <endian.h>

#include "out.h"
#include "util.h"
#include "btt.h"
#include "libpmempool.h"
#include "pmempool.h"
#include "pool.h"
#include "check_util.h"

enum question {
	Q_RESTORE_FROM_BACKUP,
	Q_REGENERATE,
	Q_REGENERATE_CHECKSUM,
	Q_RESTORE_FROM_HEADER
};

/*
 * location_release -- (internal) release check_btt_info_loc allocations
 */
static void
location_release(location *loc)
{
	free(loc->arenap);
	loc->arenap = NULL;
}

/*
 * btt_info_checksum -- (internal) check BTT Info checksum
 */
static int
btt_info_checksum(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	loc->arenap = calloc(1, sizeof(struct arena));
	if (!loc->arenap) {
		ERR("!calloc");
		ppc->result = CHECK_RESULT_INTERNAL_ERROR;
		CHECK_ERR(ppc, "cannot allocate memory for arena");
		goto error_cleanup;
	}

	/* read the BTT Info header at well known offset */
	if (pool_read(ppc->pool, &loc->arenap->btt_info,
			sizeof(loc->arenap->btt_info), loc->offset)) {
		CHECK_ERR(ppc, "arena %u: cannot read BTT Info header",
			loc->arenap->id);
		ppc->result = CHECK_RESULT_ERROR;
		goto error_cleanup;
	}

	loc->arenap->id = ppc->pool->narenas;

	/* BLK is consistent even without BTT Layout */
	if (ppc->pool->params.type == POOL_TYPE_BLK) {
		int is_zeroed = util_is_zeroed((const void *)
			&loc->arenap->btt_info, sizeof(loc->arenap->btt_info));
		if (is_zeroed) {
			CHECK_INFO(ppc, "BTT Layout not written");
			loc->step = CHECK_STEP_COMPLETE;
			ppc->pool->blk_no_layout = 1;
			location_release(loc);
			check_end(ppc->data);
			return 0;
		}
	}

	/* check consistency of BTT Info */
	if (pool_btt_info_valid(&loc->arenap->btt_info)) {
		CHECK_INFO(ppc, "arena %u: BTT Info header checksum correct",
			loc->arenap->id);
		loc->valid.btti_header = 1;
	} else if (CHECK_IS_NOT(ppc, REPAIR)) {
		CHECK_ERR(ppc, "arena %u: BTT Info header checksum incorrect",
			loc->arenap->id);
		ppc->result = CHECK_RESULT_NOT_CONSISTENT;
		check_end(ppc->data);
		goto error_cleanup;
	}

	return 0;

error_cleanup:
	location_release(loc);
	return -1;
}

/*
 * btt_info_backup -- (internal) check BTT Info backup
 */
static int
btt_info_backup(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	/* check BTT Info backup consistency */
	const size_t btt_info_size = sizeof(ppc->pool->bttc.btt_info);
	uint64_t btt_info_off = pool_next_arena_offset(ppc->pool, loc->offset) -
		btt_info_size;

	if (pool_read(ppc->pool, &ppc->pool->bttc.btt_info, btt_info_size,
			btt_info_off)) {
		CHECK_ERR(ppc, "arena %u: cannot read BTT Info backup",
			loc->arenap->id);
		goto error;
	}

	/* check whether this BTT Info backup is valid */
	if (pool_btt_info_valid(&ppc->pool->bttc.btt_info)) {
		loc->valid.btti_backup = 1;

		/* restore BTT Info from backup */
		if (!loc->valid.btti_header && CHECK_IS(ppc, REPAIR))
			CHECK_ASK(ppc, Q_RESTORE_FROM_BACKUP, "arena %u: BTT "
				"Info header checksum incorrect.|Restore BTT "
				"Info from backup?", loc->arenap->id);
	}

	/*
	 * if BTT Info backup require repairs it will be fixed in further steps
	 */

	return check_questions_sequence_validate(ppc);

error:
	ppc->result = CHECK_RESULT_ERROR;
	location_release(loc);
	return -1;
}

/*
 * btt_info_from_backup_fix -- (internal) fix BTT Info using its backup
 */
static int
btt_info_from_backup_fix(PMEMpoolcheck *ppc, location *loc, uint32_t question,
	void *ctx)
{
	LOG(3, NULL);

	ASSERTeq(ctx, NULL);
	ASSERTne(loc, NULL);

	switch (question) {
	case Q_RESTORE_FROM_BACKUP:
		CHECK_INFO(ppc,
			"arena %u: restoring BTT Info header from backup",
			loc->arenap->id);

		memcpy(&loc->arenap->btt_info, &ppc->pool->bttc.btt_info,
			sizeof(loc->arenap->btt_info));
		loc->valid.btti_header = 1;
		break;
	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

/*
 * btt_info_gen -- (internal) ask whether try to regenerate BTT Info
 */
static int
btt_info_gen(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	if (loc->valid.btti_header)
		return 0;

	ASSERT(CHECK_IS(ppc, REPAIR));

	if (!loc->pool_valid.btti_offset) {
		ppc->result = CHECK_RESULT_NOT_CONSISTENT;
		check_end(ppc->data);
		return CHECK_ERR(ppc, "can not find any valid BTT Info");
	}

	CHECK_ASK(ppc, Q_REGENERATE,
		"arena %u: BTT Info header checksum incorrect.|Do you want to "
		"regenerate BTT Info?", loc->arenap->id);

	return check_questions_sequence_validate(ppc);
}

/*
 * btt_info_gen_fix -- (internal) fix by regenerating BTT Info
 */
static int
btt_info_gen_fix(PMEMpoolcheck *ppc, location *loc, uint32_t question,
	void *ctx)
{
	LOG(3, NULL);

	ASSERTeq(ctx, NULL);
	ASSERTne(loc, NULL);

	switch (question) {
	case Q_REGENERATE:
		CHECK_INFO(ppc, "arena %u: regenerating BTT Info header",
			loc->arenap->id);

		/*
		 * We do not have valid BTT Info backup so we get first valid
		 * BTT Info and try to calculate BTT Info for current arena
		 */
		uint64_t arena_size = ppc->pool->set_file->size - loc->offset;
		if (arena_size > BTT_MAX_ARENA)
			arena_size = BTT_MAX_ARENA;

		uint64_t space_left = ppc->pool->set_file->size - loc->offset -
			arena_size;

		struct btt_info *bttd = &loc->arenap->btt_info;
		struct btt_info *btts = &loc->pool_valid.btti;

		btt_info_convert2h(bttd);

		/*
		 * all valid BTT Info structures have the same signature, UUID,
		 * parent UUID, flags, major, minor, external LBA size, internal
		 * LBA size, nfree, info size and data offset
		 */
		memcpy(bttd->sig, btts->sig, BTTINFO_SIG_LEN);
		memcpy(bttd->uuid, btts->uuid, BTTINFO_UUID_LEN);
		memcpy(bttd->parent_uuid, btts->parent_uuid, BTTINFO_UUID_LEN);
		memset(bttd->unused, 0, BTTINFO_UNUSED_LEN);
		bttd->flags = btts->flags;
		bttd->major = btts->major;
		bttd->minor = btts->minor;

		/* other parameters can be calculated */
		if (btt_info_set(bttd, btts->external_lbasize, btts->nfree,
				arena_size, space_left)) {
			CHECK_ERR(ppc, "can not restore BTT Info");
			return -1;
		}

		ASSERTeq(bttd->external_lbasize, btts->external_lbasize);
		ASSERTeq(bttd->internal_lbasize, btts->internal_lbasize);
		ASSERTeq(bttd->nfree, btts->nfree);
		ASSERTeq(bttd->infosize, btts->infosize);
		ASSERTeq(bttd->dataoff, btts->dataoff);
		return 0;

	default:
		ERR("not implemented question id: %u", question);
		return -1;
	}
}

/*
 * btt_info_checksum_retry -- (internal) check BTT Info checksum
 */
static int
btt_info_checksum_retry(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	if (loc->valid.btti_header)
		return 0;

	btt_info_convert2le(&loc->arenap->btt_info);

	/* check consistency of BTT Info */
	if (pool_btt_info_valid(&loc->arenap->btt_info)) {
		CHECK_INFO(ppc, "arena %u: BTT Info header checksum correct",
			loc->arenap->id);
		loc->valid.btti_header = 1;
		return 0;
	}

	if (CHECK_IS_NOT(ppc, ADVANCED)) {
		ppc->result = CHECK_RESULT_CANNOT_REPAIR;
		CHECK_INFO(ppc, REQUIRE_ADVANCED);
		CHECK_ERR(ppc, "arena %u: BTT Info header checksum incorrect",
			loc->arenap->id);
		check_end(ppc->data);
		goto error_cleanup;
	}

	CHECK_ASK(ppc, Q_REGENERATE_CHECKSUM,
		"arena %u: BTT Info header checksum incorrect.|Do you want to "
		"regenerate BTT Info checksum?", loc->arenap->id);

	return check_questions_sequence_validate(ppc);

error_cleanup:
	location_release(loc);
	return -1;
}

/*
 * btt_info_checksum_fix -- (internal) fix by regenerating BTT Info checksum
 */
static int
btt_info_checksum_fix(PMEMpoolcheck *ppc, location *loc, uint32_t question,
	void *ctx)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(ppc);

	LOG(3, NULL);

	ASSERTeq(ctx, NULL);
	ASSERTne(loc, NULL);

	switch (question) {
	case Q_REGENERATE_CHECKSUM:
		util_checksum(&loc->arenap->btt_info, sizeof(struct btt_info),
			&loc->arenap->btt_info.checksum, 1, 0);
		loc->valid.btti_header = 1;
		break;

	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

/*
 * btt_info_backup_checksum -- (internal) check BTT Info backup checksum
 */
static int
btt_info_backup_checksum(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	ASSERT(loc->valid.btti_header);

	if (loc->valid.btti_backup)
		return 0;

	/* BTT Info backup is not valid so it must be fixed */
	if (CHECK_IS_NOT(ppc, REPAIR)) {
		CHECK_ERR(ppc,
			"arena %u: BTT Info backup checksum incorrect",
			loc->arenap->id);
		ppc->result = CHECK_RESULT_NOT_CONSISTENT;
		check_end(ppc->data);
		goto error_cleanup;
	}

	CHECK_ASK(ppc, Q_RESTORE_FROM_HEADER,
		"arena %u: BTT Info backup checksum incorrect.|Do you want to "
		"restore it from BTT Info header?", loc->arenap->id);

	return check_questions_sequence_validate(ppc);

error_cleanup:
	location_release(loc);
	return -1;
}

/*
 * btt_info_backup_fix -- (internal) prepare restore BTT Info backup from header
 */
static int
btt_info_backup_fix(PMEMpoolcheck *ppc, location *loc, uint32_t question,
	void *ctx)
{
	LOG(3, NULL);

	ASSERTeq(ctx, NULL);
	ASSERTne(loc, NULL);

	switch (question) {
	case Q_RESTORE_FROM_HEADER:
		/* BTT Info backup would be restored in check_write step */
		CHECK_INFO(ppc,
			"arena %u: restoring BTT Info backup from header",
			loc->arenap->id);
		break;

	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

struct step {
	int (*check)(PMEMpoolcheck *, location *);
	int (*fix)(PMEMpoolcheck *, location *, uint32_t, void *);
};

static const struct step steps[] = {
	{
		.check		= btt_info_checksum,
	},
	{
		.check		= btt_info_backup,
	},
	{
		.fix		= btt_info_from_backup_fix,
	},
	{
		.check		= btt_info_gen,
	},
	{
		.fix		= btt_info_gen_fix,
	},
	{
		.check		= btt_info_checksum_retry,
	},
	{
		.fix		= btt_info_checksum_fix,
	},
	{
		.check		= btt_info_backup_checksum,
	},
	{
		.fix		= btt_info_backup_fix,
	},
	{
		.check		= NULL,
		.fix		= NULL,
	},
};

/*
 * step_exe -- (internal) perform single step according to its parameters
 */
static inline int
step_exe(PMEMpoolcheck *ppc, location *loc)
{
	ASSERT(loc->step < ARRAY_SIZE(steps));

	const struct step *step = &steps[loc->step++];

	if (!step->fix)
		return step->check(ppc, loc);

	if (!check_answer_loop(ppc, loc, NULL, 1, step->fix))
		return 0;

	if (check_has_error(ppc->data))
		location_release(loc);

	return -1;
}

/*
 * check_btt_info -- entry point for btt info check
 */
void
check_btt_info(PMEMpoolcheck *ppc)
{
	LOG(3, NULL);

	location *loc = check_get_step_data(ppc->data);
	uint64_t nextoff = 0;

	/* initialize check */
	if (!loc->offset) {
		CHECK_INFO(ppc, "checking BTT Info headers");
		loc->offset = sizeof(struct pool_hdr);
		if (ppc->pool->params.type == POOL_TYPE_BLK)
			loc->offset += ALIGN_UP(sizeof(struct pmemblk) -
					sizeof(struct pool_hdr),
					BLK_FORMAT_DATA_ALIGN);

		loc->pool_valid.btti_offset = pool_get_first_valid_btt(
			ppc->pool, &loc->pool_valid.btti, loc->offset, NULL);

		/* Without valid BTT Info we can not proceed */
		if (!loc->pool_valid.btti_offset) {
			if (ppc->pool->params.type == POOL_TYPE_BTT) {
				CHECK_ERR(ppc,
					"can not find any valid BTT Info");
				ppc->result = CHECK_RESULT_NOT_CONSISTENT;
				check_end(ppc->data);
				return;
			}
		} else
			btt_info_convert2h(&loc->pool_valid.btti);
	}

	do {
		/* jump to next offset */
		if (ppc->result != CHECK_RESULT_PROCESS_ANSWERS) {
			loc->offset += nextoff;
			loc->step = 0;
			loc->valid.btti_header = 0;
			loc->valid.btti_backup = 0;
		}

		/* do all checks */
		while (CHECK_NOT_COMPLETE(loc, steps)) {
			if (step_exe(ppc, loc) || ppc->pool->blk_no_layout == 1)
				return;
		}

		/* save offset and insert BTT to cache for next steps */
		loc->arenap->offset = loc->offset;
		loc->arenap->valid = true;
		check_insert_arena(ppc, loc->arenap);
		nextoff = le64toh(loc->arenap->btt_info.nextoff);

	} while (nextoff > 0);
}
