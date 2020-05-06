// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * ut_setup.h -- utility helper functions for libpmem2 tests setup
 */
#include "../../libpmem2/config.h"
#include "ut_fh.h"
#include "ut_pmem2.h"
#include "ut_setup.h"
#include "unittest.h"

/*
 * ut_prepare_config -- fill pmem2_config, this function can not set
 * the wrong value
 */
void
ut_prepare_config_internal(struct pmem2_config *cfg, struct pmem2_source **src,
	struct FHandle **fh, enum file_handle_type fh_type, const char *file,
	size_t length, size_t offset, int access)
{
	*fh = UT_FH_OPEN(fh_type, file, access);

	pmem2_config_init(cfg);
	cfg->offset = offset;
	cfg->length = length;
	cfg->requested_max_granularity = PMEM2_GRANULARITY_PAGE;

	PMEM2_SOURCE_FROM_FH(src, *fh);
}
