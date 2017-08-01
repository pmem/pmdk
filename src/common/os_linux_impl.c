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
 * os_linux_impl.c - implementation of the linux specific APIs
 */

#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/fiemap.h>
#include <linux/fs.h>
#include "file.h"
#include "os.h"
#include "out.h"
#include "os_linux.h"

struct os_extent_iter
{
	struct fiemap *fmap;
	size_t pos; /* iterator position in the above fiemap */
};

/*
 * os_extent_new -- creates a new filesystem extent iterator
 *
 * It utilizes the FS_IOC_FIEMAP ioctl. It returns an array of extents for the
 * particular device.
 * Because the array needs to be allocated from the user space, we have to
 * options to figure out how many extents there are:
 *	- allocate a fixed amount of extents and then keep querying until
 *	FIEMAP_EXTENT_LAST is found.
 *	- query the first time without requesting the extents - just to retrieve
 *	the information about the amount of extents, and then reallocate the
 *	array to fit that size.
 * This code implements the latter approach because it's simpler, and we don't
 * expect racing against any other code.
 */
struct os_extent_iter *
os_extent_new(int fd)
{
	LOG(15, "fd %d", fd);

	os_stat_t st;
	if (os_fstat(fd, &st) < 0)
		return NULL;

	struct os_extent_iter *iter = Malloc(sizeof(*iter));
	if (iter == NULL)
		goto error_iter_alloc;

	iter->fmap = Malloc(sizeof(struct fiemap));
	if (iter->fmap == NULL)
		goto error_fmap_alloc;

	ASSERT(st.st_size > 0);

	iter->fmap->fm_start = 0;
	iter->fmap->fm_length = (size_t)st.st_size;
	iter->fmap->fm_flags = 0;
	iter->fmap->fm_extent_count = 0;

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
 * os_extent_delete -- deletes the iterator instance
 */
void
os_extent_delete(struct os_extent_iter *iter)
{
	LOG(15, "iter %p", iter);

	Free(iter->fmap);
	Free(iter);
}

/*
 * os_extent_next -- returns the current extent and bumps the iterator position
 */
int
os_extent_next(struct os_extent_iter *iter, struct os_extent *extent)
{
	LOG(15, "iter %p extent %p", iter, extent);

	if (iter->pos == iter->fmap->fm_extent_count)
		return -1;

	extent->offset = iter->fmap->fm_extents[iter->pos].fe_physical;
	extent->length = iter->fmap->fm_extents[iter->pos].fe_length;

	iter->pos += 1;

	return 0;
}

struct os_sysfs_iter
{
	char *format; /* scanf format */
	FILE *sysfile; /* file from the sysfs */
};

/*
 * os_sysfs_new -- creates a new sysfs iterator instance
 *
 * This is a very simple helper code that allows one to query a config-like
 * file (sysfs) with an easy and unified interface.
 */
struct os_sysfs_iter *
os_sysfs_new(const char *path, const char *format)
{
	LOG(15, "path %s format %s", path, format);

	struct os_sysfs_iter *iter = Malloc(sizeof(*iter));
	if (iter == NULL)
		goto error_iter_alloc;

	iter->format = Strdup(format);
	if (iter->format == NULL)
		goto error_format_alloc;

	if ((iter->sysfile = fopen(path, "ro")) == NULL)
		goto error_sysfile_open;

	return iter;

error_sysfile_open:
	Free(iter->format);
error_format_alloc:
	Free(iter);

error_iter_alloc:
	return NULL;
}

/*
 * os_sysfs_delete -- deletes the iterator instance
 */
void
os_sysfs_delete(struct os_sysfs_iter *iter)
{
	LOG(15, "iter %p", iter);

	fclose(iter->sysfile);
	Free(iter->format);
	Free(iter);
}

/*
 * os_sysfs_vnext -- (internal) returns the next string through va_list
 */
static int
os_sysfs_vnext(struct os_sysfs_iter *iter, va_list ap)
{
	LOG(15, "iter %p", iter);

	return vfscanf(iter->sysfile, iter->format, ap);
}

/*
 * os_sysfs_next -- returns the next string
 */
int
os_sysfs_next(struct os_sysfs_iter *iter, ...)
{
	LOG(15, "iter %p", iter);

	va_list ap;
	va_start(ap, iter);
	int ret = os_sysfs_vnext(iter, ap);
	va_end(ap);

	return ret;
}

/*
 * os_sysfs_dev_new -- creates a new sysfs iterator with the prefix of the
 *	device backing the provided fd
 */
struct os_sysfs_iter *
os_sysfs_dev_new(int fd, const char *subpath, const char *format)
{
	LOG(15, "fd %d subpath %s format %s", fd, subpath, format);

	os_stat_t st;
	if (os_fstat(fd, &st) < 0)
		return NULL;

	char *devtype = util_fd_is_device_dax(fd) ? "char" : "block";
	char devpath[PATH_MAX];
	if (snprintf(devpath, PATH_MAX, "/sys/dev/%s/%d:%d/%s",
		devtype, major(st.st_dev), minor(st.st_dev), subpath) < 0)
		return NULL;

	return os_sysfs_new(devpath, format);
}

/*
 * os_sysfs_single -- queries sysfs file that has only a single value
 */
int
os_sysfs_single(const char *path, const char *format, ...)
{
	LOG(15, "subpath %s format %s", path, format);

	struct os_sysfs_iter *iter = os_sysfs_new(path, format);
	if (iter == NULL)
		return -1;

	va_list ap;
	va_start(ap, format);
	int ret = os_sysfs_vnext(iter, ap);
	va_end(ap);

	os_sysfs_delete(iter);

	return ret;
}

/*
 * os_sysfs_dev_single -- queries sysfs file that has only a single value
 */
int
os_sysfs_dev_single(int fd, const char *subpath, const char *format, ...)
{
	LOG(15, "fd %d subpath %s format %s", fd, subpath, format);

	struct os_sysfs_iter *iter = os_sysfs_dev_new(fd, subpath, format);
	if (iter == NULL)
		return -1;

	va_list ap;
	va_start(ap, format);
	int ret = os_sysfs_vnext(iter, ap);
	va_end(ap);

	os_sysfs_delete(iter);

	return ret;
}
