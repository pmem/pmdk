// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2021, Intel Corporation */

/*
 * check_backup.c -- pre-check backup
 */

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include "out.h"
#include "file.h"
#include "os.h"
#include "libpmempool.h"
#include "pmempool.h"
#include "pool.h"
#include "check_util.h"

enum question {
	Q_OVERWRITE_EXISTING_FILE,
	Q_OVERWRITE_EXISTING_PARTS
};

/*
 * location_release -- (internal) release poolset structure
 */
static void
location_release(location *loc)
{
	if (loc->set) {
		util_poolset_free(loc->set);
		loc->set = NULL;
	}
}

/*
 * backup_nonpoolset_requirements -- (internal) check backup requirements
 */
static int
backup_nonpoolset_requirements(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, "backup_path %s", ppc->backup_path);

	int exists = util_file_exists(ppc->backup_path);
	if (exists < 0) {
		return CHECK_ERR(ppc,
				"unable to access the backup destination: %s",
				ppc->backup_path);
	}

	if (!exists) {
		errno = 0;
		return 0;
	}

	if ((size_t)util_file_get_size(ppc->backup_path) !=
			ppc->pool->set_file->size) {
		ppc->result = CHECK_RESULT_ERROR;
		return CHECK_ERR(ppc,
			"destination of the backup does not match the size of the source pool file: %s",
			ppc->backup_path);
	}

	if (CHECK_WITHOUT_FIXING(ppc)) {
		location_release(loc);
		loc->step = CHECK_STEP_COMPLETE;
		return 0;
	}

	CHECK_ASK(ppc, Q_OVERWRITE_EXISTING_FILE,
		"destination of the backup already exists.|Do you want to overwrite it?");

	return check_questions_sequence_validate(ppc);
}

/*
 * backup_nonpoolset_overwrite -- (internal) overwrite pool
 */
static int
backup_nonpoolset_overwrite(PMEMpoolcheck *ppc, location *loc,
	uint32_t question, void *context)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(context);

	LOG(3, NULL);

	ASSERTne(loc, NULL);

	switch (question) {
	case Q_OVERWRITE_EXISTING_FILE:
		if (pool_copy(ppc->pool, ppc->backup_path, 1 /* overwrite */)) {
			location_release(loc);
			ppc->result = CHECK_RESULT_ERROR;
			return CHECK_ERR(ppc, "cannot perform backup");
		}

		location_release(loc);
		loc->step = CHECK_STEP_COMPLETE;
		return 0;
	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

/*
 * backup_nonpoolset_create -- (internal) create backup
 */
static int
backup_nonpoolset_create(PMEMpoolcheck *ppc, location *loc)
{
	CHECK_INFO(ppc, "creating backup file: %s", ppc->backup_path);

	if (pool_copy(ppc->pool, ppc->backup_path, 0)) {
		location_release(loc);
		ppc->result = CHECK_RESULT_ERROR;
		return CHECK_ERR(ppc, "cannot perform backup");
	}

	location_release(loc);
	loc->step = CHECK_STEP_COMPLETE;
	return 0;
}

/*
 * backup_poolset_requirements -- (internal) check backup requirements
 */
static int
backup_poolset_requirements(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, "backup_path %s", ppc->backup_path);

	if (ppc->pool->set_file->poolset->nreplicas > 1) {
		CHECK_INFO(ppc,
			"backup of a poolset with multiple replicas is not supported");
		goto err;
	}

	if (pool_set_parse(&loc->set, ppc->backup_path)) {
		CHECK_INFO_ERRNO(ppc, "invalid poolset backup file: %s",
			ppc->backup_path);
		goto err;
	}

	if (loc->set->nreplicas > 1) {
		CHECK_INFO(ppc,
			"backup to a poolset with multiple replicas is not supported");
		goto err_poolset;
	}

	ASSERTeq(loc->set->nreplicas, 1);
	struct pool_replica *srep = ppc->pool->set_file->poolset->replica[0];
	struct pool_replica *drep = loc->set->replica[0];
	if (srep->nparts != drep->nparts) {
		CHECK_INFO(ppc,
			"number of part files in the backup poolset must match number of part files in the source poolset");
		goto err_poolset;
	}

	int overwrite_required = 0;
	for (unsigned p = 0; p < srep->nparts; p++) {
		int exists = util_file_exists(drep->part[p].path);
		if (exists < 0) {
			CHECK_INFO(ppc,
				"unable to access the part of the destination poolset: %s",
				ppc->backup_path);
			goto err_poolset;
		}

		if (srep->part[p].filesize != drep->part[p].filesize) {
			CHECK_INFO(ppc,
				"size of the part %u of the backup poolset does not match source poolset",
				p);
			goto err_poolset;
		}

		if (!exists) {
			errno = 0;
			continue;
		}

		overwrite_required = true;

		if ((size_t)util_file_get_size(drep->part[p].path) !=
				srep->part[p].filesize) {
			CHECK_INFO(ppc,
				"destination of the backup part does not match size of the source part file: %s",
				drep->part[p].path);
			goto err_poolset;
		}
	}

	if (CHECK_WITHOUT_FIXING(ppc)) {
		location_release(loc);
		loc->step = CHECK_STEP_COMPLETE;
		return 0;
	}

	if (overwrite_required) {
		CHECK_ASK(ppc, Q_OVERWRITE_EXISTING_PARTS,
			"part files of the destination poolset of the backup already exist.|"
			"Do you want to overwrite them?");
	}

	return check_questions_sequence_validate(ppc);

err_poolset:
	location_release(loc);
err:
	ppc->result = CHECK_RESULT_ERROR;
	return CHECK_ERR(ppc, "unable to backup poolset");
}

