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

#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/fiemap.h>
#include <linux/fs.h>
#include "file.h"
#include "os.h"
#include "out.h"
#include "extent.h"

struct extent_iter {
	struct fiemap *fmap;
	size_t pos; /* iterator position in the above fiemap */
};

/*
 * extent_new -- creates a new filesystem extent iterator
 *
 * It utilizes the FS_IOC_FIEMAP ioctl which returns an array of extents for the
 * particular device.
 * Because the array needs to be allocated from the userspace, we have two
 * options to figure out how many extents there are:
 *	- allocate a fixed amount of extents and then keep querying until
 *	FIEMAP_EXTENT_LAST is found.
 *	- query the first time without requesting the extents - just to retrieve
 *	the information about the amount of extents, and then reallocate the
 *	array to fit that size.
 * This code implements the latter approach because it's simpler, and we don't
 * expect racing against any other code.
 */
struct extent_iter *
extent_new(int fd)
{
	LOG(15, "fd %d", fd);

	os_stat_t st;
	if (os_fstat(fd, &st) < 0)
		return NULL;

	struct extent_iter *iter = Malloc(sizeof(*iter));
	if (iter == NULL)
		goto error_iter_alloc;

	iter->fmap = Zalloc(sizeof(struct fiemap));
	if (iter->fmap == NULL)
		goto error_fmap_alloc;

	ASSERT(st.st_size > 0);

	iter->fmap->fm_start = 0;
	iter->fmap->fm_length = (size_t)st.st_size;
	iter->fmap->fm_flags = 0;
	iter->fmap->fm_extent_count = 0;

	/* devdax does not have any extents */
	if (util_fd_is_device_dax(fd))
		return iter;

	if (ioctl(fd, FS_IOC_FIEMAP, iter->fmap) != 0)
		goto error_fiemap_ioctl;

	struct fiemap *fmap = Realloc(iter->fmap,
		sizeof(struct fiemap) +
		iter->fmap->fm_mapped_extents * sizeof(struct fiemap_extent));
	if (fmap == NULL)
		goto error_fmap_realloc;

	iter->fmap = fmap;
	iter->fmap->fm_extent_count = iter->fmap->fm_mapped_extents;

	if (ioctl(fd, FS_IOC_FIEMAP, iter->fmap) != 0)
		goto error_fiemap_ioctl;

	iter->pos = 0;

	return iter;

error_fmap_realloc:
error_fiemap_ioctl:
	Free(iter->fmap);
error_fmap_alloc:
	Free(iter);

error_iter_alloc:
	return NULL;
}

/*
 * extent_delete -- deletes the iterator instance
 */
void
extent_delete(struct extent_iter *iter)
{
	LOG(15, "iter %p", iter);

	Free(iter->fmap);
	Free(iter);
}

/*
 * extent_count -- returns the number of extents
 */
size_t
extent_count(struct extent_iter *iter)
{
	return iter->fmap->fm_extent_count;
}

/*
 * extent_next -- returns the current extent and bumps the iterator position
 */
int
extent_next(struct extent_iter *iter, struct extent *extent)
{
	LOG(15, "iter %p extent %p", iter, extent);

	if (iter->pos == iter->fmap->fm_extent_count)
		return -1;

	extent->offset = iter->fmap->fm_extents[iter->pos].fe_physical;
	extent->length = iter->fmap->fm_extents[iter->pos].fe_length;

	iter->pos += 1;

	return 0;
}
