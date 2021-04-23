// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2021, Intel Corporation */

/*
 * file_posix.c -- Posix versions of file APIs
 */

#ifndef __FreeBSD__
/* for O_TMPFILE */
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>

#include "os.h"
#include "file.h"
#include "out.h"
#include "libpmem2.h"
#include "../libpmem2/pmem2_utils.h"
#include "../libpmem2/region_namespace.h"

/*
 * util_is_absolute_path -- check if the path is an absolute one
 */
int
util_is_absolute_path(const char *path)
{
	LOG(3, "path: %s", path);

	if (path[0] == OS_DIR_SEPARATOR)
		return 1;
	else
		return 0;
}

/*
 * util_create_mkdir -- creates new dir
 */
int
util_file_mkdir(const char *path, mode_t mode)
{
	LOG(3, "path: %s mode: %o", path, mode);
	return mkdir(path, mode);
}

/*
 * util_file_dir_open -- open a directory
 */
int
util_file_dir_open(struct dir_handle *handle, const char *path)
{
	LOG(3, "handle: %p path: %s", handle, path);
	handle->dirp = opendir(path);
	return handle->dirp == NULL;
}

/*
 * util_file_dir_next -- read next file in directory
 */
int
util_file_dir_next(struct dir_handle *handle, struct file_info *info)
{
	LOG(3, "handle: %p info: %p", handle, info);
	struct dirent *d = readdir(handle->dirp);
	if (d == NULL)
		return 1; /* break */
	info->filename[NAME_MAX] = '\0';
	strncpy(info->filename, d->d_name, NAME_MAX + 1);
	if (info->filename[NAME_MAX] != '\0')
		return -1; /* filename truncated */
	info->is_dir = d->d_type == DT_DIR;
	return 0; /* continue */
}

/*
 * util_file_dir_close -- close a directory
 */
int
util_file_dir_close(struct dir_handle *handle)
{
	LOG(3, "path: %p", handle);
	return closedir(handle->dirp);
}

/*
 * util_file_dir_remove -- remove directory
 */
int
util_file_dir_remove(const char *path)
{
	LOG(3, "path: %s", path);
	return rmdir(path);
}

/*
 * device_dax_alignment -- (internal) checks the alignment of given Device DAX
 */
static size_t
device_dax_alignment(const char *path)
{
	size_t size = 0;

	LOG(3, "path \"%s\"", path);

	struct pmem2_source *src;

	int fd = os_open(path, O_RDONLY);
	if (fd == -1) {
		LOG(1, "Cannot open file %s", path);
		return size;
	}

	int ret = pmem2_source_from_fd(&src, fd);
	if (ret)
		goto end;

	ret = pmem2_device_dax_alignment(src, &size);
	if (ret) {
		size = 0;
		goto end;
	}

end:
	pmem2_source_delete(&src);
	os_close(fd);
	return size;
}

/*
 * util_file_device_dax_alignment -- returns internal Device DAX alignment
 */
size_t
util_file_device_dax_alignment(const char *path)
{
	LOG(3, "path \"%s\"", path);

	return device_dax_alignment(path);
}

/*
 * util_ddax_region_find -- returns Device DAX region id
 */
int
util_ddax_region_find(const char *path, unsigned *region_id)
{
	LOG(3, "path \"%s\"", path);

	os_stat_t st;
	int ret;

	if (os_stat(path, &st) < 0) {
		ERR("!stat \"%s\"", path);
		return -1;
	}

	enum pmem2_file_type ftype;
	if ((ret = pmem2_get_type_from_stat(&st, &ftype)) < 0) {
		errno = pmem2_err_to_errno(ret);
		return -1;
	}

	/*
	 * XXX: this is a hack to workaround the fact that common is using
	 * non-public APIs of libpmem2, and there's often no way to properly
	 * create the required structures...
	 * This needs to go away together with refactoring that untangles
	 * these internal dependencies.
	 */
	struct pmem2_source src;
	src.type = PMEM2_SOURCE_FD;
	src.value.ftype = ftype;
	src.value.st_rdev = st.st_rdev;
	src.value.st_dev = st.st_dev;

	ret = pmem2_get_region_id(&src, region_id);
	if (ret < 0) {
		errno = pmem2_err_to_errno(ret);
		return -1;
	}

	return ret;
}
