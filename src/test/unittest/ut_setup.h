// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * ut_setup.h -- utility helper functions for libpmem2 tests setup
 */

#ifndef UT_SETUP_H
#define UT_SETUP_H 1

#include "ut_fh.h"

void ut_prepare_config_internal(struct pmem2_config *cfg,
	struct pmem2_source **src, struct FHandle **fh,
	enum file_handle_type fh_type, const char *path, size_t length,
	size_t offset, int access);

#endif /* UT_SETUP_H */
