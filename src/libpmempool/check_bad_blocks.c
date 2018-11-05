/*
 * Copyright 2016-2018, Intel Corporation
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
#include "badblock.h"

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
