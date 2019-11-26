/*
 * Copyright 2018-2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * extent_linux.c - implementation of the linux fs extent query API
 */

#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/fiemap.h>

#include "file.h"
#include "out.h"
#include "extent.h"
#include "alloc.h"

/*
 * os_extents_common -- (internal) common part of getting extents
 *                      of the given file
 *
 * Returns: number of extents the file consists of or -1 in case of an error.
 * Sets: file descriptor, struct fiemap and struct extents.
 */
static long
os_extents_common(const char *path, struct extents *exts,
			int *pfd, struct fiemap **pfmap)
{
	LOG(3, "path %s exts %p pfd %p pfmap %p", path, exts, pfd, pfmap);

	int fd = os_open(path, O_RDONLY);
	if (fd == -1) {
		ERR("!open %s", path);
		return -1;
	}

	enum file_type type = util_fd_get_type(fd);
	if (type < 0)
		goto error_close;

	struct stat st;
	if (fstat(fd, &st) < 0) {
		ERR("!fstat %d", fd);
		goto error_close;
	}

	if (exts->extents_count == 0) {
		LOG(10, "%s: block size: %li", path, (long int)st.st_blksize);
		exts->blksize = (uint64_t)st.st_blksize;
	}

	/* devdax does not have any extents */
	if (type == TYPE_DEVDAX) {
		close(fd);
		return 0;
	}

	struct fiemap *fmap = Zalloc(sizeof(struct fiemap));
	if (fmap == NULL) {
		ERR("!malloc");
		goto error_close;
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
		LOG(4, "%s: number of extents: %u", path, exts->extents_count);

	} else if (exts->extents_count != fmap->fm_mapped_extents) {
		ERR("number of extents differs (was: %u, is: %u)",
			exts->extents_count, fmap->fm_mapped_extents);
		goto error_free;
	}

	*pfd = fd;
	*pfmap = fmap;

	return exts->extents_count;

error_free:
	Free(fmap);

error_close:
	close(fd);

	return -1;
}

/*
 * os_extents_count -- get number of extents of the given file
 *                     (and optionally read its block size)
 */
long
os_extents_count(const char *path, struct extents *exts)
{
	LOG(3, "path %s extents %p", path, exts);

	struct fiemap *fmap = NULL;
	int fd = -1;

	ASSERTne(exts, NULL);
	memset(exts, 0, sizeof(*exts));

	long ret = os_extents_common(path, exts, &fd, &fmap);

	Free(fmap);

	if (fd != -1)
		close(fd);

	return ret;
}

/*
 * os_extents_get -- get extents of the given file
 *                   (and optionally read its block size)
 */
int
os_extents_get(const char *path, struct extents *exts)
{
	LOG(3, "path %s extents %p", path, exts);

	struct fiemap *fmap = NULL;
	int fd = -1;
	int ret = -1;

	ASSERTne(exts, NULL);

	if (exts->extents_count == 0)
		return 0;

	ASSERTne(exts->extents, NULL);

	if (os_extents_common(path, exts, &fd, &fmap) <= 0)
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
		LOG(10, "file %s has %u extents:", path, fmap->fm_extent_count);
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

	if (fd != -1)
		close(fd);

	return ret;
}
