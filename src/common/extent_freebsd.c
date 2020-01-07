// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * extent_freebsd.c - implementation of the FreeBSD fs extent query API
 * XXX THIS IS CURRENTLY A DUMMY MODULE.
 */

#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "file.h"
#include "out.h"
#include "extent.h"

/*
 * os_extents_count -- get number of extents of the given file
 *                     (and optionally read its block size)
 */
long
os_extents_count(const char *path, struct extents *exts)
{
	LOG(3, "path %s extents %p", path, exts);

	return -1;
}

/*
 * os_extents_get -- get extents of the given file
 *                   (and optionally read its block size)
 */
int
os_extents_get(const char *path, struct extents *exts)
{
	LOG(3, "path %s extents %p", path, exts);

	return -1;
}
