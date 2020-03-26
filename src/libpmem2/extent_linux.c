// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * extent_linux.c - implementation of the linux fs extent query API
 */

#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/fiemap.h>

#include "libpmem2.h"
#include "pmem2_utils.h"

#include "file.h"
#include "out.h"
#include "extent.h"
#include "alloc.h"

/*
 * os_extents_common -- (internal) common part of getting extents
 *                      of the given file
 *
 * Returns: number of extents the file consists of or -1 in case of an error.
 * Sets: struct fiemap and struct extents.
 */
static long
os_extents_common(int fd, struct extents *exts, struct fiemap **pfmap)
{
	LOG(3, "fd %i exts %p pfmap %p", fd, exts, pfmap);

	os_stat_t st;
	if (os_fstat(fd, &st) < 0) {
		ERR("!fstat %d", fd);
		return -1;
	}

	enum pmem2_file_type pmem2_type;

	int ret = pmem2_get_type_from_stat(&st, &pmem2_type);
	if (ret) {
		errno = pmem2_err_to_errno(ret);
		return -1;
	}

	/* directories do not have any extents */
	if (pmem2_type == PMEM2_FTYPE_DIR) {
		ERR(
			"checking extents does not make sense in case of directories");
		return -1;
	}

	if (exts->extents_count == 0) {
		LOG(10, "fd %i: block size: %li", fd, (long int)st.st_blksize);
		exts->blksize = (uint64_t)st.st_blksize;
	}

	/* devdax does not have any extents */
	if (pmem2_type == PMEM2_FTYPE_DEVDAX) {
		return 0;
	}

	ASSERTeq(pmem2_type, PMEM2_FTYPE_REG);

	struct fiemap *fmap = Zalloc(sizeof(struct fiemap));
	if (fmap == NULL) {
		ERR("!malloc");
		return -1;
	}

	fmap->fm_start = 0;
	fmap->fm_length = (size_t)st.st_size;
	fmap->fm_flags = 0;
	fmap->fm_extent_count = 0;

	if (ioctl(fd, FS_IOC_FIEMAP, fmap) != 0) {
		ERR("!ioctl %d", fd);
		goto error_free;
	}

	if (exts->extents_count == 0) {
		exts->extents_count = fmap->fm_mapped_extents;
		LOG(4, "fd: %i: number of extents: %u",
			fd, exts->extents_count);

	} else if (exts->extents_count != fmap->fm_mapped_extents) {
		ERR("number of extents differs (was: %u, is: %u)",
			exts->extents_count, fmap->fm_mapped_extents);
		goto error_free;
	}

	*pfmap = fmap;

	return exts->extents_count;

error_free:
	Free(fmap);

	return -1;
}

/*
 * os_extents_count -- get number of extents of the given file
 *                     (and optionally read its block size)
 */
long
os_extents_count(int fd, struct extents *exts)
{
	LOG(3, "fd %i extents %p", fd, exts);

	struct fiemap *fmap = NULL;

	ASSERTne(exts, NULL);
	memset(exts, 0, sizeof(*exts));

	long ret = os_extents_common(fd, exts, &fmap);

	Free(fmap);

	return ret;
}

/*
 * os_extents_get -- get extents of the given file
 *                   (and optionally read its block size)
 */
int
os_extents_get(int fd, struct extents *exts)
{
	LOG(3, "fd %i extents %p", fd, exts);

	struct fiemap *fmap = NULL;
	int ret = -1;

	ASSERTne(exts, NULL);

	if (exts->extents_count == 0)
		return 0;

	ASSERTne(exts->extents, NULL);

	if (os_extents_common(fd, exts, &fmap) <= 0)
		goto error_free;

	struct fiemap *newfmap = Realloc(fmap, sizeof(struct fiemap) +
						fmap->fm_mapped_extents *
						sizeof(struct fiemap_extent));
	if (newfmap == NULL) {
		ERR("!Realloc");
		goto error_free;
	}

	fmap = newfmap;
	fmap->fm_extent_count = fmap->fm_mapped_extents;

	memset(fmap->fm_extents, 0, fmap->fm_mapped_extents *
					sizeof(struct fiemap_extent));

	if (ioctl(fd, FS_IOC_FIEMAP, fmap) != 0) {
		ERR("!ioctl %d", fd);
		goto error_free;
	}

	if (fmap->fm_extent_count > 0) {
		LOG(10, "file with fd %i has %u extents:",
			fd, fmap->fm_extent_count);
	}

	unsigned e;
	for (e = 0; e < fmap->fm_extent_count; e++) {
		exts->extents[e].offset_physical =
						fmap->fm_extents[e].fe_physical;
		exts->extents[e].offset_logical =
						fmap->fm_extents[e].fe_logical;
		exts->extents[e].length = fmap->fm_extents[e].fe_length;

		LOG(10, "   #%u: off_phy: %lu off_log: %lu len: %lu",
			e,
			exts->extents[e].offset_physical,
			exts->extents[e].offset_logical,
			exts->extents[e].length);
	}

	ret = 0;

error_free:
	Free(fmap);

	return ret;
}
