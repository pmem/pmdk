// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2021, Intel Corporation */

/*
 * check_log.c -- check pmemlog
 */

#include <inttypes.h>
#include <sys/param.h>
#include <endian.h>

#include "out.h"
#include "libpmempool.h"
#include "pmempool.h"
#include "pool.h"
#include "check_util.h"

enum question {
	Q_LOG_START_OFFSET,
	Q_LOG_END_OFFSET,
	Q_LOG_WRITE_OFFSET,
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
	log_convert2h(&ppc->pool->hdr.log);
	return 0;
}

/*
 * log_hdr_check -- (internal) check pmemlog header
 */
static int
log_hdr_check(PMEMpoolcheck *ppc, location *loc)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(loc);

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
log_hdr_fix(PMEMpoolcheck *ppc, location *loc, uint32_t question, void *ctx)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(loc, ctx);

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

struct step {
	int (*check)(PMEMpoolcheck *, location *);
	int (*fix)(PMEMpoolcheck *, location *, uint32_t, void *);
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
	ASSERTeq(ppc->pool->params.type, POOL_TYPE_LOG);

	const struct step *step = &steps[loc->step++];

	if (!(step->type & ppc->pool->params.type))
		return 0;

	if (!step->fix)
		return step->check(ppc, loc);

	if (log_read(ppc)) {
		ppc->result = CHECK_RESULT_ERROR;
		return -1;
	}

	return check_answer_loop(ppc, loc, NULL, 1,  step->fix);
}

/*
 * check_log -- entry point for pmemlog checks
 */
void
check_log(PMEMpoolcheck *ppc)
{
	LOG(3, NULL);

	location *loc = check_get_step_data(ppc->data);

	/* do all checks */
	while (CHECK_NOT_COMPLETE(loc, steps)) {
		if (step_exe(ppc, loc))
			break;
	}
}
