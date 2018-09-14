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

/*
 * pool_hdr_valid -- (internal) return true if pool header is valid
 */
static int
pool_hdr_valid(struct pool_hdr *hdrp)
{
	return !util_is_zeroed((void *)hdrp, sizeof(*hdrp)) &&
		util_checksum(hdrp, sizeof(*hdrp), &hdrp->checksum, 0,
			POOL_HDR_CSUM_END_OFF);
}

/*
 * check_shutdown_state -- (internal) check if poolset has healthy replica
 */
static int
check_shutdown_state(struct pool_set *set)
{
	LOG(3, "set %p", set);

	if (set == NULL)
		return 0; /* skip */

	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		struct pool_hdr *hdrp = HDR(rep, 0);

		if (rep->remote)
			continue;

		struct shutdown_state curr_sds;
		shutdown_state_init(&curr_sds, NULL);
		for (unsigned p = 0; p < rep->nparts; ++p) {
			if (shutdown_state_add_part(&curr_sds,
					PART(rep, p)->path, NULL))
				return -1;
		}
		/* make a copy of sds as we shouldn't modify a pool */
		struct shutdown_state pool_sds = hdrp->sds;

		if (!shutdown_state_check(&curr_sds, &pool_sds, NULL)) {
			return 0; /* healthy replica found */
		}

	}

	return -1;
}

/*
 * shutdown_state_preliminary_checks -- (internal) check shutdown_state
 */
static int
shutdown_state_preliminary_check(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	CHECK_INFO(ppc, "%schecking shutdown state", loc->prefix);

	if (check_shutdown_state(loc->set)) {
		if (CHECK_IS_NOT(ppc, REPAIR)) {
			check_end(ppc->data);
			ppc->result = CHECK_RESULT_NOT_CONSISTENT;
			return CHECK_ERR(ppc,
				"%san ADR failure was detected - your pool might be corrupted",
				loc->prefix);
		}
	} else {
		/* valid check sum */
		CHECK_INFO(ppc, "%sshutdown state correct",
			loc->prefix);
		loc->step = CHECK_STEP_COMPLETE;
		return 0;
	}

	ASSERT(CHECK_IS(ppc, REPAIR));

	return 0;
}

/*
 * shutdown_state_sds_check -- (internal) check shutdown state
 */
static int
shutdown_state_sds_check(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	if (loc->part == 0 && check_shutdown_state(loc->set)) {
		CHECK_ASK(ppc, Q_RESET_SDS,
			"An ADR failure was detected - your pool might be corrupted.|"
			"Do you want to reset shutdown state for replica: %u to be able to open pool on your own risk? "
			"If you have more then one replica you will have to synchronize your pool after this operation",
			loc->replica);
	}

	return check_questions_sequence_validate(ppc);
}

/*
 * shutdown_state_sds_fix -- (internal) fix shutdown state
 */
static int
shutdown_state_sds_fix(PMEMpoolcheck *ppc, location *loc, uint32_t question,
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

static const struct step steps_initial[] = {
	{
		.check	= shutdown_state_preliminary_check,
	},
	{
		.check	= shutdown_state_sds_check,
	},
	{
		.fix	= shutdown_state_sds_fix,
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

	if (check_answer_loop(ppc, loc, NULL, 0, step->fix))
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
				"replica %u: ",
				loc->replica);
			if (ret < 0 || ret >= PREFIX_MAX_SIZE)
				FATAL("!snprintf");
		} else
			loc->prefix[0] = '\0';
		loc->step = 0;
	}

	loc->set = ppc->pool->set_file->poolset;
	struct pool_replica *rep = REP(loc->set, loc->replica);
	loc->hdrp = HDR(rep, loc->part);
	memcpy(&loc->hdr, loc->hdrp, sizeof(loc->hdr));
	util_convert2h_hdr_nocheck(&loc->hdr);
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
	struct pool_set *poolset = ppc->pool->set_file->poolset;

	for (; loc->replica < nreplicas; loc->replica++) {
		struct pool_replica *rep = poolset->replica[loc->replica];
		loc->part = 0;
		init_location_data(ppc, loc);

		/* do all checks */
		while (CHECK_NOT_COMPLETE(loc, steps_initial)) {
			ASSERT(loc->step < ARRAY_SIZE(steps_initial));
			if (step_exe(ppc, steps_initial, loc, rep,
					nreplicas))
				return;
		}
	}

	if (check_shutdown_state(ppc->pool->set_file->poolset)) {
		ppc->result = CHECK_RESULT_NOT_CONSISTENT;
		CHECK_ERR(ppc, "cannot complete repair, reverting changes");
		return;
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
