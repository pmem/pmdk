// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * check_pool_hdr.c -- pool header check
 */

#include <stdio.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <endian.h>

#include "out.h"
#include "util_pmem.h"
#include "libpmempool.h"
#include "libpmem.h"
#include "pmempool.h"
#include "pool.h"
#include "set.h"
#include "check_util.h"

#define NO_COMMON_POOLSET_UUID	"%sno common pool_hdr.poolset_uuid"
#define INVALID_UUID		"%sinvalid pool_hdr.uuid"
#define INVALID_CHECKSUM	"%sinvalid pool_hdr.checksum"

enum question {
	Q_DEFAULT_SIGNATURE,
	Q_DEFAULT_MAJOR,
	Q_DEFAULT_COMPAT_FEATURES,
	Q_DEFAULT_INCOMPAT_FEATURES,
	Q_DEFAULT_RO_COMPAT_FEATURES,
	Q_ZERO_UNUSED_AREA,
	Q_ARCH_FLAGS,
	Q_CRTIME,
	Q_CHECKSUM,
	Q_POOLSET_UUID_SET,
	Q_POOLSET_UUID_FROM_BTT_INFO,
	Q_POOLSET_UUID_REGENERATE,
	Q_UUID_SET,
	Q_UUID_REGENERATE,
	Q_NEXT_PART_UUID_SET,
	Q_PREV_PART_UUID_SET,
	Q_NEXT_REPL_UUID_SET,
	Q_PREV_REPL_UUID_SET
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
		util_checksum(hdrp, sizeof(*hdrp), &hdrp->checksum, 0,
			POOL_HDR_CSUM_END_OFF(hdrp));
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
 * pool_hdr_preliminary_check -- (internal) check pool header checksum and pool
 *	parameters
 */
static int
pool_hdr_preliminary_check(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	CHECK_INFO(ppc, "%schecking pool header", loc->prefix);

	if (util_is_zeroed((void *)&loc->hdr, sizeof(loc->hdr))) {
		if (CHECK_IS_NOT(ppc, REPAIR)) {
			check_end(ppc->data);
			ppc->result = CHECK_RESULT_NOT_CONSISTENT;
			return CHECK_ERR(ppc, "%sempty pool hdr", loc->prefix);
		}
	} else if (loc->hdr_valid) {
		enum pool_type type = pool_hdr_get_type(&loc->hdr);
		if (type == POOL_TYPE_UNKNOWN) {
			if (CHECK_IS_NOT(ppc, REPAIR)) {
				check_end(ppc->data);
				ppc->result = CHECK_RESULT_NOT_CONSISTENT;
				return CHECK_ERR(ppc, "%sinvalid signature",
					loc->prefix);
			}

			CHECK_INFO(ppc, "%sinvalid signature", loc->prefix);
		} else {
			/* valid check sum */
			CHECK_INFO(ppc, "%spool header correct",
				loc->prefix);
			loc->step = CHECK_STEP_COMPLETE;
			return 0;
		}
	} else if (CHECK_IS_NOT(ppc, REPAIR)) {
		check_end(ppc->data);
		ppc->result = CHECK_RESULT_NOT_CONSISTENT;
		return CHECK_ERR(ppc, "%sincorrect pool header", loc->prefix);
	} else {
		CHECK_INFO(ppc, "%sincorrect pool header", loc->prefix);
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
			pool_get_pool_type_str(ppc->pool->params.type));
	}

	return 0;
}

/*
 * pool_hdr_default_check -- (internal) check some default values in pool header
 */
