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
	FILE *sysfile; /* file from the sysfs */
};

/*
 * sysfs_vread -- (internal) read a single value from the provided path
 */
static int
sysfs_vread(struct sysfs_iter **uiter,
	const char *path, const char *format, va_list ap)
{
	LOG(15, "path %s format %s", path, format);

	struct sysfs_iter *iter;

	if (uiter == NULL || *uiter == NULL) {
		iter = Malloc(sizeof(*iter));
		if (iter == NULL)
			goto error_iter_alloc;

		iter->sysfile = os_fopen(path, "r");
		if (iter->sysfile == NULL)
			goto error_sysfile_open;

		if (uiter != NULL)
			*uiter = iter;
	} else {
		iter = *uiter;
	}

	int ret = vfscanf(iter->sysfile, format, ap);
	if (ret == EOF || ret == 0 || uiter == NULL) {
		fclose(iter->sysfile);
		Free(iter);
		if (uiter != NULL)
			*uiter = NULL;
	}

	return ret;

error_sysfile_open:
	Free(iter);
error_iter_alloc:
	return -1;
}

/*
 * sysfs_read -- read a single value from the provided path
 */
int
sysfs_read(struct sysfs_iter **iter,
	const char *path, const char *format, ...)
{
	LOG(15, "path %s format %s", path, format);

	va_list ap;
	va_start(ap, format);
	int ret = sysfs_vread(iter, path, format, ap);
	va_end(ap);

	return ret;
}

/*
 * sysfs_dev_read -- read a single value from the device sysfs location
 */
int
sysfs_dev_read(struct sysfs_iter **iter,
	int fd, const char *subpath, const char *format, ...)
{
	LOG(15, "fd %d subpath %s format %s", fd, subpath, format);

	const char *path = NULL;
	if (iter == NULL || *iter == NULL) {
		os_stat_t st;
		if (os_fstat(fd, &st) < 0)
			return -1;

		char *devtype;
		unsigned major;
		unsigned minor;

		if (S_ISCHR(st.st_mode)) {
			devtype = "char";
			major = major(st.st_rdev);
			minor = minor(st.st_rdev);
		} else {
			devtype = "block";
			major = major(st.st_dev);
			minor = minor(st.st_dev);
		}

		char devpath[PATH_MAX];
		if (snprintf(devpath, PATH_MAX, "/sys/dev/%s/%d:%d/%s",
			devtype, major, minor, subpath) < 0)
			return -1;

		path = devpath;
	}

	va_list ap;
	va_start(ap, format);
	int ret = sysfs_vread(iter, path, format, ap);
	va_end(ap);

	return ret;
}

/*
 * sysfs_early_delete -- if needed, deletes the sysfs instance
 */
void
sysfs_early_delete(struct sysfs_iter **iter)
{
	if (*iter != NULL) {
		fclose((*iter)->sysfile);
		Free(*iter);
		*iter = NULL;
	}
}
