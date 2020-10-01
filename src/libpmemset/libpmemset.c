// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * libpmemset.c -- pmemset library constructor & destructor
 */

#include "libpmemset.h"

#include "out.h"
#include "pmemset.h"
#include "util.h"

/*
 * libpmemset_init -- load-time initialization for libpmemset
 *
 * Called automatically by the run-time loader.
 */
ATTR_CONSTRUCTOR
void
libpmemset_init(void)
{
	util_init();
	out_init(PMEMSET_LOG_PREFIX, PMEMSET_LOG_LEVEL_VAR,
			PMEMSET_LOG_FILE_VAR, PMEMSET_MAJOR_VERSION,
			PMEMSET_MINOR_VERSION);

	LOG(3, NULL);
}

/*
 * libpmemset_fini -- libpmemset cleanup routine
 *
 * Called automatically when the process terminates.
 */
ATTR_DESTRUCTOR
void
libpmemset_fini(void)
{
	LOG(3, NULL);

	out_fini();
}
