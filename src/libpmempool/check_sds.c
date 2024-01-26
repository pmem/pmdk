// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2024, Intel Corporation */

/*
 * check_shutdown_state.c -- shutdown state check
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

enum question {
	Q_RESET_SDS,
};

#define SDS_CHECK_STR	"checking shutdown state"
#define SDS_OK_STR	"shutdown state correct"
#define SDS_DIRTY_STR	"shutdown state is dirty"

#define ADR_FAILURE_STR \
	"an ADR failure was detected - your pool might be corrupted"

#define ZERO_SDS_STR \
	"Do you want to zero shutdown state?"

#define RESET_SDS_STR \
	"Do you want to reset shutdown state at your own risk? " \
	"If you have more then one replica you will have to " \
	"synchronize your pool after this operation."

#define SDS_FAIL_MSG(hdrp) \
	IGNORE_SDS(hdrp) ? SDS_DIRTY_STR : ADR_FAILURE_STR

#define SDS_REPAIR_MSG(hdrp) \
	IGNORE_SDS(hdrp) \
		? SDS_DIRTY_STR ".|" ZERO_SDS_STR \
		: ADR_FAILURE_STR ".|" RESET_SDS_STR

/*
 * sds_check_replica -- (internal) check if replica is healthy
 */
static int
sds_check_replica(location *loc)
{
	LOG(3, NULL);

	struct pool_replica *rep = REP(loc->set, loc->replica);

	/* make a copy of sds as we shouldn't modify a pool */
	struct shutdown_state old_sds = loc->hdr.sds;
	struct shutdown_state curr_sds;

	if (IGNORE_SDS(&loc->hdr))
		return util_is_zeroed(&old_sds, sizeof(old_sds)) ? 0 : -1;

	shutdown_state_init(&curr_sds, NULL);

	/* get current shutdown state */
	for (unsigned p = 0; p < rep->nparts; ++p) {
		if (shutdown_state_add_part(&curr_sds,
				PART(rep, p)->fd, NULL))
			return -1;
	}

	/* compare current and old shutdown state */
	return shutdown_state_check(&curr_sds, &old_sds, NULL);
}

/*
 * sds_check -- (internal) check shutdown_state
 */
static int
sds_check(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	CHECK_INFO(ppc, "%s" SDS_CHECK_STR, loc->prefix);

	/* shutdown state is valid */
	if (!sds_check_replica(loc)) {
		CHECK_INFO(ppc, "%s" SDS_OK_STR, loc->prefix);
		loc->step = CHECK_STEP_COMPLETE;
		return 0;
	}

	/* shutdown state is NOT valid and can NOT be repaired */
	if (CHECK_IS_NOT(ppc, REPAIR)) {
		check_end(ppc->data);
		ppc->result = CHECK_RESULT_NOT_CONSISTENT;
		return CHECK_ERR(ppc, "%s%s", loc->prefix,
				SDS_FAIL_MSG(&loc->hdr));
	}

	/* shutdown state is NOT valid but can be repaired */
	CHECK_ASK(ppc, Q_RESET_SDS, "%s%s", loc->prefix,
			SDS_REPAIR_MSG(&loc->hdr));
	return check_questions_sequence_validate(ppc);
}

/*
 * sds_fix -- (internal) fix shutdown state
 */
static int
sds_fix(PMEMpoolcheck *ppc, location *loc, uint32_t question,
	void *context)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(context);

	LOG(3, NULL);

	switch (question) {
	case Q_RESET_SDS:
		CHECK_INFO(ppc, "%sresetting pool_hdr.sds", loc->prefix);
		memset(&loc->hdr.sds, 0, sizeof(loc->hdr.sds));
		++loc->healthy_replicas;
		break;
	default:
		ERR_WO_ERRNO("not implemented question id: %u", question);
	}
	return 0;
}

struct step {
	int (*check)(PMEMpoolcheck *, location *);
	int (*fix)(PMEMpoolcheck *, location *, uint32_t, void *);
};

