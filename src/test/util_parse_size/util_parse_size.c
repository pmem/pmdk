// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016, Intel Corporation */

/*
 * util_parse_size.c -- unit test for parsing a size
 */

#include "unittest.h"
#include "util.h"
#include <inttypes.h>

int
main(int argc, char *argv[])
{
	int ret = 0;
	uint64_t size = 0;

	START(argc, argv, "util_parse_size");

	for (int arg = 1; arg < argc; ++arg) {
		ret = util_parse_size(argv[arg], &size);
		if (ret == 0) {
			UT_OUT("%s - correct %"PRIu64, argv[arg], size);
		} else {
			UT_OUT("%s - incorrect", argv[arg]);
		}
	}

	DONE(NULL);
}
