/*
 * Copyright 2018, Intel Corporation
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

#define ADR_FAILURE_STR \
	"an ADR failure was detected - your pool might be corrupted."

#define RESET_SDS_STR \
	"Do you want to reset shutdown state at your own risk? " \
	"If you have more then one replica you may want to" \
	"synchronize your pool after this operation."

/*
 * sds_replica_check -- (internal) check if replica is healthy
 */
static int
sds_replica_check(location *loc)
{
	LOG(3, NULL);

	struct pool_replica *rep = REP(loc->set, loc->replica);

	if (rep->remote)
		return 0;

	/* make a copy of sds as we shouldn't modify a pool */
	struct shutdown_state old_sds = loc->hdr.sds;
	struct shutdown_state curr_sds;

	shutdown_state_init(&curr_sds, NULL);

	/* get current shutdown state */
	for (unsigned p = 0; p < rep->nparts; ++p) {
		shutdown_state_add_part(&curr_sds, PART(rep, p)->path, NULL);
	}

	/* compare current and old shutdown state */
	if (!shutdown_state_check(&curr_sds, &old_sds, NULL)) {
		return 0; /* replica is healthy */
	}

	return -1;
}

/*
 * sds_check -- (internal) check shutdown_state
 */
static int
sds_check(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	ASSERTeq(loc->part, 0);

	CHECK_INFO(ppc, "%schecking shutdown state", loc->prefix);

	/* shutdown state is valid */
	if (!sds_replica_check(loc)) {
		CHECK_INFO(ppc, "%sshutdown state correct", loc->prefix);
		loc->step = CHECK_STEP_COMPLETE;
		return 0;
	}

	/* shutdown state is NOT valid and can NOT be repaired */
	if (CHECK_IS_NOT(ppc, REPAIR)) {
		check_end(ppc->data);
		ppc->result = CHECK_RESULT_NOT_CONSISTENT;
		return CHECK_ERR(ppc, "%s" ADR_FAILURE_STR, loc->prefix);
	}

	/* shutdown state is NOT valid but can be repaired */
	CHECK_ASK(ppc, Q_RESET_SDS, "%s" ADR_FAILURE_STR "|" RESET_SDS_STR,
			loc->prefix);
	return check_questions_sequence_validate(ppc);
}

/*
 * sds_fix -- (internal) fix shutdown state
 */
static int
sds_fix(PMEMpoolcheck *ppc, location *loc, uint32_t question,
	void *context)
{
	LOG(3, NULL);

	switch (question) {
	case Q_RESET_SDS:
		CHECK_INFO(ppc, "%sresetting pool_hdr.sds", loc->prefix);
		memset(&loc->hdr.sds, 0, sizeof(loc->hdr.sds));
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
step_exe(PMEMpoolcheck *ppc, const struct step *steps, location *loc)
{
	const struct step *step = &steps[loc->step++];

	if (!step->fix)
		return step->check(ppc, loc);

	if (!check_has_answer(ppc->data))
		return 0;

	if (check_answer_loop(ppc, loc, NULL, 1 /* fail on no */, step->fix))
		return -1;

	util_convert2le_hdr(&loc->hdr);
	memcpy(loc->hdrp, &loc->hdr, sizeof(loc->hdr));
	util_persist_auto(loc->is_dev_dax, loc->hdrp, sizeof(*loc->hdrp));

	util_convert2h_hdr_nocheck(&loc->hdr);
	loc->pool_hdr_modified = 1;

	return 0;
}

/*
 * init_location_data -- (internal) prepare location information
 */
static void
init_location_data(PMEMpoolcheck *ppc, location *loc)
{
	loc->set = ppc->pool->set_file->poolset;

	/* prepare prefix for messages */
	if (ppc->result != CHECK_RESULT_PROCESS_ANSWERS) {
		if (loc->set->nreplicas > 1) {
			int ret = snprintf(loc->prefix, PREFIX_MAX_SIZE,
				"replica %u: ",
				loc->replica);
			if (ret < 0 || ret >= PREFIX_MAX_SIZE)
				FATAL("!snprintf");
		} else
			loc->prefix[0] = '\0';
		loc->step = 0;
	}

	struct pool_replica *rep = REP(loc->set, loc->replica);
	loc->hdrp = HDR(rep, loc->part);
	memcpy(&loc->hdr, loc->hdrp, sizeof(loc->hdr));
	util_convert2h_hdr_nocheck(&loc->hdr);
	loc->is_dev_dax = PART(rep, 0)->is_dev_dax;
}

/*
 * check_sds -- entry point for shutdown state checks
 */
void
check_sds(PMEMpoolcheck *ppc)
{
	LOG(3, NULL);

	location *loc = check_get_step_data(ppc->data);
	unsigned nreplicas = ppc->pool->set_file->poolset->nreplicas;

	for (; loc->replica < nreplicas; loc->replica++) {
		loc->part = 0;
		init_location_data(ppc, loc);

		while (CHECK_NOT_COMPLETE(loc, steps)) {
			ASSERT(loc->step < ARRAY_SIZE(steps));
			if (step_exe(ppc, steps, loc))
				return;
		}
	}
}
