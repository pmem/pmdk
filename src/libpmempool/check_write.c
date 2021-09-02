// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2021, Intel Corporation */

/*
 * check_write.c -- write fixed data back
 */

#include <stdint.h>
#include <endian.h>

#include "out.h"
#include "btt.h"
#include "libpmempool.h"
#include "pmempool.h"
#include "pool.h"
#include "check_util.h"

enum questions {
	Q_REPAIR_MAP,
	Q_REPAIR_FLOG,
};

/*
 * log_write -- (internal) write all structures for log pool
 */
static int
log_write(PMEMpoolcheck *ppc, location *loc)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(loc);

	LOG(3, NULL);

	if (CHECK_WITHOUT_FIXING(ppc))
		return 0;

	/* endianness conversion */
	struct pmemlog *log = &ppc->pool->hdr.log;
	log_convert2le(log);

	if (pool_write(ppc->pool, log, sizeof(*log), 0)) {
		ppc->result = CHECK_RESULT_CANNOT_REPAIR;
		return CHECK_ERR(ppc, "writing pmemlog structure failed");
	}

	return 0;
}

/*
 * blk_write_flog -- (internal) convert and write flog to file
 */
static int
blk_write_flog(PMEMpoolcheck *ppc, struct arena *arenap)
{
	if (!arenap->flog) {
		ppc->result = CHECK_RESULT_ERROR;
		return CHECK_ERR(ppc, "flog is missing");
	}

	uint64_t flogoff = arenap->offset + arenap->btt_info.flogoff;

	uint8_t *ptr = arenap->flog;
	uint32_t i;
	for (i = 0; i < arenap->btt_info.nfree; i++) {
		struct btt_flog *flog = (struct btt_flog *)ptr;
		btt_flog_convert2le(&flog[0]);
		btt_flog_convert2le(&flog[1]);
		ptr += BTT_FLOG_PAIR_ALIGN;
	}

	if (pool_write(ppc->pool, arenap->flog, arenap->flogsize, flogoff)) {
		CHECK_INFO(ppc, "%s", ppc->path);
		ppc->result = CHECK_RESULT_CANNOT_REPAIR;
		return CHECK_ERR(ppc, "arena %u: writing BTT FLOG failed\n",
			arenap->id);
	}

	return 0;
}

/*
 * blk_write_map -- (internal) convert and write map to file
 */
static int
blk_write_map(PMEMpoolcheck *ppc, struct arena *arenap)
{
	if (!arenap->map) {
		ppc->result = CHECK_RESULT_ERROR;
		return CHECK_ERR(ppc, "map is missing");
	}

	uint64_t mapoff = arenap->offset + arenap->btt_info.mapoff;

	uint32_t i;
	for (i = 0; i < arenap->btt_info.external_nlba; i++)
		arenap->map[i] = htole32(arenap->map[i]);

	if (pool_write(ppc->pool, arenap->map, arenap->mapsize, mapoff)) {
		CHECK_INFO(ppc, "%s", ppc->path);
		ppc->result = CHECK_RESULT_CANNOT_REPAIR;
		return CHECK_ERR(ppc, "arena %u: writing BTT map failed\n",
			arenap->id);
	}

	return 0;
}

/*
 * blk_write -- (internal) write all structures for blk pool
 */
static int
blk_write(PMEMpoolcheck *ppc, location *loc)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(loc);

	LOG(3, NULL);

	if (CHECK_WITHOUT_FIXING(ppc))
		return 0;

	/* endianness conversion */
	ppc->pool->hdr.blk.bsize = htole32(ppc->pool->hdr.blk.bsize);

	if (pool_write(ppc->pool, &ppc->pool->hdr.blk,
			sizeof(ppc->pool->hdr.blk), 0)) {
		CHECK_INFO(ppc, "%s", ppc->path);
		ppc->result = CHECK_RESULT_CANNOT_REPAIR;
		return CHECK_ERR(ppc, "writing pmemblk structure failed");
	}

	return 0;
}

/*
 * btt_data_write -- (internal) write BTT data
 */
static int
btt_data_write(PMEMpoolcheck *ppc, location *loc)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(loc);

	LOG(3, NULL);

	struct arena *arenap;

	PMDK_TAILQ_FOREACH(arenap, &ppc->pool->arenas, next) {

		if (ppc->pool->uuid_op == UUID_NOT_FROM_BTT) {
			memcpy(arenap->btt_info.parent_uuid,
				ppc->pool->hdr.pool.poolset_uuid,
					sizeof(arenap->btt_info.parent_uuid));

			util_checksum(&arenap->btt_info,
					sizeof(arenap->btt_info),
				&arenap->btt_info.checksum, 1, 0);
		}

		if (pool_write(ppc->pool, &arenap->btt_info,
				sizeof(arenap->btt_info), arenap->offset)) {
			CHECK_INFO(ppc, "%s", ppc->path);
			CHECK_ERR(ppc, "arena %u: writing BTT Info failed",
				arenap->id);
			goto error;
		}

		if (pool_write(ppc->pool, &arenap->btt_info,
				sizeof(arenap->btt_info), arenap->offset +
				le64toh(arenap->btt_info.infooff))) {
			CHECK_INFO(ppc, "%s", ppc->path);
			CHECK_ERR(ppc,
				"arena %u: writing BTT Info backup failed",
				arenap->id);
			goto error;
		}

		if (blk_write_flog(ppc, arenap))
			goto error;

		if (blk_write_map(ppc, arenap))
			goto error;
	}

	return 0;

error:
	ppc->result = CHECK_RESULT_CANNOT_REPAIR;
	return -1;
}

struct step {
	int (*func)(PMEMpoolcheck *, location *loc);
	enum pool_type type;
};

static const struct step steps[] = {
	{
		.func		= log_write,
		.type		= POOL_TYPE_LOG,
	},
	{
		.func		= blk_write,
		.type		= POOL_TYPE_BLK,
	},
	{
		.func		= btt_data_write,
		.type		= POOL_TYPE_BLK | POOL_TYPE_BTT,
	},
	{
		.func		= NULL,
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

	/* check step conditions */
	if (!(step->type & ppc->pool->params.type))
		return 0;

	return step->func(ppc, loc);
}

/*
 * check_write -- write fixed data back
 */
void
check_write(PMEMpoolcheck *ppc)
{
	/*
	 * XXX: Disabling individual checks based on type should be done in the
	 *	step structure. This however requires refactor of the step
	 *	processing code.
	 */
	if (CHECK_IS_NOT(ppc, REPAIR))
		return;

	location *loc = (location *)check_get_step_data(ppc->data);

	/* do all steps */
	while (loc->step != CHECK_STEP_COMPLETE &&
		steps[loc->step].func != NULL) {

		if (step_exe(ppc, loc))
			return;
	}
}
