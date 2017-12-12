/*
 * Copyright 2014-2017, Intel Corporation
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
 * file_linux.c -- Linux-specific versions of file APIs
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

#define MAX_SIZE_LENGTH 64

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
	int fd = open(dir, O_TMPFILE | O_RDWR | flags, S_IRUSR | S_IWUSR);
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
	LOG(3, "path \"%s\"", path);

	os_stat_t st;
	int olderrno;

	if (os_stat(path, &st) < 0) {
		ERR("!stat \"%s\"", path);
		return 0;
	}

	char spath[PATH_MAX];
	snprintf(spath, PATH_MAX, "/sys/dev/char/%d:%d/device/align",
		major(st.st_rdev), minor(st.st_rdev));

	LOG(4, "device align path \"%s\"", spath);

	int fd = os_open(spath, O_RDONLY);
	if (fd < 0) {
		ERR("!open \"%s\"", spath);
		return 0;
	}

	size_t size = 0;

	char sizebuf[MAX_SIZE_LENGTH + 1];
	ssize_t nread;
	if ((nread = read(fd, sizebuf, MAX_SIZE_LENGTH)) < 0) {
		ERR("!read");
		goto out;
	}

	sizebuf[nread] = 0; /* null termination */

	char *endptr;

	olderrno = errno;
	errno = 0;

	/* 'align' is in decimal format */
	size = strtoull(sizebuf, &endptr, 10);
	if (endptr == sizebuf || *endptr != '\n' ||
	    (size == ULLONG_MAX && errno == ERANGE)) {
		ERR("invalid device alignment %s", sizebuf);
		size = 0;
		goto out;
	}

	/*
	 * If the alignment value is not a power of two, try with
	 * hex format, as this is how it was printed in older kernels.
	 * Just in case someone is using kernel <4.9.
	 */
	if ((size & (size - 1)) != 0) {
		size = strtoull(sizebuf, &endptr, 16);
		if (endptr == sizebuf || *endptr != '\n' ||
		    (size == ULLONG_MAX && errno == ERANGE)) {
			ERR("invalid device alignment %s", sizebuf);
			size = 0;
			goto out;
		}
	}

	errno = olderrno;

out:
	olderrno = errno;
	(void) os_close(fd);
	errno = olderrno;

	LOG(4, "device alignment %zu", size);
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
