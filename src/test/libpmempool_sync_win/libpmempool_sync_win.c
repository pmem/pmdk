// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * libpmempool_sync_win -- a unittest for libpmempool sync.
 *
 */

#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "unittest.h"

int
wmain(int argc, wchar_t *argv[])
{
	STARTW(argc, argv, "libpmempool_sync_win");
	if (argc != 3)
		UT_FATAL("usage: %s poolset_file flags", ut_toUTF8(argv[0]));

	int ret = pmempool_syncW(argv[1], (unsigned)wcstoul(argv[2], NULL, 0));

	if (ret)
		UT_OUT("result: %d, errno: %d", ret, errno);
	else
		UT_OUT("result: 0");

	DONEW(NULL);
}
