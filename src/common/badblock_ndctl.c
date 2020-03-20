// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * badblock_ndctl.c - implementation of the bad block API using ndctl library
 */

#include "os_badblock.h"
#include "out.h"
#include "../libpmem2/badblocks.h"

/*
 * badblocks_check_file -- check if the file contains bad blocks
 *
 * Return value:
 * -1 : an error
 *  0 : no bad blocks
 *  1 : bad blocks detected
 */
int
badblocks_check_file(const char *file)
{
	LOG(3, "file %s", file);

	long bbsc = badblocks_count(file);
	if (bbsc < 0) {
		LOG(1, "counting bad blocks failed -- '%s'", file);
		return -1;
	}

	if (bbsc > 0) {
		LOG(1, "pool file '%s' contains %li bad block(s)", file, bbsc);
		return 1;
	}

	return 0;
}
