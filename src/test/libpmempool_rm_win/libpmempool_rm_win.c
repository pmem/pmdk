// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2022, Intel Corporation */

/*
 * libpmempool_rm_win -- a unittest for pmempool_rm.
 *
 */

#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include "unittest.h"

#define FATAL_USAGE(n) UT_FATAL("usage: %s [-f -l -r] path..", (n))

static PMEMobjpool *Pop;

int
wmain(int argc, wchar_t *argv[])
{
	STARTW(argc, argv, "libpmempool_rm_win");
	if (argc < 2)
		FATAL_USAGE(ut_toUTF8(argv[0]));

	unsigned flags = 0;
	int do_open = 0;
	int i = 1;
	for (; i < argc - 1; i++) {
		wchar_t *optarg = argv[i + 1];
		if (wcscmp(L"-f", argv[i]) == 0)
			flags |= PMEMPOOL_RM_FORCE;
		else if (wcscmp(L"-l", argv[i]) == 0)
			flags |= PMEMPOOL_RM_POOLSET_LOCAL;
		else if (wcscmp(L"-o", argv[i]) == 0)
			do_open = 1;
		else if (wcschr(argv[i], L'-') == argv[i])
			FATAL_USAGE(argv[0]);
		else
			break;
	}

	for (; i < argc; i++) {
		const wchar_t *path = argv[i];
		if (do_open) {
			Pop = pmemobj_openW(path, NULL);
			UT_ASSERTne(Pop, NULL);
		}
		int ret = pmempool_rmW(path, flags);
		if (ret) {
			UT_OUT("!%s: %s", ut_toUTF8(path),
				pmempool_errormsgU());
		}

		if (do_open) {
			UT_ASSERTne(Pop, NULL);
			pmemobj_close(Pop);
		}
	}

	DONEW(NULL);
}
