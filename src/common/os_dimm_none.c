// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * os_dimm_none.c -- fake dimm functions
 */

#include "out.h"
#include "os.h"
#include "os_dimm.h"
/*
 * os_dimm_uid -- returns empty uid
 */
int
os_dimm_uid(const char *path, char *uid, size_t *len)
{
	LOG(3, "path %s, uid %p, len %lu", path, uid, *len);
	if (uid == NULL) {
		*len = 1;
	} else {
		*uid = '\0';
	}
	return 0;
}

/*
 * os_dimm_usc -- returns fake unsafe shutdown count
 */
int
os_dimm_usc(const char *path, uint64_t *usc)
{
	LOG(3, "path %s, usc %p", path, usc);
	*usc = 0;
	return 0;
}

/*
 * os_dimm_files_namespace_badblocks -- fake os_dimm_files_namespace_badblocks()
 */
int
os_dimm_files_namespace_badblocks(const char *path, struct badblocks *bbs)
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
 * os_dimm_devdax_clear_badblocks -- fake bad block clearing routine
 */
int
os_dimm_devdax_clear_badblocks(const char *path, struct badblocks *bbs)
{
	LOG(3, "path %s badblocks %p", path, bbs);

	return 0;
}

/*
 * os_dimm_devdax_clear_badblocks_all -- fake bad block clearing routine
 */
int
os_dimm_devdax_clear_badblocks_all(const char *path)
{
	LOG(3, "path %s", path);

	return 0;
}
