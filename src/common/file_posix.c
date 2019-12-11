/*
 * Copyright 2014-2019, Intel Corporation
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
 * file_posix.c -- Posix versions of file APIs
 */

/* for O_TMPFILE */
#define _GNU_SOURCE

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
#include "../libpmem2/pmem2_utils.h"

#define DAX_REGION_ID_LEN 6 /* 5 digits + \0 */

/*
 * util_tmpfile_mkstemp --  (internal) create temporary file
 *                          if O_TMPFILE not supported
 */
static int
util_tmpfile_mkstemp(const char *dir, const char *templ)
{
	/* the templ must start with a path separator */
	ASSERTeq(templ[0], '/');

	int oerrno;
	int fd = -1;

	char *fullname = alloca(strlen(dir) + strlen(templ) + 1);

	(void) strcpy(fullname, dir);
	(void) strcat(fullname, templ);

	sigset_t set, oldset;
	sigfillset(&set);
	(void) sigprocmask(SIG_BLOCK, &set, &oldset);

	mode_t prev_umask = umask(S_IRWXG | S_IRWXO);

	fd = os_mkstemp(fullname);

	umask(prev_umask);

	if (fd < 0) {
		ERR("!mkstemp");
		goto err;
	}

	(void) os_unlink(fullname);
	(void) sigprocmask(SIG_SETMASK, &oldset, NULL);
	LOG(3, "unlinked file is \"%s\"", fullname);

	return fd;

err:
	oerrno = errno;
	(void) sigprocmask(SIG_SETMASK, &oldset, NULL);
	if (fd != -1)
		(void) os_close(fd);
	errno = oerrno;
	return -1;
}

/*
 * util_tmpfile -- create temporary file
 */
int
util_tmpfile(const char *dir, const char *templ, int flags)
{
	LOG(3, "dir \"%s\" template \"%s\" flags %x", dir, templ, flags);

	/* only O_EXCL is allowed here */
	ASSERT(flags == 0 || flags == O_EXCL);

#ifdef O_TMPFILE
	int fd = os_open(dir, O_TMPFILE | O_RDWR | flags, S_IRUSR | S_IWUSR);
	/*
	 * Open can fail if underlying file system does not support O_TMPFILE
	 * flag.
	 */
	if (fd >= 0)
		return fd;
	if (errno != EOPNOTSUPP) {
		ERR("!open");
		return -1;
	}
#endif

	return util_tmpfile_mkstemp(dir, templ);
}

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
	os_stat_t st;

	LOG(3, "path \"%s\"", path);

	if (os_stat(path, &st) < 0) {
		ERR("!stat \"%s\"", path);
		return 0;
	}

	int ret = pmem2_device_dax_alignment_from_stat(&st, &size);
	if (ret)
		return 0;

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
util_ddax_region_find(const char *path)
{
	LOG(3, "path \"%s\"", path);

	int dax_reg_id_fd;
	char dax_region_path[PATH_MAX];
	char reg_id[DAX_REGION_ID_LEN];
	char *end_addr;
	os_stat_t st;

	ASSERTne(path, NULL);
	if (os_stat(path, &st) < 0) {
		ERR("!stat \"%s\"", path);
		return -1;
	}

	dev_t dev_id = st.st_rdev;

	unsigned major = os_major(dev_id);
	unsigned minor = os_minor(dev_id);
	int ret = snprintf(dax_region_path, PATH_MAX,
		"/sys/dev/char/%u:%u/device/dax_region/id",
		major, minor);
	if (ret < 0) {
		ERR("snprintf(%p, %d, /sys/dev/char/%u:%u/device/"
			"dax_region/id, %u, %u): %d",
			dax_region_path, PATH_MAX, major, minor, major, minor,
			ret);
		return -1;
	}

	if ((dax_reg_id_fd = os_open(dax_region_path, O_RDONLY)) < 0) {
		LOG(1, "!open(\"%s\", O_RDONLY)", dax_region_path);
		return -1;
	}

	ssize_t len = read(dax_reg_id_fd, reg_id, DAX_REGION_ID_LEN);

	if (len == -1) {
		ERR("!read(%d, %p, %d)", dax_reg_id_fd,
			reg_id, DAX_REGION_ID_LEN);
		goto err;
	} else if (len < 2 || reg_id[len - 1] != '\n') {
		errno = EINVAL;
		ERR("!read(%d, %p, %d) invalid format", dax_reg_id_fd,
			reg_id, DAX_REGION_ID_LEN);
		goto err;
	}

	int olderrno = errno;
	errno = 0;
	long reg_num = strtol(reg_id, &end_addr, 10);
	if ((errno == ERANGE && (reg_num == LONG_MAX || reg_num == LONG_MIN)) ||
			(errno != 0 && reg_num == 0)) {
		ERR("!strtol(%p, %p, 10)", reg_id, end_addr);
		goto err;
	}
	errno = olderrno;

	if (end_addr == reg_id) {
		ERR("!strtol(%p, %p, 10) no digits were found",
			reg_id, end_addr);
		goto err;
	}
	if (*end_addr != '\n') {
		ERR("!strtol(%s, %s, 10) invalid format",
			reg_id, end_addr);
		goto err;
	}

	os_close(dax_reg_id_fd);
	return (int)reg_num;

err:
	os_close(dax_reg_id_fd);
	return -1;
}
