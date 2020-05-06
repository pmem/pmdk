// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * ut_pmem2_setup.h -- libpmem2 setup functions using non-public API
 * (only for unit tests)
 */

#include "../../libpmem2/config.h"
#include "ut_pmem2_source.h"
#include "ut_pmem2_setup.h"
#include "unittest.h"

/*
 * ut_pmem2_prepare_config -- fill pmem2_config, this function can not set
 * the wrong value
 */
void
ut_pmem2_prepare_config(struct pmem2_config *cfg, struct pmem2_source **src,
	struct FHandle **fh, enum file_handle_type fh_type, const char *file,
	size_t length, size_t offset, int access)
{
	pmem2_config_init(cfg);
	cfg->offset = offset;
	cfg->length = length;
	cfg->requested_max_granularity = PMEM2_GRANULARITY_PAGE;

	*fh = UT_FH_OPEN(fh_type, file, access);
	PMEM2_SOURCE_FROM_FH(src, *fh);
}
