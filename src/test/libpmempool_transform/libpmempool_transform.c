// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * libpmempool_transform -- a unittest for libpmempool transform.
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
	START(argc, argv, "libpmempool_transform");
	if (argc != 4)
		UT_FATAL("usage: %s poolset_in poolset_out flags", argv[0]);

	int ret = pmempool_transform(argv[1], argv[2],
			(unsigned)strtoul(argv[3], NULL, 0));
	if (ret)
		UT_OUT("result: %d, errno: %d", ret, errno);
	else
		UT_OUT("result: %d", ret);

	DONE(NULL);
}
