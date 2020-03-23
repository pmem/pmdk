// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * badblock_none.c - fake bad block API
 */

#include <errno.h>

#include "out.h"
#include "os_badblock.h"

/*
 * os_badblocks_check_file -- check if the file contains bad blocks
 *
 * Return value:
 * -1 : an error
 *  0 : no bad blocks
 *  1 : bad blocks detected
 */
int
os_badblocks_check_file(const char *file)
{
	LOG(3, "file %s", file);

	/* not supported */
	errno = ENOTSUP;
	return -1;
}

/*
 * os_badblocks_clear -- clears the given bad blocks in a file
 *                       (regular file or dax device)
 */
int
os_badblocks_clear(const char *file, struct badblocks *bbs)
{
	LOG(3, "file %s badblocks %p", file, bbs);

	/* not supported */
	errno = ENOTSUP;
	return -1;
}

/*
 * os_badblocks_clear_all -- clears all bad blocks in a file
 *                           (regular file or dax device)
 */
int
os_badblocks_clear_all(const char *file)
{
	LOG(3, "file %s", file);

	/* not supported */
	errno = ENOTSUP;
	return -1;
}
