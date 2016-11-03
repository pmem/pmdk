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
#include <endian.h>

#include "out.h"
#include "libpmempool.h"
#include "pmempool.h"
#include "pool.h"
#include "set.h"
#include "check_util.h"

#define PREFIX_MAX_SIZE 30

/* assure size match between global and internal check step data */
union location {
	/* internal check step data */
	struct {
		unsigned replica;
		unsigned part;
		unsigned step;
		char prefix[PREFIX_MAX_SIZE];
		int header_modified;

		int single_repl;
		int single_part;

		struct pool_hdr *hdrp;
		/* copy of the pool header in host byte order */
		struct pool_hdr hdr;

		struct pool_hdr *next_part_hdrp;
		struct pool_hdr *prev_part_hdrp;
		struct pool_hdr *next_repl_hdrp;
		struct pool_hdr *prev_repl_hdrp;

		int next_part_hdr_valid;
		int prev_part_hdr_valid;
		int next_repl_hdr_valid;
		int prev_repl_hdr_valid;

		uuid_t *valid_uuid;
	};
	/* global check step data */
	struct check_step_data step_data;
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
	Q_POOLSET_UUID_FROM_BTT_INFO,
	Q_POOLSET_UUID_FROM_VALID_PART,
	Q_POOLSET_UUID_REGENERATE,
	Q_UUID_FROM_LINK,
	Q_UUID_REGENERATE,
	Q_NEXT_PART_UUID_SET,
	Q_NEXT_PART_UUID_REGENERATE,
	Q_PREV_PART_UUID_SET,
	Q_PREV_PART_UUID_REGENERATE,
	Q_NEXT_REPL_UUID_SET,
	Q_NEXT_REPL_UUID_REGENERATE,
	Q_PREV_REPL_UUID_SET,
	Q_PREV_REPL_UUID_REGENERATE
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
 * pool_hdr_checksum -- (internal) check pool header by checksum
 */
static int
pool_hdr_checksum(PMEMpoolcheck *ppc, union location *loc)
{
	LOG(3, NULL);

	CHECK_INFO(ppc, "%schecking pool header", loc->prefix);
	struct pool_hdr hdr;
	memcpy(&hdr, loc->hdrp, sizeof(hdr));

	int hdr_valid = pool_hdr_valid(&hdr);

	if (util_is_zeroed((void *)&hdr, sizeof(hdr))) {
		if (CHECK_IS_NOT(ppc, REPAIR)) {
			check_end(ppc->data);
			ppc->result = CHECK_RESULT_NOT_CONSISTENT;
			return CHECK_ERR(ppc, "empty pool hdr");
		}
	} else if (hdr_valid) {
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
		return CHECK_ERR(ppc, "the repair of %s pools is not supported",
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

	struct pool_hdr def_hdr;
	pool_hdr_default(ppc->pool->params.type, &def_hdr);

	if (memcmp(loc->hdr.signature, def_hdr.signature, POOL_HDR_SIG_LEN)) {
		CHECK_ASK(ppc, Q_DEFAULT_SIGNATURE,
			"%spool_hdr.signature is not valid.|Do you want to set "
			"it to %.8s?", loc->prefix, def_hdr.signature);
	}

	if (loc->hdr.major != def_hdr.major) {
		CHECK_ASK(ppc, Q_DEFAULT_MAJOR,
			"%spool_hdr.major is not valid.|Do you want to set it "
			"to default value 0x%x?", loc->prefix, def_hdr.major);
	}

	if (loc->hdr.compat_features != def_hdr.compat_features) {
		CHECK_ASK(ppc, Q_DEFAULT_COMPAT_FEATURES,
			"%spool_hdr.compat_features is not valid.|Do you want "
			"to set it to default value 0x%x?", loc->prefix,
			def_hdr.compat_features);
	}

	if (loc->hdr.incompat_features != def_hdr.incompat_features) {
		CHECK_ASK(ppc, Q_DEFAULT_INCOMPAT_FEATURES,
			"%spool_hdr.incompat_features is not valid.|Do you "
			"want to set it to default value 0x%x?", loc->prefix,
			def_hdr.incompat_features);
	}

	if (loc->hdr.ro_compat_features != def_hdr.ro_compat_features) {
		CHECK_ASK(ppc, Q_DEFAULT_RO_COMPAT_FEATURES,
			"%spool_hdr.ro_compat_features is not valid.|Do you "
			"want to set it to default value 0x%x?", loc->prefix,
			def_hdr.ro_compat_features);
	}

	if (!util_is_zeroed(loc->hdr.unused, sizeof(loc->hdr.unused))) {
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

	struct pool_hdr def_hdr;
	pool_hdr_default(ppc->pool->params.type, &def_hdr);
	union location *loc = (union location *)location;

	switch (question) {
	case Q_DEFAULT_SIGNATURE:
		CHECK_INFO(ppc, "setting pool_hdr.signature to %.8s",
			def_hdr.signature);
		memcpy(&loc->hdr.signature, &def_hdr.signature,
			POOL_HDR_SIG_LEN);
		break;
	case Q_DEFAULT_MAJOR:
		CHECK_INFO(ppc, "setting pool_hdr.major to 0x%x",
			def_hdr.major);
		loc->hdr.major = def_hdr.major;
		break;
	case Q_DEFAULT_COMPAT_FEATURES:
		CHECK_INFO(ppc, "setting pool_hdr.compat_features to 0x%x",
			def_hdr.compat_features);
		loc->hdr.compat_features = def_hdr.compat_features;
		break;
	case Q_DEFAULT_INCOMPAT_FEATURES:
		CHECK_INFO(ppc, "setting pool_hdr.incompat_features to 0x%x",
			def_hdr.incompat_features);
		loc->hdr.incompat_features = def_hdr.incompat_features;
		break;
	case Q_DEFAULT_RO_COMPAT_FEATURES:
		CHECK_INFO(ppc, "setting pool_hdr.ro_compat_features to 0x%x",
			def_hdr.ro_compat_features);
		loc->hdr.ro_compat_features = def_hdr.ro_compat_features;
		break;
	case Q_ZERO_UNUSED_AREA:
		CHECK_INFO(ppc, "setting pool_hdr.unused to zeros");
		memset(loc->hdr.unused, 0, sizeof(loc->hdr.unused));
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
pool_get_valid_part(PMEMpoolcheck *ppc, unsigned rid, unsigned pid)
{
	const struct pool_set *poolset = ppc->pool->set_file->poolset;
	for (unsigned r = 0; r < poolset->nreplicas; r++) {
		struct pool_replica *rep = poolset->replica[r];
		for (unsigned p = 0; p < rep->nparts; p++) {
			/* skip part of replica known as invalid */
			if (r == rid && p == pid)
				continue;

			if (pool_hdr_valid(rep->part[p].hdr))
				return &rep->part[p];
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

	/* for blk pool we can take the UUID from BTT Info header */
	if (ppc->pool->params.type == POOL_TYPE_BLK &&
		ppc->pool->bttc.valid) {
		if (uuidcmp(loc->hdr.poolset_uuid,
				ppc->pool->bttc.btt_info.parent_uuid) == 0) {
			return 0;
		}

		CHECK_ASK(ppc, Q_POOLSET_UUID_FROM_BTT_INFO,
			"%sinvalid pool_hdr.poolset_uuid.|Do you want to set "
			"it to %s from BTT Info?", loc->prefix,
			check_get_uuid_str(
			ppc->pool->bttc.btt_info.parent_uuid));
		goto exit_question;
	} else if (ppc->pool->params.is_poolset) {
		const struct pool_set_part *valid_part =
			pool_get_valid_part(ppc, loc->replica, loc->part);
		if (!valid_part)
			goto regenerate;

		loc->valid_uuid = &((struct pool_hdr *)valid_part->hdr)->
			poolset_uuid;
		if (uuidcmp(loc->hdr.poolset_uuid, *loc->valid_uuid) == 0)
			return 0;
		CHECK_ASK(ppc, Q_POOLSET_UUID_FROM_VALID_PART,
			"%sinvalid pool_hdr.poolset_uuid.|Do you want to set "
			"it to %s from a valid pool file part?", loc->prefix,
			check_get_uuid_str(*loc->valid_uuid));
		goto exit_question;
	}

regenerate:
	if (CHECK_IS_NOT(ppc, ADVANCED)) {
		ppc->result = CHECK_RESULT_CANNOT_REPAIR;
		return CHECK_ERR(ppc, "can not repair pool_hdr.poolset_uuid");
	} else {
		CHECK_ASK(ppc, Q_POOLSET_UUID_REGENERATE,
			"%sinvalid pool_hdr.poolset_uuid.|Do you want to "
			"regenerate pool_hdr.poolset_uuid?", loc->prefix);
	}
exit_question:
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

	union location *loc = (union location *)location;

	switch (question) {
	case Q_POOLSET_UUID_FROM_BTT_INFO:
		CHECK_INFO(ppc, "%ssetting pool_hdr.poolset_uuid to %s",
			loc->prefix, check_get_uuid_str(
			ppc->pool->bttc.btt_info.parent_uuid));
		memcpy(loc->hdr.poolset_uuid,
			ppc->pool->bttc.btt_info.parent_uuid,
			POOL_HDR_UUID_LEN);
		ppc->pool->uuid_op = UUID_FROM_BTT;
		break;
	case Q_POOLSET_UUID_FROM_VALID_PART:
		CHECK_INFO(ppc, "%ssetting pool_hdr.poolset_uuid to %s",
			loc->prefix,
			check_get_uuid_str(*loc->valid_uuid));
		memcpy(loc->hdr.poolset_uuid, loc->valid_uuid,
			POOL_HDR_UUID_LEN);
		break;
	case Q_POOLSET_UUID_REGENERATE:
		if (util_uuid_generate(loc->hdr.poolset_uuid) != 0) {
			ppc->result = CHECK_RESULT_CANNOT_REPAIR;
			return CHECK_ERR(ppc, "uuid generation failed");
		}
		CHECK_INFO(ppc, "%ssetting pool_hdr.pooset_uuid to %s",
			loc->prefix,
			check_get_uuid_str(loc->hdr.poolset_uuid));
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
	memcpy(&hdr, loc->hdrp, sizeof(hdr));

	if (pool_hdr_valid(&hdr))
		loc->step = CHECK_STEP_COMPLETE;

	return 0;
}

/*
 * pool_hdr_gen -- (internal) validate creation time and checksum
 */
static int
pool_hdr_gen(PMEMpoolcheck *ppc, union location *loc)
{
	LOG(3, NULL);

	if (loc->hdr.crtime > (uint64_t)ppc->pool->set_file->mtime) {
		CHECK_ASK(ppc, Q_CRTIME,
			"%spool_hdr.crtime is not valid.|Do you want to set it "
			"to file's modtime [%s]?", loc->prefix,
			check_get_time_str(ppc->pool->set_file->mtime));
	}

	CHECK_ASK(ppc, Q_CHECKSUM, "Do you want to regenerate checksum?");

	return check_questions_sequence_validate(ppc);
}

/*
 * pool_hdr_gen_fix -- (internal) fix creation time and checksum
 */
static int
pool_hdr_gen_fix(PMEMpoolcheck *ppc, struct check_step_data *location,
	uint32_t question, void *context)
{
	LOG(3, NULL);

	union location *loc = (union location *)location;

	switch (question) {
	case Q_CRTIME:
		CHECK_INFO(ppc, "setting pool_hdr.crtime to file's modtime: %s",
			check_get_time_str(ppc->pool->set_file->mtime));
		util_convert2h_hdr_nocheck(&loc->hdr);
		loc->hdr.crtime = (uint64_t)ppc->pool->set_file->mtime;
		util_convert2le_hdr(&loc->hdr);
		break;
	case Q_CHECKSUM:
		util_checksum(&loc->hdr, sizeof(loc->hdr), &loc->hdr.checksum,
			1);
		CHECK_INFO(ppc, "setting pool_hdr.checksum to 0x%jx",
			le64toh(loc->hdr.checksum));
		break;
	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

/*
 * pool_hdr_uuid -- (internal) check UUID value
 */
static int
pool_hdr_uuid(PMEMpoolcheck *ppc, union location *loc)
{
	LOG(3, NULL);

	int valid;
	loc->valid_uuid = NULL;
	if (loc->next_part_hdr_valid) {
		valid = uuidcmp(
			loc->hdr.uuid, loc->next_part_hdrp->prev_part_uuid);
		if (valid != 0) {
			loc->valid_uuid = &loc->next_part_hdrp->prev_part_uuid;
		}
	} else if (loc->prev_part_hdr_valid) {
		valid = uuidcmp(
			loc->hdr.uuid, loc->prev_part_hdrp->next_part_uuid);
		if (valid != 0) {
			loc->valid_uuid = &loc->prev_part_hdrp->next_part_uuid;
		}
	} else if (loc->part == 0) {
		if (loc->next_repl_hdr_valid) {
			valid = uuidcmp(loc->hdr.uuid,
				loc->next_repl_hdrp->prev_repl_uuid);
			if (valid != 0) {
				loc->valid_uuid =
					&loc->next_repl_hdrp->prev_repl_uuid;
			}
		} else if (loc->prev_repl_hdr_valid) {
			valid = uuidcmp(loc->hdr.uuid,
				loc->prev_repl_hdrp->next_repl_uuid);
			if (valid != 0) {
				loc->valid_uuid =
					&loc->prev_repl_hdrp->next_repl_uuid;
			}
		}
	}
	if (loc->valid_uuid) {
		CHECK_ASK(ppc, Q_UUID_FROM_LINK,
			"%sinvalid pool_hdr.uuid.|Do you want to set it to a "
			"valid value?", loc->prefix);
	} else if (CHECK_IS(ppc, ADVANCED)) {
		CHECK_ASK(ppc, Q_UUID_REGENERATE,
			"%sinvalid pool_hdr.uuid.|Do you want to regenerate "
			"it?", loc->prefix);
	} else {
		ppc->result = CHECK_RESULT_CANNOT_REPAIR;
		return CHECK_ERR(ppc, "can not repair pool_hdr.uuid");
	}

	return check_questions_sequence_validate(ppc);
}

/*
 * pool_hdr_uuid_fix -- (internal) fix UUID value
 */
static int
pool_hdr_uuid_fix(PMEMpoolcheck *ppc, struct check_step_data *location,
	uint32_t question, void *context)
{
	LOG(3, NULL);

	union location *loc = (union location *)location;

	switch (question) {
	case Q_UUID_FROM_LINK:
		CHECK_INFO(ppc, "%ssetting pool_hdr.uuid to %s", loc->prefix,
			check_get_uuid_str(*loc->valid_uuid));
		memcpy(loc->hdr.uuid, *loc->valid_uuid, POOL_HDR_UUID_LEN);
		break;
	case Q_UUID_REGENERATE:
		if (util_uuid_generate(loc->hdr.uuid) != 0) {
			ppc->result = CHECK_RESULT_CANNOT_REPAIR;
			return CHECK_ERR(ppc, "uuid generation failed");
		}
		CHECK_INFO(ppc, "%ssetting pool_hdr.uuid to %s", loc->prefix,
			check_get_uuid_str(loc->hdr.uuid));
		break;
	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

/*
 * pool_hdr_uuid_links -- (internal) check UUID links values
 */
static int
pool_hdr_uuid_links(PMEMpoolcheck *ppc, union location *loc)
{
	LOG(3, NULL);

	const char *field = "";

	if (uuidcmp(loc->hdr.next_part_uuid, loc->next_part_hdrp->uuid)) {
		field = "pool_hdr.next_part_uuid";
		if (loc->single_part || loc->next_part_hdr_valid) {
			CHECK_ASK(ppc, Q_NEXT_PART_UUID_SET,
				"%sinvalid %s.|Do you want to set it to a "
				"valid value?", loc->prefix, field);
		} else if (CHECK_IS(ppc, ADVANCED)) {
			CHECK_ASK(ppc, Q_NEXT_PART_UUID_REGENERATE,
				"%sinvalid %s.|Do you want to regenerate it?",
				loc->prefix, field);
		} else {
			goto cannot_repair;
		}
	}

	if (uuidcmp(loc->hdr.prev_part_uuid, loc->prev_part_hdrp->uuid)) {
		field = "pool_hdr.prev_part_uuid";
		if (loc->single_part || loc->prev_part_hdr_valid) {
			CHECK_ASK(ppc, Q_PREV_PART_UUID_SET,
				"%sinvalid %s.|Do you want to set it to a "
				"valid value?", loc->prefix, field);
		} else if (CHECK_IS(ppc, ADVANCED)) {
			CHECK_ASK(ppc, Q_PREV_PART_UUID_REGENERATE,
				"%sinvalid %s.|Do you want to regenerate it?",
				loc->prefix, field);
		} else {
			goto cannot_repair;
		}
	}

	if (uuidcmp(loc->hdr.next_repl_uuid, loc->next_repl_hdrp->uuid)) {
		field = "pool_hdr.next_repl_uuid";
		if (loc->single_repl || loc->next_repl_hdr_valid) {
			CHECK_ASK(ppc, Q_NEXT_REPL_UUID_SET,
				"%sinvalid %s.|Do you want to set it to a "
				"valid value?", loc->prefix, field);
		} else if (CHECK_IS(ppc, ADVANCED)) {
			CHECK_ASK(ppc, Q_NEXT_REPL_UUID_REGENERATE,
				"%sinvalid %s.|Do you want to regenerate it?",
				loc->prefix, field);
		} else {
			goto cannot_repair;
		}
	}

	if (uuidcmp(loc->hdr.prev_repl_uuid, loc->prev_repl_hdrp->uuid)) {
		field = "pool_hdr.prev_repl_uuid";
		if (loc->single_repl || loc->prev_repl_hdr_valid) {
			CHECK_ASK(ppc, Q_PREV_REPL_UUID_SET,
				"%sinvalid %s.|Do you want to set it to a "
				"valid value?", loc->prefix, field);
		} else if (CHECK_IS(ppc, ADVANCED)) {
			CHECK_ASK(ppc, Q_PREV_REPL_UUID_REGENERATE,
				"%sinvalid %s.|Do you want to regenerate it?",
				loc->prefix, field);
		} else {
			goto cannot_repair;
		}
	}

	return check_questions_sequence_validate(ppc);

cannot_repair:
	ppc->result = CHECK_RESULT_CANNOT_REPAIR;
	return CHECK_ERR(ppc, "can not repair %s", field);
}

/*
 * pool_hdr_uuid_links_fix -- (internal) fix UUID links values
 */
static int
pool_hdr_uuid_links_fix(PMEMpoolcheck *ppc, struct check_step_data *location,
	uint32_t question, void *context)
{
	LOG(3, NULL);

	union location *loc = (union location *)location;

	switch (question) {
	case Q_NEXT_PART_UUID_SET:
		CHECK_INFO(ppc, "%ssetting pool_hdr.next_part_uuid to %s",
			loc->prefix,
			check_get_uuid_str(loc->next_part_hdrp->uuid));
		memcpy(loc->hdr.next_part_uuid, loc->next_part_hdrp->uuid,
			POOL_HDR_UUID_LEN);
		break;
	case Q_NEXT_PART_UUID_REGENERATE:
		if (util_uuid_generate(loc->hdr.next_part_uuid) != 0) {
			ppc->result = CHECK_RESULT_CANNOT_REPAIR;
			return CHECK_ERR(ppc, "uuid generation failed");
		}
		CHECK_INFO(ppc, "%ssetting pool_hdr.next_part_uuid to %s",
			loc->prefix,
			check_get_uuid_str(loc->hdr.next_part_uuid));
		break;
	case Q_PREV_PART_UUID_SET:
		CHECK_INFO(ppc, "%ssetting pool_hdr.prev_part_uuid to %s",
			loc->prefix,
			check_get_uuid_str(loc->prev_part_hdrp->uuid));
		memcpy(loc->hdr.prev_part_uuid, loc->prev_part_hdrp->uuid,
			POOL_HDR_UUID_LEN);
		break;
	case Q_PREV_PART_UUID_REGENERATE:
		if (util_uuid_generate(loc->hdr.prev_part_uuid) != 0) {
			ppc->result = CHECK_RESULT_CANNOT_REPAIR;
			return CHECK_ERR(ppc, "uuid generation failed");
		}
		CHECK_INFO(ppc, "%ssetting pool_hdr.prev_part_uuid to %s",
			loc->prefix,
			check_get_uuid_str(loc->hdr.prev_part_uuid));
		break;
	case Q_NEXT_REPL_UUID_SET:
		CHECK_INFO(ppc, "%ssetting pool_hdr.next_repl_uuid to %s",
			loc->prefix,
			check_get_uuid_str(loc->next_repl_hdrp->uuid));
		memcpy(loc->hdr.next_repl_uuid, loc->next_repl_hdrp->uuid,
			POOL_HDR_UUID_LEN);
		break;
	case Q_NEXT_REPL_UUID_REGENERATE:
		if (util_uuid_generate(loc->hdr.next_repl_uuid) != 0) {
			ppc->result = CHECK_RESULT_CANNOT_REPAIR;
			return CHECK_ERR(ppc, "uuid generation failed");
		}
		CHECK_INFO(ppc, "%ssetting pool_hdr.next_repl_uuid to %s",
			loc->prefix,
			check_get_uuid_str(loc->hdr.next_repl_uuid));
		break;
	case Q_PREV_REPL_UUID_SET:
		CHECK_INFO(ppc, "%ssetting pool_hdr.prev_repl_uuid to %s",
			loc->prefix,
			check_get_uuid_str(loc->prev_repl_hdrp->uuid));
		memcpy(loc->hdr.prev_repl_uuid, loc->prev_repl_hdrp->uuid,
			POOL_HDR_UUID_LEN);
		break;
	case Q_PREV_REPL_UUID_REGENERATE:
		if (util_uuid_generate(loc->hdr.prev_repl_uuid) != 0) {
			ppc->result = CHECK_RESULT_CANNOT_REPAIR;
			return CHECK_ERR(ppc, "uuid generation failed");
		}
		CHECK_INFO(ppc, "%ssetting pool_hdr.prev_repl_uuid to %s",
			loc->prefix,
			check_get_uuid_str(loc->hdr.prev_repl_uuid));
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
		.check	= pool_hdr_checksum,
	},
	{
		.check	= pool_hdr_default_check,
	},
	{
		.fix	= pool_hdr_default_fix,
		.check	= pool_hdr_checksum_retry,
	},
	{
		.check	= pool_hdr_poolset_uuid,
	},
	{
		.fix	= pool_hdr_poolset_uuid_fix,
		.check	= pool_hdr_checksum_retry,
	},
	{
		.check	= pool_hdr_uuid,
	},
	{
		.fix	= pool_hdr_uuid_fix,
		.check	= pool_hdr_checksum_retry,
	},
	{
		.check	= pool_hdr_uuid_links,
	},
	{
		.fix	= pool_hdr_uuid_links_fix,
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

	if (!step->fix)
		return step->check(ppc, loc);

	if (!check_has_answer(ppc->data))
		return 0;

	if (check_answer_loop(ppc, &loc->step_data, NULL, step->fix))
		return -1;

	util_convert2le_hdr(&loc->hdr);
	memcpy(loc->hdrp, &loc->hdr, sizeof(loc->hdr));
	msync(loc->hdrp, sizeof(*loc->hdrp), MS_SYNC);

	util_convert2h_hdr_nocheck(&loc->hdr);
	loc->header_modified = 1;

	/* execute check after fix if available */
	if (step->check)
		return step->check(ppc, loc);

	return 0;
}

/*
 * init_location_data -- (internal) prepare location information
 */
static void
init_location_data(PMEMpoolcheck *ppc, union location *loc)
{
	/* prepare prefix for messages */
	unsigned nfiles = pool_set_files_count(ppc->pool->set_file);
	if (ppc->result != CHECK_RESULT_PROCESS_ANSWERS) {
		if (nfiles > 1) {
			snprintf(loc->prefix, PREFIX_MAX_SIZE,
				"replica %u part %u: ",
				loc->replica, loc->part);
		} else
			loc->prefix[0] = '\0';
		loc->step = 0;
	}

	/* get neighboring parts and replicas and briefly validate them */
	const struct pool_set *poolset = ppc->pool->set_file->poolset;
	loc->single_repl = poolset->nreplicas == 1;
	loc->single_part = poolset->replica[loc->replica]->nparts == 1;

	struct pool_replica *rep = REP(poolset, loc->replica);
	struct pool_replica *next_rep = REP(poolset, loc->replica + 1);
	struct pool_replica *prev_rep = REP(poolset, loc->replica - 1);

	loc->hdrp = HDR(rep, loc->part);
	memcpy(&loc->hdr, loc->hdrp, sizeof(loc->hdr));
	util_convert2h_hdr_nocheck(&loc->hdr);

	loc->next_part_hdrp = HDR(rep, loc->part + 1);
	loc->prev_part_hdrp = HDR(rep, loc->part - 1);
	loc->next_repl_hdrp = HDR(next_rep, 0);
	loc->prev_repl_hdrp = HDR(prev_rep, 0);

	loc->next_part_hdr_valid = pool_hdr_valid(loc->next_part_hdrp);
	loc->prev_part_hdr_valid = pool_hdr_valid(loc->prev_part_hdrp);
	loc->next_repl_hdr_valid = pool_hdr_valid(loc->next_repl_hdrp);
	loc->prev_repl_hdr_valid = pool_hdr_valid(loc->prev_repl_hdrp);
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
	struct pool_set *poolset = ppc->pool->set_file->poolset;

	for (; loc->replica < nreplicas; loc->replica++) {
		struct pool_replica *rep = poolset->replica[loc->replica];
		for (; loc->part < rep->nparts; loc->part++) {
			init_location_data(ppc, loc);

			/* do all checks */
			while (CHECK_NOT_COMPLETE(loc, steps)) {
				if (step_exe(ppc, loc, rep, nreplicas))
					goto cleanup;
			}
		}

		loc->step = 0;
		loc->part = 0;
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
