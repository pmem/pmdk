// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2022, Intel Corporation */

/*
 * badblocks_none.c -- fake bad blocks functions
 */

#include <errno.h>

#include "libpmem2.h"
#include "out.h"

/*
 * pmem2_badblock_context_new -- allocate and create a new bad block context
 */
int
pmem2_badblock_context_new(struct pmem2_badblock_context **bbctx,
	const struct pmem2_source *src)
{
	SUPPRESS_UNUSED(bbctx, src);
	return PMEM2_E_NOSUPP;
}

/*
 * pmem2_badblock_context_delete -- delete and free the bad block context
 */
void
pmem2_badblock_context_delete(
	struct pmem2_badblock_context **bbctx)
{
	SUPPRESS_UNUSED(bbctx);
}

/*
 * pmem2_badblock_next -- get the next bad block
 */
int
pmem2_badblock_next(struct pmem2_badblock_context *bbctx,
	struct pmem2_badblock *bb)
{
	return PMEM2_E_NOSUPP;
}

/*
 * pmem2_badblock_clear -- clear one bad block
 */
int
pmem2_badblock_clear(struct pmem2_badblock_context *bbctx,
			const struct pmem2_badblock *bb)
{
	SUPPRESS_UNUSED(bbctx, bb);
	return PMEM2_E_NOSUPP;
}