static int
pool_hdr_default_check(PMEMpoolcheck *ppc, location *loc)
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

	features_t unknown = util_get_unknown_features(
			loc->hdr.features, def_hdr.features);
	if (unknown.compat) {
		CHECK_ASK(ppc, Q_DEFAULT_COMPAT_FEATURES,
			"%spool_hdr.features.compat is not valid.|Do you want "
			"to set it to default value 0x%x?", loc->prefix,
			def_hdr.features.compat);
	}

	if (unknown.incompat) {
		CHECK_ASK(ppc, Q_DEFAULT_INCOMPAT_FEATURES,
			"%spool_hdr.features.incompat is not valid.|Do you "
			"want to set it to default value 0x%x?", loc->prefix,
			def_hdr.features.incompat);
	}

	if (unknown.ro_compat) {
		CHECK_ASK(ppc, Q_DEFAULT_RO_COMPAT_FEATURES,
			"%spool_hdr.features.ro_compat is not valid.|Do you "
			"want to set it to default value 0x%x?", loc->prefix,
			def_hdr.features.ro_compat);
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
pool_hdr_default_fix(PMEMpoolcheck *ppc, location *loc, uint32_t question,
	void *context)
{
	LOG(3, NULL);

	ASSERTne(loc, NULL);
	struct pool_hdr def_hdr;
	pool_hdr_default(ppc->pool->params.type, &def_hdr);

	switch (question) {
	case Q_DEFAULT_SIGNATURE:
		CHECK_INFO(ppc, "%ssetting pool_hdr.signature to %.8s",
			loc->prefix, def_hdr.signature);
		memcpy(&loc->hdr.signature, &def_hdr.signature,
			POOL_HDR_SIG_LEN);
		break;
	case Q_DEFAULT_MAJOR:
		CHECK_INFO(ppc, "%ssetting pool_hdr.major to 0x%x", loc->prefix,
			def_hdr.major);
		loc->hdr.major = def_hdr.major;
		break;
	case Q_DEFAULT_COMPAT_FEATURES:
		CHECK_INFO(ppc, "%ssetting pool_hdr.features.compat to 0x%x",
			loc->prefix, def_hdr.features.compat);
		loc->hdr.features.compat = def_hdr.features.compat;
		break;
	case Q_DEFAULT_INCOMPAT_FEATURES:
		CHECK_INFO(ppc, "%ssetting pool_hdr.features.incompat to 0x%x",
			loc->prefix, def_hdr.features.incompat);
		loc->hdr.features.incompat = def_hdr.features.incompat;
		break;
	case Q_DEFAULT_RO_COMPAT_FEATURES:
		CHECK_INFO(ppc, "%ssetting pool_hdr.features.ro_compat to 0x%x",
			loc->prefix, def_hdr.features.ro_compat);
		loc->hdr.features.ro_compat = def_hdr.features.ro_compat;
		break;
	case Q_ZERO_UNUSED_AREA:
		CHECK_INFO(ppc, "%ssetting pool_hdr.unused to zeros",
			loc->prefix);
		memset(loc->hdr.unused, 0, sizeof(loc->hdr.unused));
		break;
	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

/*
 * pool_hdr_quick_check -- (internal) end check if pool header is valid
 */
static int
pool_hdr_quick_check(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);
	if (pool_hdr_valid(loc->hdrp))
		loc->step = CHECK_STEP_COMPLETE;

	return 0;
}

/*
 * pool_hdr_nondefault -- (internal) validate custom value fields
 */
static int
pool_hdr_nondefault(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	if (loc->hdr.crtime > (uint64_t)ppc->pool->set_file->mtime) {
		const char * const error = "%spool_hdr.crtime is not valid";
		if (CHECK_IS_NOT(ppc, REPAIR)) {
			ppc->result = CHECK_RESULT_NOT_CONSISTENT;
			return CHECK_ERR(ppc, error, loc->prefix);
		} else if (CHECK_IS_NOT(ppc, ADVANCED)) {
			ppc->result = CHECK_RESULT_CANNOT_REPAIR;
			CHECK_INFO(ppc, "%s" REQUIRE_ADVANCED, loc->prefix);
			return CHECK_ERR(ppc, error, loc->prefix);
		}

		CHECK_ASK(ppc, Q_CRTIME,
			"%spool_hdr.crtime is not valid.|Do you want to set it "
			"to file's modtime [%s]?", loc->prefix,
			check_get_time_str(ppc->pool->set_file->mtime));
	}

	if (loc->valid_part_hdrp &&
			memcmp(&loc->valid_part_hdrp->arch_flags,
				&loc->hdr.arch_flags,
				sizeof(struct arch_flags)) != 0) {
		const char * const error = "%spool_hdr.arch_flags is not valid";
		if (CHECK_IS_NOT(ppc, REPAIR)) {
			ppc->result = CHECK_RESULT_NOT_CONSISTENT;
			return CHECK_ERR(ppc, error, loc->prefix);
		}

		CHECK_ASK(ppc, Q_ARCH_FLAGS,
			"%spool_hdr.arch_flags is not valid.|Do you want to "
			"copy it from a valid part?", loc->prefix);
	}

	return check_questions_sequence_validate(ppc);
}

/*
 * pool_hdr_nondefault_fix -- (internal) fix custom value fields
 */
static int
pool_hdr_nondefault_fix(PMEMpoolcheck *ppc, location *loc, uint32_t question,
	void *context)
{
	LOG(3, NULL);

	ASSERTne(loc, NULL);
	uint64_t *flags = NULL;

	switch (question) {
	case Q_CRTIME:
		CHECK_INFO(ppc, "%ssetting pool_hdr.crtime to file's modtime: "
			"%s", loc->prefix,
			check_get_time_str(ppc->pool->set_file->mtime));
		util_convert2h_hdr_nocheck(&loc->hdr);
		loc->hdr.crtime = (uint64_t)ppc->pool->set_file->mtime;
		util_convert2le_hdr(&loc->hdr);
		break;
	case Q_ARCH_FLAGS:
		flags = (uint64_t *)&loc->valid_part_hdrp->arch_flags;
		CHECK_INFO(ppc, "%ssetting pool_hdr.arch_flags to 0x%08" PRIx64
				"%08" PRIx64, loc->prefix, flags[0], flags[1]);
		util_convert2h_hdr_nocheck(&loc->hdr);
		memcpy(&loc->hdr.arch_flags, &loc->valid_part_hdrp->arch_flags,
			sizeof(struct arch_flags));
		util_convert2le_hdr(&loc->hdr);
		break;
	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

/*
 * pool_hdr_poolset_uuid -- (internal) check poolset_uuid field
 */
static int
pool_hdr_poolset_uuid_find(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	/*
	 * If the pool header is valid and there is not other parts or replicas
	 * in the poolset its poolset_uuid is also valid.
	 */
	if (loc->hdr_valid && loc->single_repl && loc->single_part)
		return 0;

	if (loc->replica != 0 || loc->part != 0)
		goto after_lookup;

	/* for blk pool we can take the UUID from BTT Info header */
	if (ppc->pool->params.type == POOL_TYPE_BLK && ppc->pool->bttc.valid) {
		loc->valid_puuid = &ppc->pool->bttc.btt_info.parent_uuid;
		if (uuidcmp(loc->hdr.poolset_uuid, *loc->valid_puuid) != 0) {
			CHECK_ASK(ppc, Q_POOLSET_UUID_FROM_BTT_INFO,
				"%sinvalid pool_hdr.poolset_uuid.|Do you want "
				"to set it to %s from BTT Info?", loc->prefix,
				check_get_uuid_str(*loc->valid_puuid));
			goto exit_question;
		}
	}

	if (loc->single_part && loc->single_repl) {
		/*
		 * If the pool is not blk pool or BTT Info header is invalid
		 * there is no other way to validate poolset uuid.
		 */
		return 0;
	}

	/*
	 * if all valid poolset part files have the same poolset uuid it is
	 * the valid poolset uuid
	 * if all part files have the same poolset uuid it is valid poolset uuid
	 */
	struct pool_set *poolset = ppc->pool->set_file->poolset;
	unsigned nreplicas = poolset->nreplicas;
	uuid_t *common_puuid = loc->valid_puuid;
	for (unsigned r = 0; r < nreplicas; r++) {
		struct pool_replica *rep = REP(poolset, r);
		for (unsigned p = 0; p < rep->nhdrs; p++) {
			struct pool_hdr *hdr = HDR(rep, p);

			/*
			 * find poolset uuid if it is the same for all part
			 * files
			 */
			if (common_puuid == NULL) {
				if (r == 0 && p == 0) {
					common_puuid = &hdr->poolset_uuid;
				}
			} else if (uuidcmp(*common_puuid, hdr->poolset_uuid)
					!= 0) {
				common_puuid = NULL;
			}

			if (!pool_hdr_valid(hdr))
				continue;

			/*
			 * find poolset uuid if it is the same for all valid
			 * part files
			 */
			if (loc->valid_puuid == NULL) {
				loc->valid_puuid = &hdr->poolset_uuid;
			} else if (uuidcmp(*loc->valid_puuid, hdr->poolset_uuid)
					!= 0) {
				ppc->result = CHECK_RESULT_NOT_CONSISTENT;
				return CHECK_ERR(ppc, "the poolset contains "
					"part files from various poolsets");
			}
		}
	}

	if (!loc->valid_puuid && common_puuid)
		loc->valid_puuid = common_puuid;

	if (loc->valid_puuid)
		goto after_lookup;

	if (CHECK_IS_NOT(ppc, REPAIR)) {
		ppc->result = CHECK_RESULT_NOT_CONSISTENT;
		return CHECK_ERR(ppc, NO_COMMON_POOLSET_UUID, loc->prefix);
	} else if (CHECK_IS_NOT(ppc, ADVANCED)) {
		ppc->result = CHECK_RESULT_CANNOT_REPAIR;
		CHECK_INFO(ppc, "%s" REQUIRE_ADVANCED, loc->prefix);
		return CHECK_ERR(ppc, NO_COMMON_POOLSET_UUID, loc->prefix);
	} else {
		CHECK_ASK(ppc, Q_POOLSET_UUID_REGENERATE, NO_COMMON_POOLSET_UUID
			".|Do you want to regenerate pool_hdr.poolset_uuid?",
			loc->prefix);
		goto exit_question;
	}

after_lookup:
	if (loc->valid_puuid) {
		if (uuidcmp(*loc->valid_puuid, loc->hdr.poolset_uuid) != 0) {
			if (CHECK_IS_NOT(ppc, REPAIR)) {
				ppc->result = CHECK_RESULT_NOT_CONSISTENT;
				return CHECK_ERR(ppc, "%sinvalid "
					"pool_hdr.poolset_uuid", loc->prefix);
			}

			CHECK_ASK(ppc, Q_POOLSET_UUID_SET, "%sinvalid "
				"pool_hdr.poolset_uuid.|Do you want to set "
				"it to %s from a valid part file?", loc->prefix,
				check_get_uuid_str(*loc->valid_puuid));
		}
	}

exit_question:
	return check_questions_sequence_validate(ppc);
}

/*
 * pool_hdr_poolset_uuid_fix -- (internal) fix poolset_uuid field
 */
static int
pool_hdr_poolset_uuid_fix(PMEMpoolcheck *ppc, location *loc, uint32_t question,
	void *context)
{
	LOG(3, NULL);

	ASSERTne(loc, NULL);

	switch (question) {
	case Q_POOLSET_UUID_SET:
	case Q_POOLSET_UUID_FROM_BTT_INFO:
		CHECK_INFO(ppc, "%ssetting pool_hdr.poolset_uuid to %s",
			loc->prefix, check_get_uuid_str(*loc->valid_puuid));
		memcpy(loc->hdr.poolset_uuid, loc->valid_puuid,
			POOL_HDR_UUID_LEN);
		if (question == Q_POOLSET_UUID_SET)
			ppc->pool->uuid_op = UUID_NOT_FROM_BTT;
		else
			ppc->pool->uuid_op = UUID_FROM_BTT;
		break;
	case Q_POOLSET_UUID_REGENERATE:
		if (util_uuid_generate(loc->hdr.poolset_uuid) != 0) {
			ppc->result = CHECK_RESULT_INTERNAL_ERROR;
			return CHECK_ERR(ppc, "%suuid generation failed",
				loc->prefix);
		}
		CHECK_INFO(ppc, "%ssetting pool_hdr.pooset_uuid to %s",
			loc->prefix,
			check_get_uuid_str(loc->hdr.poolset_uuid));
		ppc->pool->uuid_op = UUID_NOT_FROM_BTT;
		break;
	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

#define COMPARE_TO_FIRST_PART_ONLY 2

/*
 * pool_hdr_uuid_find -- (internal) check UUID value
 */
static int
pool_hdr_uuid_find(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	/*
	 * If the pool header is valid and there is not other parts or replicas
	 * in the poolset its uuid is also valid.
	 */
	if (loc->hdr_valid && loc->single_repl && loc->single_part)
		return 0;

	int hdrs_valid[] = {
		loc->next_part_hdr_valid, loc->prev_part_hdr_valid,
		loc->next_repl_hdr_valid, loc->prev_repl_hdr_valid};
	uuid_t *uuids[] = {
		&loc->next_part_hdrp->prev_part_uuid,
		&loc->prev_part_hdrp->next_part_uuid,
		&loc->next_repl_hdrp->prev_repl_uuid,
		&loc->prev_repl_hdrp->next_repl_uuid
	};

	/*
	 * if all valid poolset part files have the same uuid links to this part
	 * file it is valid uuid
	 * if all links have the same uuid and it is single file pool it is also
	 * the valid uuid
	 */
	loc->valid_uuid = NULL;
	if (loc->hdr_valid)
		loc->valid_uuid = &loc->hdr.uuid;
	uuid_t *common_uuid = uuids[0];

	COMPILE_ERROR_ON(ARRAY_SIZE(uuids) != ARRAY_SIZE(hdrs_valid));
	COMPILE_ERROR_ON(COMPARE_TO_FIRST_PART_ONLY >= ARRAY_SIZE(uuids));
	for (unsigned i = 0; i < ARRAY_SIZE(uuids); ++i) {
		if (i > 0 && common_uuid != NULL) {
			if (uuidcmp(*common_uuid, *uuids[i]) != 0) {
				common_uuid = NULL;
			}
		}

		if (i >= COMPARE_TO_FIRST_PART_ONLY && loc->part != 0)
			continue;

		if (!hdrs_valid[i])
			continue;

		if (!loc->valid_uuid) {
			loc->valid_uuid = uuids[i];
		} else if (uuidcmp(*loc->valid_uuid, *uuids[i]) != 0) {
			ppc->result = CHECK_RESULT_NOT_CONSISTENT;
			return CHECK_ERR(ppc, "%sambiguous pool_hdr.uuid",
				loc->prefix);
		}
	}

	if (!loc->valid_uuid && common_uuid)
		loc->valid_uuid = common_uuid;

	if (loc->valid_uuid != NULL) {
		if (uuidcmp(*loc->valid_uuid, loc->hdr.uuid) != 0) {
			CHECK_ASK(ppc, Q_UUID_SET, INVALID_UUID ".|Do you want "
				"to set it to %s from a valid part file?",
				loc->prefix,
				check_get_uuid_str(*loc->valid_uuid));
		}
	} else if (CHECK_IS(ppc, ADVANCED)) {
		CHECK_ASK(ppc, Q_UUID_REGENERATE, INVALID_UUID ".|Do you want "
				"to regenerate it?", loc->prefix);
	} else if (CHECK_IS(ppc, REPAIR)) {
		ppc->result = CHECK_RESULT_CANNOT_REPAIR;
		CHECK_INFO(ppc, "%s" REQUIRE_ADVANCED, loc->prefix);
		return CHECK_ERR(ppc, INVALID_UUID, loc->prefix);
	} else {
		ppc->result = CHECK_RESULT_NOT_CONSISTENT;
		return CHECK_ERR(ppc, INVALID_UUID, loc->prefix);
	}

	return check_questions_sequence_validate(ppc);
}

/*
 * pool_hdr_uuid_fix -- (internal) fix UUID value
 */
static int
pool_hdr_uuid_fix(PMEMpoolcheck *ppc, location *loc, uint32_t question,
	void *context)
{
	LOG(3, NULL);

	ASSERTne(loc, NULL);

	switch (question) {
	case Q_UUID_SET:
		CHECK_INFO(ppc, "%ssetting pool_hdr.uuid to %s", loc->prefix,
			check_get_uuid_str(*loc->valid_uuid));
		memcpy(loc->hdr.uuid, loc->valid_uuid, POOL_HDR_UUID_LEN);
		break;
	case Q_UUID_REGENERATE:
		if (util_uuid_generate(loc->hdr.uuid) != 0) {
			ppc->result = CHECK_RESULT_INTERNAL_ERROR;
			return CHECK_ERR(ppc, "%suuid generation failed",
				loc->prefix);
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
pool_hdr_uuid_links(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	/*
	 * If the pool header is valid and there is not other parts or replicas
	 * in the poolset its uuid links are also valid.
	 */
	if (loc->hdr_valid && loc->single_repl && loc->single_part)
		return 0;

	uuid_t *links[] = {
		&loc->hdr.next_part_uuid, &loc->hdr.prev_part_uuid,
		&loc->hdr.next_repl_uuid, &loc->hdr.prev_repl_uuid};
	uuid_t *uuids[] = {
		&loc->next_part_hdrp->uuid, &loc->prev_part_hdrp->uuid,
		&loc->next_repl_hdrp->uuid, &loc->prev_repl_hdrp->uuid
	};
	uint32_t questions[] = {
		Q_NEXT_PART_UUID_SET, Q_PREV_PART_UUID_SET,
		Q_NEXT_REPL_UUID_SET, Q_PREV_REPL_UUID_SET
	};
	const char *fields[] = {
		"pool_hdr.next_part_uuid", "pool_hdr.prev_part_uuid",
		"pool_hdr.next_repl_uuid", "pool_hdr.prev_repl_uuid"
	};

	COMPILE_ERROR_ON(ARRAY_SIZE(links) != ARRAY_SIZE(uuids));
	COMPILE_ERROR_ON(ARRAY_SIZE(links) != ARRAY_SIZE(questions));
	COMPILE_ERROR_ON(ARRAY_SIZE(links) != ARRAY_SIZE(fields));
	for (uint64_t i = 0; i < ARRAY_SIZE(links); ++i) {
		if (uuidcmp(*links[i], *uuids[i]) == 0)
			continue;

		if (CHECK_IS(ppc, REPAIR)) {
			CHECK_ASK(ppc, questions[i],
				"%sinvalid %s.|Do you want to set it to a "
				"valid value?", loc->prefix, fields[i]);
		} else {
			ppc->result = CHECK_RESULT_NOT_CONSISTENT;
			return CHECK_ERR(ppc, "%sinvalid %s", loc->prefix,
				fields[i]);
		}
	}

	return check_questions_sequence_validate(ppc);
}

/*
 * pool_hdr_uuid_links_fix -- (internal) fix UUID links values
 */
static int
pool_hdr_uuid_links_fix(PMEMpoolcheck *ppc, location *loc, uint32_t question,
	void *context)
{
	LOG(3, NULL);

	ASSERTne(loc, NULL);

	switch (question) {
	case Q_NEXT_PART_UUID_SET:
		CHECK_INFO(ppc, "%ssetting pool_hdr.next_part_uuid to %s",
			loc->prefix,
			check_get_uuid_str(loc->next_part_hdrp->uuid));
		memcpy(loc->hdr.next_part_uuid, loc->next_part_hdrp->uuid,
			POOL_HDR_UUID_LEN);
		break;
	case Q_PREV_PART_UUID_SET:
		CHECK_INFO(ppc, "%ssetting pool_hdr.prev_part_uuid to %s",
			loc->prefix,
			check_get_uuid_str(loc->prev_part_hdrp->uuid));
		memcpy(loc->hdr.prev_part_uuid, loc->prev_part_hdrp->uuid,
			POOL_HDR_UUID_LEN);
		break;
	case Q_NEXT_REPL_UUID_SET:
		CHECK_INFO(ppc, "%ssetting pool_hdr.next_repl_uuid to %s",
			loc->prefix,
			check_get_uuid_str(loc->next_repl_hdrp->uuid));
		memcpy(loc->hdr.next_repl_uuid, loc->next_repl_hdrp->uuid,
			POOL_HDR_UUID_LEN);
		break;
	case Q_PREV_REPL_UUID_SET:
		CHECK_INFO(ppc, "%ssetting pool_hdr.prev_repl_uuid to %s",
			loc->prefix,
			check_get_uuid_str(loc->prev_repl_hdrp->uuid));
		memcpy(loc->hdr.prev_repl_uuid, loc->prev_repl_hdrp->uuid,
			POOL_HDR_UUID_LEN);
		break;
	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

/*
 * pool_hdr_checksum -- (internal) validate checksum
 */
static int
pool_hdr_checksum(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	if (loc->hdr_valid)
		return 0;

	if (CHECK_IS_NOT(ppc, REPAIR)) {
		ppc->result = CHECK_RESULT_NOT_CONSISTENT;
		return CHECK_ERR(ppc, INVALID_CHECKSUM, loc->prefix);
	} else if (CHECK_IS_NOT(ppc, ADVANCED)) {
		ppc->result = CHECK_RESULT_CANNOT_REPAIR;
		CHECK_INFO(ppc, "%s" REQUIRE_ADVANCED, loc->prefix);
		return CHECK_ERR(ppc, INVALID_CHECKSUM, loc->prefix);
	}

	CHECK_ASK(ppc, Q_CHECKSUM, INVALID_CHECKSUM ".|Do you want to "
		"regenerate checksum?", loc->prefix);
	return check_questions_sequence_validate(ppc);
}

/*
 * pool_hdr_checksum_fix -- (internal) fix checksum
 */
static int
pool_hdr_checksum_fix(PMEMpoolcheck *ppc, location *loc, uint32_t question,
	void *context)
{
	LOG(3, NULL);

	ASSERTne(loc, NULL);

	switch (question) {
	case Q_CHECKSUM:
		util_checksum(&loc->hdr, sizeof(loc->hdr), &loc->hdr.checksum,
			1, POOL_HDR_CSUM_END_OFF(&loc->hdr));
		CHECK_INFO(ppc, "%ssetting pool_hdr.checksum to 0x%jx",
			loc->prefix, le64toh(loc->hdr.checksum));
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

static const struct step steps_initial[] = {
	{
		.check	= pool_hdr_preliminary_check,
	},
	{
		.check	= pool_hdr_default_check,
	},
	{
		.fix	= pool_hdr_default_fix,
		.check	= pool_hdr_quick_check,
	},
	{
		.check	= pool_hdr_nondefault,
	},
	{
		.fix	= pool_hdr_nondefault_fix,
	},
	{
		.check	= NULL,
		.fix	= NULL,
	},
};

static const struct step steps_uuids[] = {
	{
		.check	= pool_hdr_poolset_uuid_find,
	},
	{
		.fix	= pool_hdr_poolset_uuid_fix,
	},
	{
		.check	= pool_hdr_uuid_find,
	},
	{
		.fix	= pool_hdr_uuid_fix,
	},
	{
		.check	= pool_hdr_uuid_links,
	},
	{
		.fix	= pool_hdr_uuid_links_fix,
	},
	{
		.check	= pool_hdr_checksum,
	},
	{
		.fix	= pool_hdr_checksum_fix,
	},
	{
		.check	= NULL,
		.fix	= NULL,
	},
};

/*
 * step_exe -- (internal) perform single step according to its parameters
 */
static int
step_exe(PMEMpoolcheck *ppc, const struct step *steps, location *loc,
	struct pool_replica *rep, unsigned nreplicas)
{
	const struct step *step = &steps[loc->step++];

	if (!step->fix)
		return step->check(ppc, loc);

	if (!check_has_answer(ppc->data))
		return 0;

	if (check_answer_loop(ppc, loc, NULL, 1, step->fix))
		return -1;

	util_convert2le_hdr(&loc->hdr);
	memcpy(loc->hdrp, &loc->hdr, sizeof(loc->hdr));
	loc->hdr_valid = pool_hdr_valid(loc->hdrp);
	util_persist_auto(rep->part[0].is_dev_dax, loc->hdrp,
			sizeof(*loc->hdrp));

	util_convert2h_hdr_nocheck(&loc->hdr);
	loc->pool_hdr_modified = 1;

	/* execute check after fix if available */
	if (step->check)
		return step->check(ppc, loc);

	return 0;
}

/*
 * init_location_data -- (internal) prepare location information
 */
static void
init_location_data(PMEMpoolcheck *ppc, location *loc)
{
	/* prepare prefix for messages */
	unsigned nfiles = pool_set_files_count(ppc->pool->set_file);
	if (ppc->result != CHECK_RESULT_PROCESS_ANSWERS) {
		if (nfiles > 1) {
			int ret = snprintf(loc->prefix, PREFIX_MAX_SIZE,
				"replica %u part %u: ",
				loc->replica, loc->part);
			if (ret < 0 || ret >= PREFIX_MAX_SIZE)
				FATAL("snprintf: %d", ret);
		} else
			loc->prefix[0] = '\0';
		loc->step = 0;
	}

	/* get neighboring parts and replicas and briefly validate them */
	const struct pool_set *poolset = ppc->pool->set_file->poolset;
	loc->single_repl = poolset->nreplicas == 1;
	loc->single_part = poolset->replica[loc->replica]->nparts == 1;

	struct pool_replica *rep = REP(poolset, loc->replica);
	struct pool_replica *next_rep = REPN(poolset, loc->replica);
	struct pool_replica *prev_rep = REPP(poolset, loc->replica);

	loc->hdrp = HDR(rep, loc->part);
	memcpy(&loc->hdr, loc->hdrp, sizeof(loc->hdr));
	util_convert2h_hdr_nocheck(&loc->hdr);
	loc->hdr_valid = pool_hdr_valid(loc->hdrp);

	loc->next_part_hdrp = HDRN(rep, loc->part);
	loc->prev_part_hdrp = HDRP(rep, loc->part);
	loc->next_repl_hdrp = HDR(next_rep, 0);
	loc->prev_repl_hdrp = HDR(prev_rep, 0);

	loc->next_part_hdr_valid = pool_hdr_valid(loc->next_part_hdrp);
	loc->prev_part_hdr_valid = pool_hdr_valid(loc->prev_part_hdrp);
	loc->next_repl_hdr_valid = pool_hdr_valid(loc->next_repl_hdrp);
	loc->prev_repl_hdr_valid = pool_hdr_valid(loc->prev_repl_hdrp);

	if (!loc->valid_part_done || loc->valid_part_replica != loc->replica) {
		loc->valid_part_hdrp = NULL;
		for (unsigned p = 0; p < rep->nhdrs; ++p) {
			if (pool_hdr_valid(HDR(rep, p))) {
				loc->valid_part_hdrp = HDR(rep, p);
				break;
			}
		}
		loc->valid_part_done = true;
	}
}

/*
 * check_pool_hdr -- entry point for pool header checks
 */
void
check_pool_hdr(PMEMpoolcheck *ppc)
{
	LOG(3, NULL);

	location *loc = check_get_step_data(ppc->data);
	unsigned nreplicas = ppc->pool->set_file->poolset->nreplicas;
	struct pool_set *poolset = ppc->pool->set_file->poolset;

	for (; loc->replica < nreplicas; loc->replica++) {
		struct pool_replica *rep = poolset->replica[loc->replica];
		for (; loc->part < rep->nhdrs; loc->part++) {
			init_location_data(ppc, loc);

			/* do all checks */
			while (CHECK_NOT_COMPLETE(loc, steps_initial)) {
				ASSERT(loc->step < ARRAY_SIZE(steps_initial));
				if (step_exe(ppc, steps_initial, loc, rep,
						nreplicas))
					return;
			}
		}

		loc->part = 0;
	}

	memcpy(&ppc->pool->hdr.pool, poolset->replica[0]->part[0].hdr,
		sizeof(struct pool_hdr));

	if (loc->pool_hdr_modified) {
		struct pool_hdr hdr;
		memcpy(&hdr, &ppc->pool->hdr.pool, sizeof(struct pool_hdr));
		util_convert2h_hdr_nocheck(&hdr);
		pool_params_from_header(&ppc->pool->params, &hdr);
	}
}

/*
 * check_pool_hdr_uuids -- entry point for pool header links checks
 */
void
check_pool_hdr_uuids(PMEMpoolcheck *ppc)
{
	LOG(3, NULL);

	location *loc = check_get_step_data(ppc->data);
	unsigned nreplicas = ppc->pool->set_file->poolset->nreplicas;
	struct pool_set *poolset = ppc->pool->set_file->poolset;

	for (; loc->replica < nreplicas; loc->replica++) {
		struct pool_replica *rep = poolset->replica[loc->replica];
		for (; loc->part < rep->nparts; loc->part++) {
			init_location_data(ppc, loc);

			/* do all checks */
			while (CHECK_NOT_COMPLETE(loc, steps_uuids)) {
				ASSERT(loc->step < ARRAY_SIZE(steps_uuids));
				if (step_exe(ppc, steps_uuids, loc, rep,
						nreplicas))
					return;
			}
		}

		loc->part = 0;
	}

	memcpy(&ppc->pool->hdr.pool, poolset->replica[0]->part[0].hdr,
		sizeof(struct pool_hdr));

	if (loc->pool_hdr_modified) {
		struct pool_hdr hdr;
		memcpy(&hdr, &ppc->pool->hdr.pool, sizeof(struct pool_hdr));
		util_convert2h_hdr_nocheck(&hdr);
		pool_params_from_header(&ppc->pool->params, &hdr);
	}
}
