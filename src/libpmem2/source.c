// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

#include "source.h"
#include "alloc.h"
#include "libpmem2.h"
#include "out.h"
#include "pmem2.h"
#include "pmem2_utils.h"

int
pmem2_source_from_anon(struct pmem2_source **src)
{
	return PMEM2_E_NOSUPP;
}

int
pmem2_source_delete(struct pmem2_source **src)
{
	Free(*src);
	*src = NULL;
	return 0;
}

int
pmem2_badblock_clear(struct pmem2_badblock_context *bbctx,
	const struct pmem2_badblock *bb)
{
	return PMEM2_E_NOSUPP;
}
