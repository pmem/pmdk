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
 * check_pool_hdr.c -- pool header check
 */

#include <stdio.h>
#include <inttypes.h>
#include <sys/mman.h>

#include "out.h"
#include "libpmempool.h"
#include "pmempool.h"
#include "pool.h"
#include "check_util.h"

#define PREFIX_MAX_SIZE 30

#define POOL_HDR_UUIDS_NUM 5

/* assure size match between global and internal check step data */
union location {
	/* internal check step data */
	struct {
		unsigned replica;
		unsigned part;
		unsigned step;
		char prefix[PREFIX_MAX_SIZE];
		int header_modified;
	};
	/* global check step data */
	struct check_step_data step_data;
};

struct context {
	struct pool_hdr hdr;
	struct pool_hdr *hdrp;
	struct pool_hdr def_hdr;
	struct pool_hdr *next_part_hdrp;
	struct pool_hdr *prev_part_hdrp;
	struct pool_hdr *next_repl_hdrp;
	struct pool_hdr *prev_repl_hdrp;
};

enum question {
	Q_DEFAULT_SIGNATURE,
	Q_DEFAULT_MAJOR,
	Q_DEFAULT_COMPAT_FEATURES,
	Q_DEFAULT_INCOMPAT_FEATURES,
	Q_DEFAULT_RO_COMPAT_FEATURES,
	Q_ZERO_UNUSED_AREA,
	Q_CRTIME,
	Q_CHECKSUM,
	Q_BLK_UUID_FROM_BTT_INFO,
	Q_UUID_FROM_VALID_PART,
	Q_REGENERATE_UUIDS,
	Q_SET_VALID_UUID,
	Q_SET_NEXT_PART_UUID,
	Q_SET_PREV_PART_UUID,
	Q_SET_NEXT_REPL_UUID,
	Q_SET_PREV_REPL_UUID
};

enum num_of_parts_and_replicas {
	NUM_OF_PAR_ANY,
	NUM_OF_PAR_SINGLE,
	NUM_OF_PAR_MANY
};

/*
 * pool_hdr_possible_type -- (internal) return possible type of pool
 */
static enum pool_type
pool_hdr_possible_type(PMEMpoolcheck *ppc)
{
	if (pool_blk_get_first_valid_arena(ppc->pool, &ppc->pool->bttc))
		return POOL_TYPE_BLK;

	return POOL_TYPE_UNKNOWN;
}

/*
 * pool_hdr_valid -- (internal) return true if pool header is valid
 */
static int
pool_hdr_valid(struct pool_hdr *hdrp)
{
	return !util_is_zeroed((void *)hdrp, sizeof(*hdrp)) &&
		util_checksum(hdrp, sizeof(*hdrp), &hdrp->checksum, 0);
}

/*
 * pool_supported -- (internal) check if pool type is supported
 */
static int
pool_supported(enum pool_type type)
{
	switch (type) {
	case POOL_TYPE_LOG:
		return 1;
	case POOL_TYPE_BLK:
		return 1;
	case POOL_TYPE_OBJ:
	default:
		return 0;
	}
}

/*
 * pool_hdr_get -- (internal) get pool header from given location
 */
static void
pool_hdr_get(PMEMpoolcheck *ppc, struct pool_hdr *hdr, struct pool_hdr **shdr,
	union location *loc)
{
	struct pool_replica *rep =
		ppc->pool->set_file->poolset->replica[loc->replica];
	struct pool_hdr *hdrp = rep->part[loc->part].hdr;
	memcpy(hdr, hdrp, sizeof(*hdr));

	if (shdr != NULL)
		*shdr = hdrp;
}

/*
 * pool_hdr_checksum -- (internal) check pool header by checksum
 */
