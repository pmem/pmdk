/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020-2024, Intel Corporation */

/*
 * pmemcore.h -- definitions for "core" module
 */

#ifndef PMEMCORE_H
#define PMEMCORE_H 1

#include "util.h"
#include "out.h"
#include "last_error_msg.h"
#include "log_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * core_init -- core module initialization
 */
static inline void
core_init(const char *log_prefix, const char *log_level_var,
		const char *log_file_var, int major_version,
		int minor_version)
{
	util_init();
	core_log_init();
	out_init(log_prefix, log_level_var, log_file_var, major_version,
		minor_version);
}

/*
 * core_fini -- core module cleanup
 */
static inline void
core_fini(void)
{
	out_fini();
	core_log_fini();
	last_error_msg_fini();
}

#ifdef __cplusplus
}
#endif

#endif