static const struct step steps[] = {
	{
		.check	= sds_check,
	},
	{
		.fix	= sds_fix,
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
step_exe(PMEMpoolcheck *ppc, location *loc)
{
	const struct step *step = &steps[loc->step++];

	if (!step->fix)
		return step->check(ppc, loc);

	if (!check_has_answer(ppc->data))
		return 0;

	if (check_answer_loop(ppc, loc, NULL, 0 /* fail on no */, step->fix))
		return -1;

	util_convert2le_hdr(&loc->hdr);
	memcpy(loc->hdrp, &loc->hdr, sizeof(loc->hdr));
	util_persist_auto(loc->is_dev_dax, loc->hdrp, sizeof(*loc->hdrp));

	util_convert2h_hdr_nocheck(&loc->hdr);
	loc->pool_hdr_modified = 1;

	return 0;
}

/*
 * init_prefix -- prepare prefix for messages
 */
static void
init_prefix(location *loc)
{
	if (loc->set->nreplicas > 1) {
		int ret = util_snprintf(loc->prefix, PREFIX_MAX_SIZE,
			"replica %u: ",
			loc->replica);
		if (ret < 0)
			CORE_LOG_FATAL_W_ERRNO("snprintf");
	} else
		loc->prefix[0] = '\0';
	loc->step = 0;
}

/*
 * init_location_data -- (internal) prepare location information
 */
static void
init_location_data(PMEMpoolcheck *ppc, location *loc)
{
	ASSERTeq(loc->part, 0);

	loc->set = ppc->pool->set_file->poolset;

	if (ppc->result != CHECK_RESULT_PROCESS_ANSWERS)
		init_prefix(loc);

	struct pool_replica *rep = REP(loc->set, loc->replica);
	loc->hdrp = HDR(rep, loc->part);
	memcpy(&loc->hdr, loc->hdrp, sizeof(loc->hdr));
	util_convert2h_hdr_nocheck(&loc->hdr);
	loc->is_dev_dax = PART(rep, 0)->is_dev_dax;
}

/*
 * sds_get_healthy_replicas_num -- (internal) get number of healthy replicas
 */
static void
sds_get_healthy_replicas_num(PMEMpoolcheck *ppc, location *loc)
{
	const unsigned nreplicas = ppc->pool->set_file->poolset->nreplicas;
	loc->healthy_replicas = 0;
	loc->part = 0;

	for (; loc->replica < nreplicas; loc->replica++) {
		init_location_data(ppc, loc);

		if (!sds_check_replica(loc)) {
			++loc->healthy_replicas; /* healthy replica found */
		}
	}

	loc->replica = 0; /* reset replica index */
}

/*
 * check_sds -- entry point for shutdown state checks
 */
void
check_sds(PMEMpoolcheck *ppc)
{
	LOG(3, NULL);

	const unsigned nreplicas = ppc->pool->set_file->poolset->nreplicas;
	location *loc = check_get_step_data(ppc->data);

	if (!loc->init_done) {
		sds_get_healthy_replicas_num(ppc, loc);

		if (loc->healthy_replicas == nreplicas) {
			/* all replicas have healthy shutdown state */
			/* print summary */
			for (; loc->replica < nreplicas; loc->replica++) {
				init_prefix(loc);
				CHECK_INFO(ppc, "%s" SDS_CHECK_STR,
						loc->prefix);
				CHECK_INFO(ppc, "%s" SDS_OK_STR, loc->prefix);
			}
			return;
		} else if (loc->healthy_replicas > 0) {
			ppc->sync_required = true;
			return;
		}
		loc->init_done = true;
	}

	/* produce single healthy replica */
	loc->part = 0;
	for (; loc->replica < nreplicas; loc->replica++) {
		init_location_data(ppc, loc);

		while (CHECK_NOT_COMPLETE(loc, steps)) {
			ASSERT(loc->step < ARRAY_SIZE(steps));
			if (step_exe(ppc, loc))
				return;
		}

		if (loc->healthy_replicas)
			break;
	}

	if (loc->healthy_replicas == 0) {
		ppc->result = CHECK_RESULT_NOT_CONSISTENT;
		CHECK_ERR(ppc, "cannot complete repair, reverting changes");
	} else if (loc->healthy_replicas < nreplicas) {
		ppc->sync_required = true;
	}
}
