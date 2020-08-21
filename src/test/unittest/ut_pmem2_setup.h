/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020, Intel Corporation */

/*
 * ut_pmem2_setup.h -- libpmem2 setup functions using non-public API
 * (only for unit tests)
 */

#ifndef UT_PMEM2_SETUP_H
#define UT_PMEM2_SETUP_H 1

#include "ut_fh.h"

void ut_pmem2_prepare_config(struct pmem2_config *cfg,
	struct pmem2_source **src, struct FHandle **fh,
	enum file_handle_type fh_type, const char *path, size_t length,
	size_t offset, int access);

#endif /* UT_PMEM2_SETUP_H */
