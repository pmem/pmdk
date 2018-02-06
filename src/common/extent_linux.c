/*
 * Copyright 2017, Intel Corporation
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

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include "file.h"
#include "out.h"
#include "extent.h"

/*
 * os_extents_get -- get extents of the given file
 *                   (and optionally read its block size)
 */
struct fiemap *
os_extents_get(const char *path, long *blksize)
{
	LOG(3, "path %s", path);

	struct fiemap *fmap = NULL;

	int fd = open(path, O_RDONLY);
	if (fd == -1)
		return NULL;

	os_stat_t st;
	if (os_fstat(fd, &st) < 0)
		goto error_close;

	fmap = Zalloc(sizeof(struct fiemap));
	if (fmap == NULL)
		goto error_close;

	fmap->fm_start = 0;
	fmap->fm_length = (size_t)st.st_size;
	fmap->fm_flags = 0;
	fmap->fm_extent_count = 0;

	LOG(10, "%s: block size: %li", path, st.st_blksize);

	if (blksize)
		*blksize = st.st_blksize;

	/* devdax does not have any extents */
	if (util_fd_is_device_dax(fd)) {
		close(fd);
		return fmap;
	}

	if (ioctl(fd, FS_IOC_FIEMAP, fmap) != 0)
		goto error_free;

	fmap = Realloc(fmap,
			sizeof(struct fiemap) +
			fmap->fm_mapped_extents * sizeof(struct fiemap_extent));
	if (fmap == NULL)
		goto error_free;

	fmap->fm_extent_count = fmap->fm_mapped_extents;

	if (ioctl(fd, FS_IOC_FIEMAP, fmap) != 0)
		goto error_free;

	LOG(4, "%s: number of extents: %u", path, fmap->fm_extent_count);

	close(fd);

	return fmap;

error_free:
	Free(fmap);

error_close:
	close(fd);

	return NULL;
}
