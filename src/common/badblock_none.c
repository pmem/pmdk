// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * badblock_none.c - fake bad block API
 */

#include <errno.h>

#include "os_badblock.h"
#include "out.h"

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

	/* not supported */
	errno = ENOTSUP;
	return -1;
}
