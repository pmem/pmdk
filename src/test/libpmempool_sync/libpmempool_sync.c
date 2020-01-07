// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2019, Intel Corporation */

/*
 * libpmempool_sync -- a unittest for libpmempool sync.
 *
 */

#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "unittest.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "libpmempool_sync");
	if (argc != 3)
		UT_FATAL("usage: %s poolset_file flags", argv[0]);

	int ret = pmempool_sync(argv[1], (unsigned)strtoul(argv[2], NULL, 0));
	if (ret)
		UT_OUT("result: %d, errno: %d", ret, errno);
	else
		UT_OUT("result: %d", ret);

	DONE(NULL);
}
