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
 * check_backup.c -- pre-check backup
 */

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include "out.h"
#include "libpmempool.h"
#include "pmempool.h"
#include "pool.h"
#include "check_util.h"

/*
 * check_backup -- perform backup if requested and needed
 */
void
check_backup(PMEMpoolcheck *ppc)
{
	LOG(3, "backup_path %s", ppc->backup_path);

	if (CHECK_WITHOUT_FIXING(ppc))
		return;

	if (ppc->backup_path == NULL)
		return;

	if (ppc->pool->params.is_poolset) {
		if (ppc->pool->set_file->poolset->nreplicas > 1) {
			CHECK_INFO(ppc, "only the first replica will be backed "
				"up");
		}

		struct pool_set *set = NULL;
		if (pool_set_parse(&set, ppc->backup_path)) {
			CHECK_INFO(ppc, "invalid poolset backup file: %s",
				ppc->backup_path);
			goto err_poolset;
		}

		if (set->nreplicas > 1) {
			CHECK_INFO(ppc, "backup to a poolset with multiple "
				"replicas is not supported");
			goto err_poolset;
		}

		ASSERTeq(set->nreplicas, 1);
		struct pool_replica *srep =
			ppc->pool->set_file->poolset->replica[0];
		struct pool_replica *drep = set->replica[0];
		if (srep->nparts != drep->nparts) {
			CHECK_INFO(ppc, "number of parts in the backup poolset "
				"must match number of parts in the source "
				"poolset");
			goto err_poolset;
		}

		for (unsigned p = 0; p < srep->nparts; p++) {
			if (srep->part[p].filesize != drep->part[p].filesize) {
				CHECK_INFO(ppc, "size of the part %u of the "
					"backup poolset does not match source "
					"poolset", p);
				goto err_poolset;
			}

			if (!access(drep->part[p].path, F_OK)) {
				CHECK_INFO(ppc, "unable to backup to the "
					"poolset with already existing parts");
				goto err_poolset;
			}

			errno = 0;
		}

		for (unsigned p = 0; p < srep->nparts; p++) {
			CHECK_INFO(ppc, "creating backup file: %s",
				drep->part[p].path);
			if (pool_set_part_copy(
					&drep->part[p], &srep->part[p])) {
				CHECK_INFO(ppc, "unable to create backup file");
				goto err_poolset;
			}
		}
	} else {
		CHECK_INFO(ppc, "creating backup file: %s", ppc->backup_path);
		if (pool_copy(ppc->pool, ppc->backup_path)) {
			CHECK_ERR(ppc, "unable to create backup file");
			ppc->result = CHECK_RESULT_ERROR;
		}
	}

	return;

err_poolset:
	CHECK_ERR(ppc, "unable to backup poolset");
	ppc->result = CHECK_RESULT_ERROR;
}
