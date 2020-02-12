// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2020, Intel Corporation */

/*
 * pmemcommon.h -- definitions for "common" module
 */

#ifndef PMEMCOMMON_H
#define PMEMCOMMON_H 1

#include "mmap.h"
#include "pmemcore.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline void
common_init(const char *log_prefix, const char *log_level_var,
		const char *log_file_var, int major_version,
		int minor_version)
{
	core_init(log_prefix, log_level_var, log_file_var, major_version,
		minor_version);
	util_mmap_init();
}

static inline void
common_fini(void)
{
	util_mmap_fini();
	core_fini();
}

#ifdef __cplusplus
}
#endif

#endif