static int
pool_hdr_checksum(PMEMpoolcheck *ppc, union location *loc)
{
	LOG(3, NULL);

	CHECK_INFO(ppc, "%schecking pool header", loc->prefix);
	struct pool_hdr hdr;
	pool_hdr_get(ppc, &hdr, NULL, loc);

	int cs_valid = pool_hdr_valid(&hdr);

	if (util_is_zeroed((void *)&hdr, sizeof(hdr))) {
		if (CHECK_IS_NOT(ppc, REPAIR)) {
			check_end(ppc->data);
			ppc->result = CHECK_RESULT_NOT_CONSISTENT;
			return CHECK_ERR(ppc, "empty pool hdr");
		}
	} else if (cs_valid) {
		enum pool_type type = pool_hdr_get_type(&hdr);
		if (type == POOL_TYPE_UNKNOWN) {
			if (CHECK_IS_NOT(ppc, REPAIR)) {
				check_end(ppc->data);
				ppc->result = CHECK_RESULT_NOT_CONSISTENT;
				return CHECK_ERR(ppc, "invalid signature");
			}

			CHECK_INFO(ppc, "invalid signature");
		} else {
			/* valid check sum */
			CHECK_INFO(ppc, "%spool header checksum correct",
				loc->prefix);
			loc->step = CHECK_STEP_COMPLETE;
			return 0;
		}
	} else if (CHECK_IS_NOT(ppc, REPAIR)) {
		check_end(ppc->data);
		ppc->result = CHECK_RESULT_NOT_CONSISTENT;
		return CHECK_ERR(ppc, "%sincorrect pool header checksum",
			loc->prefix);
	} else {
		CHECK_INFO(ppc, "%sincorrect pool header checksum",
			loc->prefix);
	}

	ASSERT(CHECK_IS(ppc, REPAIR));

	if (ppc->pool->params.type == POOL_TYPE_UNKNOWN) {
		ppc->pool->params.type = pool_hdr_possible_type(ppc);
		if (ppc->pool->params.type == POOL_TYPE_UNKNOWN) {
			ppc->result = CHECK_RESULT_CANNOT_REPAIR;
			return CHECK_ERR(ppc, "cannot determine pool type");
		}
	}

	if (!pool_supported(ppc->pool->params.type)) {
		ppc->result = CHECK_RESULT_CANNOT_REPAIR;
		return CHECK_ERR(ppc, "unsupported pool type '%s'",
			check_get_pool_type_str(ppc->pool->params.type));
	}

	return 0;
}

/*
 * pool_hdr_default_check -- (internal) check some default values in pool header
 */
static int
pool_hdr_default_check(PMEMpoolcheck *ppc, union location *loc)
{
	LOG(3, NULL);

	ASSERT(CHECK_IS(ppc, REPAIR));

	struct pool_hdr hdr;
	pool_hdr_get(ppc, &hdr, NULL, loc);
	util_convert2h_hdr_nocheck(&hdr);

	struct pool_hdr def_hdr;
	pool_hdr_default(ppc->pool->params.type, &def_hdr);

	if (memcmp(hdr.signature, def_hdr.signature, POOL_HDR_SIG_LEN)) {
		CHECK_ASK(ppc, Q_DEFAULT_SIGNATURE,
			"%spool_hdr.signature is not valid.|Do you want to set "
			"it to %.8s?", loc->prefix, def_hdr.signature);
	}

	if (hdr.major != def_hdr.major) {
		CHECK_ASK(ppc, Q_DEFAULT_MAJOR,
			"%spool_hdr.major is not valid.|Do you want to set it "
			"to default value 0x%x?", loc->prefix, def_hdr.major);
	}

	if (hdr.compat_features != def_hdr.compat_features) {
		CHECK_ASK(ppc, Q_DEFAULT_COMPAT_FEATURES,
			"%spool_hdr.compat_features is not valid.|Do you want "
			"to set it to default value 0x%x?", loc->prefix,
			def_hdr.compat_features);
	}

	if (hdr.incompat_features != def_hdr.incompat_features) {
		CHECK_ASK(ppc, Q_DEFAULT_INCOMPAT_FEATURES,
			"%spool_hdr.incompat_features is not valid.|Do you "
			"want to set it to default value 0x%x?", loc->prefix,
			def_hdr.incompat_features);
	}

	if (hdr.ro_compat_features != def_hdr.ro_compat_features) {
		CHECK_ASK(ppc, Q_DEFAULT_RO_COMPAT_FEATURES,
			"%spool_hdr.ro_compat_features is not valid.|Do you "
			"want to set it to default value 0x%x?", loc->prefix,
			def_hdr.ro_compat_features);
	}

	if (!util_is_zeroed(hdr.unused, sizeof(hdr.unused))) {
		CHECK_ASK(ppc, Q_ZERO_UNUSED_AREA,
			"%sunused area is not filled by zeros.|Do you want to "
			"fill it up?", loc->prefix);
	}

	return check_questions_sequence_validate(ppc);
}

