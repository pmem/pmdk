// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * libpmempool_check_version -- a unittest for libpmempool_check_version.
 *
 */

#include "unittest.h"
#include "libpmempool.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "libpmempool_check_version");

	UT_ASSERTne(pmempool_check_version(0, 0), NULL);

	UT_ASSERTne(NULL, pmempool_check_version(PMEMPOOL_MAJOR_VERSION - 1,
		PMEMPOOL_MINOR_VERSION));

	if (PMEMPOOL_MINOR_VERSION > 0) {
		UT_ASSERTeq(NULL, pmempool_check_version(PMEMPOOL_MAJOR_VERSION,
			PMEMPOOL_MINOR_VERSION - 1));
	}

	UT_ASSERTeq(NULL, pmempool_check_version(PMEMPOOL_MAJOR_VERSION,
		PMEMPOOL_MINOR_VERSION));

	UT_ASSERTne(NULL, pmempool_check_version(PMEMPOOL_MAJOR_VERSION + 1,
		PMEMPOOL_MINOR_VERSION));

	UT_ASSERTne(NULL, pmempool_check_version(PMEMPOOL_MAJOR_VERSION,
		PMEMPOOL_MINOR_VERSION + 1));

	DONE(NULL);
}
