// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2024, Intel Corporation */

/*
 * badblocks.c -- implementation of common bad blocks API
 */

#include "libpmem2.h"
#include "pmem2_utils.h"
#include "bad_blocks.h"
#include "badblocks.h"
#include "alloc.h"
#include "out.h"
#include "log_internal.h"

/*
 * badblocks_new -- zalloc bad blocks structure
 */
struct badblocks *
badblocks_new(void)
{
	LOG(3, " ");

	struct badblocks *bbs = Zalloc(sizeof(struct badblocks));
	if (bbs == NULL) {
		ERR_W_ERRNO("Zalloc");
	}

	return bbs;
}

/*
 * badblocks_delete -- free bad blocks structure
 */
void
badblocks_delete(struct badblocks *bbs)
{
	LOG(3, "badblocks %p", bbs);

	if (bbs == NULL)
		return;

	Free(bbs->bbv);
	Free(bbs);
}

/*
 * pmem2_badblock_next -- get the next bad block
 */
int
pmem2_badblock_next(struct pmem2_badblock_context *bbctx,
			struct pmem2_badblock *bb)
{
	LOG(3, "bbctx %p bb %p", bbctx, bb);
	PMEM2_ERR_CLR();

	ASSERTne(bbctx, NULL);
	ASSERTne(bb, NULL);

	int ret = pmem2_badblock_next_internal(bbctx, bb);

	if (ret == ENODEV) {
		ERR_WO_ERRNO(
			"Cannot find any matching device, no bad blocks found");
		ret = PMEM2_E_NO_BAD_BLOCK_FOUND;
	}

	return ret;
}
