// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2024, Intel Corporation */

/*
 * libpmem.c -- pmem entry points for libpmem
 */

#include <stdio.h>
#include <stdint.h>

#include "libpmem.h"

#include "pmem.h"
#include "pmemcommon.h"

/*
 * libpmem_init -- load-time initialization for libpmem
 *
 * Called automatically by the run-time loader.
 */
ATTR_CONSTRUCTOR
void
libpmem_init(void)
{
	common_init(PMEM_LOG_PREFIX, PMEM_LOG_LEVEL_VAR, PMEM_LOG_FILE_VAR,
			PMEM_MAJOR_VERSION, PMEM_MINOR_VERSION);
	LOG(3, NULL);
	pmem_init();
}

/*
 * libpmem_fini -- libpmem cleanup routine
 *
 * Called automatically when the process terminates.
 */
ATTR_DESTRUCTOR
void
libpmem_fini(void)
{
	LOG(3, NULL);

	common_fini();
}

/*
 * pmem_check_versionU -- see if library meets application version requirements
 */
static inline
const char *
pmem_check_versionU(unsigned major_required, unsigned minor_required)
{
	LOG(3, "major_required %u minor_required %u",
			major_required, minor_required);

	if (major_required != PMEM_MAJOR_VERSION) {
		ERR_WO_ERRNO(
			"libpmem major version mismatch (need %u, found %u)",
			major_required, PMEM_MAJOR_VERSION);
		return last_error_msg_get();
	}

	if (minor_required > PMEM_MINOR_VERSION) {
		ERR_WO_ERRNO(
			"libpmem minor version mismatch (need %u, found %u)",
			minor_required, PMEM_MINOR_VERSION);
		return last_error_msg_get();
	}

	return NULL;
}

/*
 * pmem_check_version -- see if library meets application version requirements
 */
const char *
pmem_check_version(unsigned major_required, unsigned minor_required)
{
	return pmem_check_versionU(major_required, minor_required);
}

/*
 * pmem_errormsgU -- return the last error message
 */
static inline
const char *
pmem_errormsgU(void)
{
	return last_error_msg_get();
}

/*
 * pmem_errormsg -- return the last error message
 */
const char *
pmem_errormsg(void)
{
	return pmem_errormsgU();
}

/*
 * pmem_log_set_threshold -- set the logging threshold value
 */
int
pmem_log_set_threshold(enum pmem_log_threshold threshold,
	enum pmem_log_level value)
{
	int ret = core_log_set_threshold((enum core_log_threshold)threshold,
		(enum core_log_level)value);
	return core_log_error_translate(ret);
}

/*
 * pmem_log_get_threshold -- get the logging level threshold value
 */
int
pmem_log_get_threshold(enum pmem_log_threshold threshold,
	enum pmem_log_level *value)
{
	int ret = core_log_get_threshold((enum core_log_threshold)threshold,
		(enum core_log_level *)value);
	return core_log_error_translate(ret);
}

/*
 * pmem_log_set_function -- set the log function pointer either to
 * a user-provided function pointer or to the default logging function.
 */
int
pmem_log_set_function(pmem_log_function *log_function)
{
	int ret = core_log_set_function((core_log_function *)log_function);
	return core_log_error_translate(ret);
}
