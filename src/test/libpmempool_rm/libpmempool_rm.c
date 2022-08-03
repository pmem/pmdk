// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2022, Intel Corporation */

/*
 * libpmempool_rm -- a unittest for pmempool_rm.
 *
 */

#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include "unittest.h"

#define FATAL_USAGE(n) UT_FATAL("usage: %s [-f -l -o] path..", (n))

static PMEMobjpool *Pop;

int
main(int argc, char *argv[])
{
	START(argc, argv, "libpmempool_rm");
	if (argc < 2)
		FATAL_USAGE(argv[0]);

	unsigned flags = 0;

	char *optstr = "flo";
	int do_open = 0;
	int opt;
	while ((opt = getopt(argc, argv, optstr)) != -1) {
		switch (opt) {
		case 'f':
			flags |= PMEMPOOL_RM_FORCE;
			break;
		case 'l':
			flags |= PMEMPOOL_RM_POOLSET_LOCAL;
			break;
		case 'o':
			do_open = 1;
			break;
		default:
			FATAL_USAGE(argv[0]);
		}
	}

	for (int i = optind; i < argc; i++) {
		const char *path = argv[i];
		if (do_open) {
			Pop = pmemobj_open(path, NULL);
			UT_ASSERTne(Pop, NULL);
		}
		int ret = pmempool_rm(path, flags);
		if (ret) {
			UT_OUT("!%s: %s", path, pmempool_errormsg());
		}

		if (do_open) {
			UT_ASSERTne(Pop, NULL);
			pmemobj_close(Pop);
		}
	}

	DONE(NULL);
}