/*
 * backup_poolset -- (internal) backup the poolset
 */
static int
backup_poolset(PMEMpoolcheck *ppc, location *loc, int overwrite)
{
	struct pool_replica *srep = ppc->pool->set_file->poolset->replica[0];
	struct pool_replica *drep = loc->set->replica[0];
	for (unsigned p = 0; p < srep->nparts; p++) {
		if (overwrite == 0) {
			CHECK_INFO(ppc, "creating backup file: %s",
				drep->part[p].path);
		}
		if (pool_set_part_copy(&drep->part[p], &srep->part[p],
				overwrite)) {
			location_release(loc);
			ppc->result = CHECK_RESULT_ERROR;
			CHECK_INFO(ppc, "unable to create backup file");
			return CHECK_ERR(ppc, "unable to backup poolset");
		}
	}

	return 0;
}

/*
 * backup_poolset_overwrite -- (internal) backup poolset with overwrite
 */
static int
backup_poolset_overwrite(PMEMpoolcheck *ppc, location *loc,
	uint32_t question, void *context)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(context);

	LOG(3, NULL);

	ASSERTne(loc, NULL);

	switch (question) {
	case Q_OVERWRITE_EXISTING_PARTS:
		if (backup_poolset(ppc, loc, 1 /* overwrite */)) {
			location_release(loc);
			ppc->result = CHECK_RESULT_ERROR;
			return CHECK_ERR(ppc, "cannot perform backup");
		}

		location_release(loc);
		loc->step = CHECK_STEP_COMPLETE;
		return 0;
	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

/*
 * backup_poolset_create -- (internal) backup poolset
 */
static int
backup_poolset_create(PMEMpoolcheck *ppc, location *loc)
{
	if (backup_poolset(ppc, loc, 0)) {
		location_release(loc);
		ppc->result = CHECK_RESULT_ERROR;
		return CHECK_ERR(ppc, "cannot perform backup");
	}

	location_release(loc);
	loc->step = CHECK_STEP_COMPLETE;
	return 0;
}

struct step {
	int (*check)(PMEMpoolcheck *, location *);
	int (*fix)(PMEMpoolcheck *, location *, uint32_t, void *);
	int poolset;
};

static const struct step steps[] = {
	{
		.check		= backup_nonpoolset_requirements,
		.poolset	= false,
	},
	{
		.fix		= backup_nonpoolset_overwrite,
		.poolset	= false,
	},
	{
		.check		= backup_nonpoolset_create,
		.poolset	= false
	},
	{
		.check		= backup_poolset_requirements,
		.poolset	= true,
	},
	{
		.fix		= backup_poolset_overwrite,
		.poolset	= true,
	},
	{
		.check		= backup_poolset_create,
		.poolset	= true
	},
	{
		.check		= NULL,
		.fix		= NULL,
	},
};

/*
 * step_exe -- (internal) perform single step according to its parameters
 */
static int
step_exe(PMEMpoolcheck *ppc, location *loc)
{
	ASSERT(loc->step < ARRAY_SIZE(steps));

	const struct step *step = &steps[loc->step++];

	if (step->poolset == 0 && ppc->pool->params.is_poolset == 1)
		return 0;

	if (!step->fix)
		return step->check(ppc, loc);

	if (!check_has_answer(ppc->data))
		return 0;

	if (check_answer_loop(ppc, loc, NULL, 1, step->fix))
		return -1;

	ppc->result = CHECK_RESULT_CONSISTENT;

	return 0;
}

/*
 * check_backup -- perform backup if requested and needed
 */
void
check_backup(PMEMpoolcheck *ppc)
{
	LOG(3, "backup_path %s", ppc->backup_path);

	if (ppc->backup_path == NULL)
		return;

	location *loc = check_get_step_data(ppc->data);

	/* do all checks */
	while (CHECK_NOT_COMPLETE(loc, steps)) {
		if (step_exe(ppc, loc))
			break;
	}
}
