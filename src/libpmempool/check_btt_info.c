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
 * check_btt_info.c -- check BTT Info
 */

#include <stdlib.h>
#include <stdint.h>

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
		struct arena *arena;
		uint64_t offset;
		struct {
			int btti_header;
			int btti_backup;
		} valid;
		struct {
			struct btt_info btti;
			uint64_t btti_offset;
		} pool_valid;
		unsigned step;
	};
	/* global check step data */
	struct check_step_data step_data;
};

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
location_release(union location *loc)
{
	free(loc->arena);
	loc->arena = NULL;
}

/*
 * btt_info_checksum -- (internal) check BTT Info checksum
 */
static int
btt_info_checksum(PMEMpoolcheck *ppc, union location *loc)
{
	LOG(3, NULL);

	loc->arena = calloc(1, sizeof(struct arena));
	if (!loc->arena) {
		ERR("!calloc");
		ppc->result = CHECK_RESULT_INTERNAL_ERROR;
		CHECK_ERR(ppc, "cannot allocate memory for arena");
		goto error_cleanup;
	}

	/* read the BTT Info header at well known offset */
	if (pool_read(ppc->pool, &loc->arena->btt_info,
			sizeof(loc->arena->btt_info), loc->offset)) {
		CHECK_ERR(ppc, "arena %u: cannot read BTT Info header",
			loc->arena->id);
		ppc->result = CHECK_RESULT_ERROR;
		goto error_cleanup;
	}

	loc->arena->id = ppc->pool->narenas;

	/* BLK is consistent even without BTT Layout */
	if (ppc->pool->params.type == POOL_TYPE_BLK) {
		int is_zeroed = util_is_zeroed((const void *)
			&loc->arena->btt_info, sizeof(loc->arena->btt_info));
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
	if (pool_btt_info_valid(&loc->arena->btt_info)) {
		CHECK_INFO(ppc, "arena %u: BTT Info header checksum correct",
			loc->arena->id);
		loc->valid.btti_header = 1;
	} else if (CHECK_IS_NOT(ppc, REPAIR)) {
		CHECK_ERR(ppc, "arena %u: BTT Info header checksum incorrect",
			loc->arena->id);
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
btt_info_backup(PMEMpoolcheck *ppc, union location *loc)
{
	LOG(3, NULL);

	/* check BTT Info backup consistency */
	const size_t btt_info_size = sizeof(ppc->pool->bttc.btt_info);
	uint64_t btt_info_off = pool_next_arena_offset(ppc->pool, loc->offset) -
		btt_info_size;

	if (pool_read(ppc->pool, &ppc->pool->bttc.btt_info, btt_info_size,
			btt_info_off)) {
		CHECK_ERR(ppc, "arena %u: cannot read BTT Info backup",
			loc->arena->id);
		goto error;
	}

	/* check whether this BTT Info backup is valid */
	if (pool_btt_info_valid(&ppc->pool->bttc.btt_info)) {
		loc->valid.btti_backup = 1;

		/* restore BTT Info from backup */
		if (!loc->valid.btti_header && CHECK_IS(ppc, REPAIR))
			CHECK_ASK(ppc, Q_RESTORE_FROM_BACKUP, "arena %u: BTT "
				"Info header checksum incorrect.|Restore BTT "
				"Info from backup?", loc->arena->id);
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
btt_info_from_backup_fix(PMEMpoolcheck *ppc, struct check_step_data *location,
	uint32_t question, void *ctx)
{
	LOG(3, NULL);

	ASSERTeq(ctx, NULL);
	ASSERTne(location, NULL);
	union location *loc = (union location *)location;

	switch (question) {
	case Q_RESTORE_FROM_BACKUP:
		CHECK_INFO(ppc,
			"arena %u: restoring BTT Info header from backup",
			loc->arena->id);

		memcpy(&loc->arena->btt_info, &ppc->pool->bttc.btt_info,
			sizeof(loc->arena->btt_info));
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
btt_info_gen(PMEMpoolcheck *ppc, union location *loc)
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
		"regenerate BTT Info?", loc->arena->id);

	return check_questions_sequence_validate(ppc);
}

/*
 * btt_info_gen_fix -- (internal) fix by regenerating BTT Info
 */
static int
btt_info_gen_fix(PMEMpoolcheck *ppc, struct check_step_data *location,
	uint32_t question, void *ctx)
{
	LOG(3, NULL);

	ASSERTeq(ctx, NULL);
	ASSERTne(location, NULL);
	union location *loc = (union location *)location;

	switch (question) {
	case Q_REGENERATE:
		CHECK_INFO(ppc, "arena %u: regenerating BTT Info header",
			loc->arena->id);

		/*
		 * We do not have valid BTT Info backup so we get first valid
		 * BTT Info and try to calculate BTT Info for current arena
		 */
		uint64_t arena_size = ppc->pool->set_file->size - loc->offset;
		if (arena_size > BTT_MAX_ARENA)
			arena_size = BTT_MAX_ARENA;

		uint64_t space_left = ppc->pool->set_file->size - loc->offset -
			arena_size;

		struct btt_info *bttd = &loc->arena->btt_info;
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
			CHECK_ERR(ppc, "Can not restore BTT Info");
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
btt_info_checksum_retry(PMEMpoolcheck *ppc, union location *loc)
{
	LOG(3, NULL);

	if (loc->valid.btti_header)
		return 0;

	btt_info_convert2le(&loc->arena->btt_info);

	/* check consistency of BTT Info */
	if (pool_btt_info_valid(&loc->arena->btt_info)) {
		CHECK_INFO(ppc, "arena %u: BTT Info header checksum correct",
			loc->arena->id);
		loc->valid.btti_header = 1;
		return 0;
	}

	if (CHECK_IS_NOT(ppc, ADVANCED)) {
		CHECK_ERR(ppc, "arena %u: BTT Info header checksum incorrect",
			loc->arena->id);
		ppc->result = CHECK_RESULT_NOT_CONSISTENT;
		check_end(ppc->data);
		goto error_cleanup;
	}

	CHECK_ASK(ppc, Q_REGENERATE_CHECKSUM,
		"arena %u: BTT Info header checksum incorrect.|Do you want to "
		"regenerate BTT Info checksum?", loc->arena->id);

	return check_questions_sequence_validate(ppc);

error_cleanup:
	location_release(loc);
	return -1;
}

/*
 * btt_info_checksum_fix -- (internal) fix by regenerating BTT Info checksum
 */
static int
btt_info_checksum_fix(PMEMpoolcheck *ppc, struct check_step_data *location,
	uint32_t question, void *ctx)
{
	LOG(3, NULL);

	ASSERTeq(ctx, NULL);
	ASSERTne(location, NULL);
	union location *loc = (union location *)location;

	switch (question) {
	case Q_REGENERATE_CHECKSUM:
		util_checksum(&loc->arena->btt_info, sizeof(struct btt_info),
			&loc->arena->btt_info.checksum, 1);
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
btt_info_backup_checksum(PMEMpoolcheck *ppc, union location *loc)
{
	LOG(3, NULL);

	ASSERT(loc->valid.btti_header);

	if (loc->valid.btti_backup)
		return 0;

	/* BTT Info backup is not valid so it must be fixed */
	if (CHECK_IS_NOT(ppc, REPAIR)) {
		CHECK_ERR(ppc,
			"arena %u: BTT Info backup checksum incorrect",
			loc->arena->id);
		ppc->result = CHECK_RESULT_NOT_CONSISTENT;
		check_end(ppc->data);
		goto error_cleanup;
	}

	CHECK_ASK(ppc, Q_RESTORE_FROM_HEADER,
		"arena %u: BTT Info backup checksum incorrect.|Do you want to "
		"restore it from BTT Info header?", loc->arena->id);

	return check_questions_sequence_validate(ppc);

error_cleanup:
	location_release(loc);
	return -1;
}

/*
 * btt_info_backup_fix -- (internal) prepare restore BTT Info backup from header
 */
static int
btt_info_backup_fix(PMEMpoolcheck *ppc, struct check_step_data *location,
	uint32_t question, void *ctx)
{
	LOG(3, NULL);

	ASSERTeq(ctx, NULL);
	ASSERTne(location, NULL);
	union location *loc = (union location *)location;

	switch (question) {
	case Q_RESTORE_FROM_HEADER:
		/* BTT Info backup would be restored in check_write step */
		CHECK_INFO(ppc,
			"arena %u: restoring BTT Info backup from header",
			loc->arena->id);
		break;

	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

struct step {
	int (*check)(PMEMpoolcheck *, union location *);
	int (*fix)(PMEMpoolcheck *, struct check_step_data *, uint32_t, void *);
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
	},
};

/*
 * step_exe -- (internal) perform single step according to its parameters
 */
static inline int
step_exe(PMEMpoolcheck *ppc, union location *loc)
{
	const struct step *step = &steps[loc->step++];

	if (!step->fix)
		return step->check(ppc, loc);

	if (!check_answer_loop(ppc, &loc->step_data, NULL, step->fix))
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

	COMPILE_ERROR_ON(sizeof(union location) !=
		sizeof(struct check_step_data));

	union location *loc = (union location *)check_get_step_data(ppc->data);
	uint64_t nextoff = 0;

	/* initialize check */
	if (!loc->offset) {
		CHECK_INFO(ppc, "checking BTT Info headers");
		loc->offset = BTT_ALIGNMENT;
		if (ppc->pool->params.type == POOL_TYPE_BLK)
			loc->offset += BTT_ALIGNMENT;

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
			nextoff = 0;
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
		loc->arena->offset = loc->offset;
		loc->arena->valid = true;
		check_insert_arena(ppc, loc->arena);
		nextoff = le64toh(loc->arena->btt_info.nextoff);

	} while (nextoff > 0);
}
