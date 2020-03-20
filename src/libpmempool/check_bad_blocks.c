// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2020, Intel Corporation */

/*
 * check_bad_blocks.c -- pre-check bad_blocks
 */

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include "out.h"
#include "libpmempool.h"
#include "pmempool.h"
#include "pool.h"
#include "check_util.h"
#include "os_badblock.h"
#include "set_badblocks.h"

/*
 * check_bad_blocks -- check poolset for bad_blocks
 */
void
check_bad_blocks(PMEMpoolcheck *ppc)
{
	LOG(3, "ppc %p", ppc);

	int ret;

	if (!(ppc->pool->params.features.compat & POOL_FEAT_CHECK_BAD_BLOCKS)) {
		/* skipping checking poolset for bad blocks */
		ppc->result = CHECK_RESULT_CONSISTENT;
		return;
	}

	if (ppc->pool->set_file->poolset) {
		ret = badblocks_check_poolset(ppc->pool->set_file->poolset, 0);
	} else {
		ret = os_badblocks_check_file(ppc->pool->set_file->fname);
	}

	if (ret < 0) {
		if (errno == ENOTSUP) {
			ppc->result = CHECK_RESULT_CANNOT_REPAIR;
			CHECK_ERR(ppc, BB_NOT_SUPP);
			return;
		}

		ppc->result = CHECK_RESULT_ERROR;
		CHECK_ERR(ppc, "checking poolset for bad blocks failed -- '%s'",
				ppc->path);
		return;
	}

	if (ret > 0) {
		ppc->result = CHECK_RESULT_CANNOT_REPAIR;
		CHECK_ERR(ppc,
			"poolset contains bad blocks, use 'pmempool info --bad-blocks=yes' to print or 'pmempool sync --bad-blocks' to clear them");
	}
}
