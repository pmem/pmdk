// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * badblocks_none.c -- fake bad blocks functions
 */

#include "out.h"
#include "os.h"
#include "badblocks.h"

/*
 * badblocks_files_namespace_badblocks --
 *                      fake badblocks_files_namespace_badblocks()
 */
int
badblocks_files_namespace_badblocks(const char *path, struct badblocks *bbs)
{
	LOG(3, "path %s", path);

	os_stat_t st;

	if (os_stat(path, &st)) {
		ERR("!stat %s", path);
		return -1;
	}

	return 0;
}

/*
 * badblocks_devdax_clear_badblocks -- fake bad block clearing routine
 */
int
badblocks_devdax_clear_badblocks(const char *path, struct badblocks *bbs)
{
	LOG(3, "path %s badblocks %p", path, bbs);

	return 0;
}

/*
 * badblocks_devdax_clear_badblocks_all -- fake bad block clearing routine
 */
int
badblocks_devdax_clear_badblocks_all(const char *path)
{
	LOG(3, "path %s", path);

	return 0;
}
