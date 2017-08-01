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
 * sysfs.c - implementation of the linux sysfs helper API
 */

#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/fiemap.h>
#include <linux/fs.h>
#include "file.h"
#include "os.h"
#include "out.h"
#include "sysfs.h"

struct sysfs_iter {
	char *format; /* scanf format */
	FILE *sysfile; /* file from the sysfs */
};

/*
 * sysfs_new -- creates a new sysfs iterator instance
 *
 * This is a very simple helper code that allows one to query a config-like
 * file (e.g., sysfs) with an easy and unified interface.
 */
struct sysfs_iter *
sysfs_new(const char *path, const char *format)
{
	LOG(15, "path %s format %s", path, format);

	struct sysfs_iter *iter = Malloc(sizeof(*iter));
	if (iter == NULL)
		goto error_iter_alloc;

	iter->format = Strdup(format);
	if (iter->format == NULL)
		goto error_format_alloc;

	if ((iter->sysfile = fopen(path, "r")) == NULL)
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
 * sysfs_delete -- deletes the iterator instance
 */
void
sysfs_delete(struct sysfs_iter *iter)
{
	LOG(15, "iter %p", iter);

	fclose(iter->sysfile);
	Free(iter->format);
	Free(iter);
}

/*
 * sysfs_vnext -- (internal) returns the next string through va_list
 */
static int
sysfs_vnext(struct sysfs_iter *iter, va_list ap)
{
	LOG(15, "iter %p", iter);

	return vfscanf(iter->sysfile, iter->format, ap);
}

/*
 * sysfs_next -- returns the next string
 */
int
sysfs_next(struct sysfs_iter *iter, ...)
{
	LOG(15, "iter %p", iter);

	va_list ap;
	va_start(ap, iter);
	int ret = sysfs_vnext(iter, ap);
	va_end(ap);

	return ret;
}

/*
 * sysfs_dev_new -- creates a new sysfs iterator with the prefix of the
 *	device backing the provided fd
 */
struct sysfs_iter *
sysfs_dev_new(int fd, const char *subpath, const char *format)
{
	LOG(15, "fd %d subpath %s format %s", fd, subpath, format);

	os_stat_t st;
	if (os_fstat(fd, &st) < 0)
		return NULL;

	char *devtype = S_ISCHR(st.st_mode) ? "char" : "block";
	char devpath[PATH_MAX];
	if (snprintf(devpath, PATH_MAX, "/sys/dev/%s/%d:%d/%s",
		devtype, major(st.st_dev), minor(st.st_dev), subpath) < 0)
		return NULL;

	return sysfs_new(devpath, format);
}

/*
 * sysfs_single -- queries sysfs file that has only a single value
 */
int
sysfs_single(const char *path, const char *format, ...)
{
	LOG(15, "subpath %s format %s", path, format);

	struct sysfs_iter *iter = sysfs_new(path, format);
	if (iter == NULL)
		return -1;

	va_list ap;
	va_start(ap, format);
	int ret = sysfs_vnext(iter, ap);
	va_end(ap);

	sysfs_delete(iter);

	return ret;
}

/*
 * sysfs_dev_single -- queries sysfs file that has only a single value
 */
int
sysfs_dev_single(int fd, const char *subpath, const char *format, ...)
{
	LOG(15, "fd %d subpath %s format %s", fd, subpath, format);

	struct sysfs_iter *iter = sysfs_dev_new(fd, subpath, format);
	if (iter == NULL)
		return -1;

	va_list ap;
	va_start(ap, format);
	int ret = sysfs_vnext(iter, ap);
	va_end(ap);

	sysfs_delete(iter);

	return ret;
}