/*
 * pool_hdr_default_fix -- (internal) fix some default values in pool header
 */
static int
pool_hdr_default_fix(PMEMpoolcheck *ppc, struct check_step_data *location,
	uint32_t question, void *context)
{
	LOG(3, NULL);

	ASSERT(context != NULL);
	struct context *ctx = (struct context *)context;

	switch (question) {
	case Q_DEFAULT_SIGNATURE:
		CHECK_INFO(ppc, "setting pool_hdr.signature to %.8s",
			ctx->def_hdr.signature);
		memcpy(ctx->hdr.signature, ctx->def_hdr.signature,
			POOL_HDR_SIG_LEN);
		break;
	case Q_DEFAULT_MAJOR:
		CHECK_INFO(ppc, "setting pool_hdr.major to 0x%x",
			ctx->def_hdr.major);
		ctx->hdr.major = ctx->def_hdr.major;
		break;
	case Q_DEFAULT_COMPAT_FEATURES:
		CHECK_INFO(ppc, "setting pool_hdr.compat_features to 0x%x",
			ctx->def_hdr.compat_features);
		ctx->hdr.compat_features = ctx->def_hdr.compat_features;
		break;
	case Q_DEFAULT_INCOMPAT_FEATURES:
		CHECK_INFO(ppc, "setting pool_hdr.incompat_features to 0x%x",
			ctx->def_hdr.incompat_features);
		ctx->hdr.incompat_features = ctx->def_hdr.incompat_features;
		break;
	case Q_DEFAULT_RO_COMPAT_FEATURES:
		CHECK_INFO(ppc, "setting pool_hdr.ro_compat_features to 0x%x",
			ctx->def_hdr.ro_compat_features);
		ctx->hdr.ro_compat_features = ctx->def_hdr.ro_compat_features;
		break;
	case Q_ZERO_UNUSED_AREA:
		CHECK_INFO(ppc, "setting pool_hdr.unused to zeros");
		memset(ctx->hdr.unused, 0, sizeof(ctx->hdr.unused));
		break;
	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

/*
 * pool_get_valid_part -- (internal) returns valid part replica and part ids
 *
 * Assume part of replica indicated by rid and pid as invalid.
 */
static struct pool_set_part *
pool_get_valid_part(PMEMpoolcheck *ppc, unsigned rid, unsigned pid,
	unsigned *ridp, unsigned *pidp)
{
	const struct pool_set *poolset = ppc->pool->set_file->poolset;
	for (unsigned r = 0; r < poolset->nreplicas; r++) {
		struct pool_replica *rep = poolset->replica[r];
		for (unsigned p = 0; p < rep->nparts; p++) {
			/* skip part of replica known as invalid */
			if (r == rid && p == pid)
				continue;

			if (pool_hdr_valid(rep->part[p].hdr)) {
				*ridp = r;
				*pidp = p;
				return &rep->part[p];
			}
		}
	}

	return NULL;
}

/*
 * pool_hdr_poolset_uuid -- (internal) check poolset_uuid field
 */
static int
pool_hdr_poolset_uuid(PMEMpoolcheck *ppc, union location *loc)
{
	LOG(3, NULL);

	struct pool_hdr hdr;
	pool_hdr_get(ppc, &hdr, NULL, loc);
	util_convert2h_hdr_nocheck(&hdr);

	/* for blk pool we can take the UUID from BTT Info header */
	if (ppc->pool->params.type == POOL_TYPE_BLK &&
		ppc->pool->bttc.valid) {
		if (memcmp(hdr.poolset_uuid,
				ppc->pool->bttc.btt_info.parent_uuid,
				POOL_HDR_UUID_LEN) == 0) {
			return 0;
		}

		CHECK_ASK(ppc, Q_BLK_UUID_FROM_BTT_INFO,
			"%sinvalid pool_hdr.poolset_uuid.|Do you want to set "
			"it to %s from BTT Info?", loc->prefix,
			check_get_uuid_str(
			ppc->pool->bttc.btt_info.parent_uuid));
	} else if (ppc->pool->params.is_poolset) {
		unsigned rid = 0;
		unsigned pid = 0;
		const struct pool_set_part *valid_part;
		if ((valid_part = pool_get_valid_part(ppc, loc->replica,
				loc->part, &rid, &pid)) == NULL) {
			ppc->result = CHECK_RESULT_CANNOT_REPAIR;
			return CHECK_ERR(ppc, "Can not repair "
				"pool_hdr.poolset_uuid");
		}
		struct pool_hdr *valid_hdrp = valid_part->hdr;
		if (memcmp(hdr.poolset_uuid, valid_hdrp->poolset_uuid,
				POOL_HDR_UUID_LEN) == 0)
			return 0;
		CHECK_ASK(ppc, Q_UUID_FROM_VALID_PART,
			"%sinvalid pool_hdr.poolset_uuid.|Do you want to set "
			"it to %s from valid pool file part ?", loc->prefix,
			check_get_uuid_str(valid_hdrp->poolset_uuid));
	}

	return check_questions_sequence_validate(ppc);
}

/*
 * pool_hdr_poolset_uuid_fix -- (internal) fix poolset_uuid field
 */
static int
pool_hdr_poolset_uuid_fix(PMEMpoolcheck *ppc, struct check_step_data *location,
	uint32_t question, void *context)
{
	LOG(3, NULL);

	ASSERT(context != NULL);
	union location *loc = (union location *)location;
	struct context *ctx = (struct context *)context;

	unsigned rid = 0;
	unsigned pid = 0;
	const struct pool_set_part *valid_part;
	switch (question) {
	case Q_BLK_UUID_FROM_BTT_INFO:
		CHECK_INFO(ppc, "%ssetting pool_hdr.poolset_uuid to %s",
			loc->prefix, check_get_uuid_str(
			ppc->pool->bttc.btt_info.parent_uuid));
		memcpy(ctx->hdr.poolset_uuid,
			ppc->pool->bttc.btt_info.parent_uuid,
			POOL_HDR_UUID_LEN);
		ppc->pool->uuid_op = UUID_FROM_BTT;
		break;
	case Q_UUID_FROM_VALID_PART:
		if ((valid_part = pool_get_valid_part(ppc, loc->replica,
				loc->part, &rid, &pid)) == NULL) {
			ppc->result = CHECK_RESULT_CANNOT_REPAIR;
			return CHECK_ERR(ppc,
				"Can not repair pool_hdr.poolset_uuid");
		}
		struct pool_hdr *valid_hdrp = valid_part->hdr;
		CHECK_INFO(ppc, "%ssetting pool_hdr.poolset_uuid to %s",
			loc->prefix,
			check_get_uuid_str(valid_hdrp->poolset_uuid));
		memcpy(ctx->hdr.poolset_uuid, valid_hdrp->poolset_uuid,
			POOL_HDR_UUID_LEN);
		break;
	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

/*
 * pool_hdr_checksum_retry -- (internal) check if checksum match after all
 *	performed fixes
 */
static int
pool_hdr_checksum_retry(PMEMpoolcheck *ppc, union location *loc)
{
	LOG(3, NULL);

	struct pool_hdr hdr;
	pool_hdr_get(ppc, &hdr, NULL, loc);

	if (pool_hdr_valid(&hdr))
		loc->step = CHECK_STEP_COMPLETE;

	return 0;
}

/*
 * pool_hdr_gen -- (internal) generate pool hdr values
 */
static int
pool_hdr_gen(PMEMpoolcheck *ppc, union location *loc)
{
	LOG(3, NULL);

	struct pool_hdr hdr;
	pool_hdr_get(ppc, &hdr, NULL, loc);
	util_convert2h_hdr_nocheck(&hdr);

	if (hdr.crtime > (uint64_t)ppc->pool->set_file->mtime) {
		CHECK_ASK(ppc, Q_CRTIME,
			"%spool_hdr.crtime is not valid.|Do you want to set it "
			"to file's modtime [%s]?", loc->prefix,
			check_get_time_str(ppc->pool->set_file->mtime));
	}

	CHECK_ASK(ppc, Q_CHECKSUM, "Do you want to regenerate checksum?");

	return check_questions_sequence_validate(ppc);
}

/*
 * pool_hdr_gen_fix -- (internal) fix pool hdr values by overwrite with
 *	generated values
 */
static int
pool_hdr_gen_fix(PMEMpoolcheck *ppc, struct check_step_data *location,
	uint32_t question, void *context)
{
	LOG(3, NULL);

	ASSERT(context != NULL);
	struct context *ctx = (struct context *)context;

	switch (question) {
	case Q_CRTIME:
		CHECK_INFO(ppc, "setting pool_hdr.crtime to file's modtime: %s",
			check_get_time_str(ppc->pool->set_file->mtime));
		util_convert2h_hdr_nocheck(&ctx->hdr);
		ctx->hdr.crtime = (uint64_t)ppc->pool->set_file->mtime;
		util_convert2le_hdr(&ctx->hdr);
		break;
	case Q_CHECKSUM:
		util_checksum(&ctx->hdr, sizeof(ctx->hdr), &ctx->hdr.checksum,
			1);
		CHECK_INFO(ppc, "setting pool_hdr.checksum to 0x%jx",
			le64toh(ctx->hdr.checksum));
		break;
	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

/*
 * pool_hdr_all_uuid_same -- (internal) check if all uuids are same and non-zero
 */
static int
pool_hdr_all_uuid_same(unsigned char (*uuids)[POOL_HDR_UUID_LEN])
{
	if (util_is_zeroed(uuids[0], POOL_HDR_UUID_LEN))
		return 0;
	for (int i = 1; i < POOL_HDR_UUIDS_NUM; i++) {
		if (memcmp(uuids[0], uuids[i], POOL_HDR_UUID_LEN))
			return 0;
	}

	return 1;
}

/*
 * uuid_get_max_same -- (internal) find index of most commonly occurring uuid
 *	if it occurs at least two times.
 */
static int
uuid_get_max_same(unsigned char (*uuids)[POOL_HDR_UUID_LEN], int *indexp)
{
	int max = 0;
	for (int i = 0; i < POOL_HDR_UUIDS_NUM; i++) {
		int icount = 0;
		if (util_is_zeroed(uuids[i], POOL_HDR_UUID_LEN))
			continue;
		for (int j = i + 1; j < POOL_HDR_UUIDS_NUM; j++) {
			if (!memcmp(uuids[i], uuids[j], POOL_HDR_UUID_LEN))
				icount++;
		}

		if (icount > max) {
			max = icount;
			*indexp = i;
		}
	}

	if (max)
		return 0;
	return -1;
}

/*
 * pool_hdr_uuids_single -- (internal) check UUID values for a single pool file
 */
static int
pool_hdr_uuids_single(PMEMpoolcheck *ppc, union location *loc)
{
	LOG(3, NULL);

	const struct pool_set *poolset = ppc->pool->set_file->poolset;
	unsigned nparts = poolset->replica[loc->replica]->nparts;
	unsigned nreplicas = poolset->nreplicas;
	if (nreplicas != 1 || nparts != 1)
		return 0;

	struct pool_hdr hdr;
	pool_hdr_get(ppc, &hdr, NULL, loc);
	util_convert2h_hdr_nocheck(&hdr);

	if (!pool_hdr_all_uuid_same(&hdr.uuid)) {
		int index = 0;
		if (uuid_get_max_same(&hdr.uuid, &index)) {
			CHECK_ASK(ppc, Q_REGENERATE_UUIDS,
				"%sUUID values don't match.|Do you want to "
				"regenerate UUIDs?", loc->prefix);
		} else {
			CHECK_ASK(ppc, Q_SET_VALID_UUID,
				"%sUUID values don't match.|Do you want to set "
				"it to valid value?", loc->prefix);
		}
	}

	return check_questions_sequence_validate(ppc);
}

/*
 * pool_hdr_set_all_uuids -- (internal) set all uuids to the specified one
 */
static void
pool_hdr_set_all_uuids(unsigned char (*uuids)[POOL_HDR_UUID_LEN], int index)
{
	for (int i = 0; i < POOL_HDR_UUIDS_NUM; i++) {
		if (i == index)
			continue;
		memcpy(uuids[i], uuids[index], POOL_HDR_UUID_LEN);
	}
}

/*
 * pool_hdr_uuids_single_fix -- (internal) fix UUID values for a single pool
 *	file
 */
static int
pool_hdr_uuids_single_fix(PMEMpoolcheck *ppc, struct check_step_data *location,
	uint32_t question, void *context)
{
	LOG(3, NULL);

	ASSERT(context != NULL);
	struct context *ctx = (struct context *)context;

	int index = 0;
	switch (question) {
	case Q_REGENERATE_UUIDS:
		if (util_uuid_generate(ctx->hdr.uuid) != 0) {
			ppc->result = CHECK_RESULT_CANNOT_REPAIR;
			return CHECK_ERR(ppc, "uuid generation failed");
		}
		CHECK_INFO(ppc, "setting UUIDs to %s",
			check_get_uuid_str(ctx->hdrp->uuid));
		pool_hdr_set_all_uuids(&ctx->hdr.uuid, 0);
		break;
	case Q_SET_VALID_UUID:
		uuid_get_max_same(&ctx->hdr.uuid, &index);
		unsigned char (*uuid_i)[POOL_HDR_UUID_LEN] = &ctx->hdr.uuid;
		CHECK_INFO(ppc, "setting UUIDs to %s",
			check_get_uuid_str(uuid_i[index]));
		pool_hdr_set_all_uuids(&ctx->hdr.uuid, index);
		break;
	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

/*
 * pool_hdr_uuids_check -- (internal) check UUID values for pool file
 */
static int
pool_hdr_uuids_check(PMEMpoolcheck *ppc, union location *loc)
{
	LOG(3, NULL);

	const struct pool_set *poolset = ppc->pool->set_file->poolset;
	int single_repl = poolset->nreplicas == 1;
	int single_part = poolset->replica[loc->replica]->nparts == 1;

	if (single_repl && single_part)
		return 0;

	struct pool_replica *rep = REP(poolset, loc->replica);
	struct pool_replica *next_rep = REP(poolset, loc->replica + 1);
	struct pool_replica *prev_rep = REP(poolset, loc->replica - 1);

	struct pool_hdr *next_part_hdrp = HDR(rep, loc->part + 1);
	struct pool_hdr *prev_part_hdrp = HDR(rep, loc->part - 1);
	struct pool_hdr *next_repl_hdrp = HDR(next_rep, 0);
	struct pool_hdr *prev_repl_hdrp = HDR(prev_rep, 0);

	int next_part_cs_valid = pool_hdr_valid(next_part_hdrp);
	int prev_part_cs_valid = pool_hdr_valid(prev_part_hdrp);
	int next_repl_cs_valid = pool_hdr_valid(next_repl_hdrp);
	int prev_repl_cs_valid = pool_hdr_valid(prev_repl_hdrp);

	struct pool_hdr hdr;
	pool_hdr_get(ppc, &hdr, NULL, loc);
	util_convert2h_hdr_nocheck(&hdr);

	int next_part_valid = !memcmp(hdr.next_part_uuid, next_part_hdrp->uuid,
		POOL_HDR_UUID_LEN);
	int prev_part_valid = !memcmp(hdr.prev_part_uuid, prev_part_hdrp->uuid,
		POOL_HDR_UUID_LEN);
	int next_repl_valid = !memcmp(hdr.next_repl_uuid, next_repl_hdrp->uuid,
		POOL_HDR_UUID_LEN);
	int prev_repl_valid = !memcmp(hdr.prev_repl_uuid, prev_repl_hdrp->uuid,
		POOL_HDR_UUID_LEN);

	if ((single_part || next_part_cs_valid) && !next_part_valid) {
		CHECK_ASK(ppc, Q_SET_NEXT_PART_UUID,
			"%sinvalid pool_hdr.next_part_uuid.|Do you want to set "
			"it to valid value?", loc->prefix);
	}

	if ((single_part || prev_part_cs_valid) && !prev_part_valid) {
		CHECK_ASK(ppc, Q_SET_PREV_PART_UUID,
			"%sinvalid pool_hdr.prev_part_uuid.|Do you want to set "
			"it to valid value?", loc->prefix);
	}

	if ((single_repl || prev_repl_cs_valid) && !next_repl_valid) {
		CHECK_ASK(ppc, Q_SET_NEXT_REPL_UUID,
			"%sinvalid pool_hdr.next_repl_uuid.|Do you want to set "
			"it to valid value?", loc->prefix);
	}

	if ((single_repl || next_repl_cs_valid) && !prev_repl_valid) {
		CHECK_ASK(ppc, Q_SET_PREV_REPL_UUID,
			"%sinvalid pool_hdr.prev_repl_uuid.|Do you want to set "
			"it to valid value?", loc->prefix);
	}

	return check_questions_sequence_validate(ppc);
}

/*
 * pool_hdr_uuids_fix -- (internal) fix UUID values for pool file
 */
static int
pool_hdr_uuids_fix(PMEMpoolcheck *ppc, struct check_step_data *location,
	uint32_t question, void *context)
{
	LOG(3, NULL);

	ASSERT(context != NULL);
	union location *loc = (union location *)location;
	struct context *ctx = (struct context *)context;

	switch (question) {
	case Q_SET_NEXT_PART_UUID:
		CHECK_INFO(ppc, "%ssetting pool_hdr.next_part_uuid to %s",
			loc->prefix,
			check_get_uuid_str(ctx->next_part_hdrp->uuid));
		memcpy(ctx->hdr.next_part_uuid, ctx->next_part_hdrp->uuid,
			POOL_HDR_UUID_LEN);
		break;
	case Q_SET_PREV_PART_UUID:
		CHECK_INFO(ppc, "%ssetting pool_hdr.prev_part_uuid to %s",
			loc->prefix,
			check_get_uuid_str(ctx->prev_part_hdrp->uuid));
		memcpy(ctx->hdr.prev_part_uuid, ctx->prev_part_hdrp->uuid,
			POOL_HDR_UUID_LEN);
		break;
	case Q_SET_NEXT_REPL_UUID:
		CHECK_INFO(ppc, "%ssetting pool_hdr.next_repl_uuid to %s",
			loc->prefix,
			check_get_uuid_str(ctx->next_repl_hdrp->uuid));
		memcpy(ctx->hdr.next_repl_uuid, ctx->next_repl_hdrp->uuid,
			POOL_HDR_UUID_LEN);
		break;
	case Q_SET_PREV_REPL_UUID:
		CHECK_INFO(ppc, "%ssetting pool_hdr.prev_repl_uuid to %s",
			loc->prefix,
			check_get_uuid_str(ctx->prev_repl_hdrp->uuid));
		memcpy(ctx->hdr.prev_repl_uuid, ctx->prev_repl_hdrp->uuid,
			POOL_HDR_UUID_LEN);
		break;
	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

struct step {
	int (*check)(PMEMpoolcheck *, union location *);
	int (*fix)(PMEMpoolcheck *, struct check_step_data *, uint32_t, void *);
	enum num_of_parts_and_replicas num_of_elements;
};

static const struct step steps[] = {
	{
		.check	= pool_hdr_checksum,
	},
	{
		.check	= pool_hdr_default_check,
	},
	{
		.fix	= pool_hdr_default_fix,
	},
	{
		.check	= pool_hdr_poolset_uuid,
	},
	{
		.fix	= pool_hdr_poolset_uuid_fix,
	},
	{
		.check	= pool_hdr_uuids_single,
		.num_of_elements = NUM_OF_PAR_SINGLE,
	},
	{
		.fix	= pool_hdr_uuids_single_fix,
		.num_of_elements = NUM_OF_PAR_SINGLE,
	},
	{
		.check	= pool_hdr_uuids_check,
		.num_of_elements = NUM_OF_PAR_MANY,
	},
	{
		.fix	= pool_hdr_uuids_fix,
		.num_of_elements = NUM_OF_PAR_MANY,
	},
	{
		.check	= pool_hdr_checksum_retry,
	},
	{
		.check	= pool_hdr_gen,
	},
	{
		.fix	= pool_hdr_gen_fix,
	},
	{
		.check	= NULL,
	},
};

/*
 * step_exe -- (internal) perform single step according to its parameters
 */
static int
step_exe(PMEMpoolcheck *ppc, union location *loc,
	struct pool_replica *rep, unsigned nreplicas)
{
	const struct step *step = &steps[loc->step++];

	if (step->num_of_elements == NUM_OF_PAR_SINGLE &&
			(nreplicas != 1 || rep->nparts != 1))
		return 0;
	if (step->num_of_elements == NUM_OF_PAR_MANY &&
			(nreplicas == 1 && rep->nparts == 1))
		return 0;

	if (!step->fix)
		return step->check(ppc, loc);

	if (!check_has_answer(ppc->data))
		return 0;

	struct context ctx;
	pool_hdr_get(ppc, &ctx.hdr, &ctx.hdrp, loc);
	util_convert2h_hdr_nocheck(&ctx.hdr);
	pool_hdr_default(ppc->pool->params.type, &ctx.def_hdr);

	if (step->num_of_elements == NUM_OF_PAR_MANY) {
		unsigned nr = (loc->replica + 1) % nreplicas;
		unsigned pr = (loc->replica - 1) % nreplicas;
		unsigned np = (loc->part + 1) % rep->nparts;
		unsigned pp = (loc->part - 1) % rep->nparts;

		const struct pool_set *poolset = ppc->pool->set_file->poolset;
		struct pool_replica *next_rep = poolset->replica[nr];
		struct pool_replica *prev_rep = poolset->replica[pr];

		ctx.next_part_hdrp = rep->part[np].hdr;
		ctx.prev_part_hdrp = rep->part[pp].hdr;

		ctx.next_repl_hdrp = next_rep->part[0].hdr;
		ctx.prev_repl_hdrp = prev_rep->part[0].hdr;
	}

	if (check_answer_loop(ppc, &loc->step_data, &ctx, step->fix))
		return -1;

	util_convert2le_hdr(&ctx.hdr);
	memcpy(ctx.hdrp, &ctx.hdr, sizeof(*ctx.hdrp));
	msync(ctx.hdrp, sizeof(*ctx.hdrp), MS_SYNC);
	loc->header_modified = 1;
	return 0;
}

/*
 * check_pool_hdr -- entry point for pool header checks
 */
void
check_pool_hdr(PMEMpoolcheck *ppc)
{
	LOG(3, NULL);

	COMPILE_ERROR_ON(sizeof(union location) !=
		sizeof(struct check_step_data));

	int rdonly = CHECK_WITHOUT_FIXING(ppc);
	if (pool_set_file_map_headers(ppc->pool->set_file, rdonly)) {
		ppc->result = CHECK_RESULT_ERROR;
		CHECK_ERR(ppc, "cannot map pool headers");
		return;
	}

	union location *loc = (union location *)check_get_step_data(ppc->data);
	unsigned nreplicas = ppc->pool->set_file->poolset->nreplicas;
	unsigned nfiles = pool_set_files_count(ppc->pool->set_file);
	struct pool_set *poolset = ppc->pool->set_file->poolset;

	for (; loc->replica < nreplicas; loc->replica++) {
		struct pool_replica *rep = poolset->replica[loc->replica];
		for (; loc->part < rep->nparts; loc->part++) {
			/* prepare prefix for messages */
			if (ppc->result != CHECK_RESULT_PROCESS_ANSWERS) {
				if (nfiles > 1) {
					snprintf(loc->prefix, PREFIX_MAX_SIZE,
						"replica %u part %u: ",
						loc->replica, loc->part);
				} else
					loc->prefix[0] = '\0';
				loc->step = 0;
			}

			/* do all checks */
			while (CHECK_NOT_COMPLETE(loc, steps)) {
				if (step_exe(ppc, loc, rep, nreplicas))
					goto cleanup;
			}
		}
	}

	memcpy(&ppc->pool->hdr.pool, poolset->replica[0]->part[0].hdr,
		sizeof(struct pool_hdr));

	if (loc->header_modified) {
		struct pool_hdr hdr;
		memcpy(&hdr, &ppc->pool->hdr.pool, sizeof(struct pool_hdr));
		util_convert2h_hdr_nocheck(&hdr);
		pool_params_from_header(&ppc->pool->params, &hdr);
	}

cleanup:
	pool_set_file_unmap_headers(ppc->pool->set_file);
}
