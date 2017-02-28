/*
 * Copyright 2015-2017, Intel Corporation
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
 * file_windows.c -- Windows emulation of Linux-specific system calls
 */

/*
 * XXX - The initial approach to NVML for Windows port was to minimize the
 * amount of changes required in the core part of the library, and to avoid
 * preprocessor conditionals, if possible.  For that reason, some of the
 * Linux system calls that have no equivalents on Windows have been emulated
 * using Windows API.
 * Note that it was not a goal to fully emulate POSIX-compliant behavior
 * of mentioned functions.  They are used only internally, so current
 * implementation is just good enough to satisfy NVML needs and to make it
 * work on Windows.
 *
 * This is a subject for change in the future.  Likely, all these functions
 * will be replaced with "util_xxx" wrappers with OS-specific implementation
 * for Linux and Windows.
 */

#include <windows.h>
#include <sys/stat.h> // XXX
#include <sys/file.h> // XXX

#include "file.h"
#include "out.h"

/*
 * util_tmpfile --  (internal) create the temporary file
 */
int
util_tmpfile(const char *dir, const char *templ)
{
	LOG(3, "dir \"%s\" template \"%s\"", dir, templ);

	int oerrno;
	int fd = -1;

	char *fullname = alloca(strlen(dir) + strlen(templ) + 1);

	(void) strcpy(fullname, dir);
	(void) strcat(fullname, templ);

	/*
	 * XXX - block signals and modify file creation mask for the time
	 * of mkstmep() execution.  Restore previous settings once the file
	 * is created.
	 */

	fd = mkstemp(fullname);
	if (fd < 0) {
		ERR("!mkstemp");
		goto err;
	}

	/*
	 * There is no point to use unlink() here.  First, because it does not
	 * work on open files.  Second, because the file is created with
	 * O_TEMPORARY flag, and it looks like such temp files cannot be open
	 * from another process, even though they are visible on
	 * the filesystem.
	 */

	return fd;

err:
	oerrno = errno;
	if (fd != -1)
		(void) close(fd);
	errno = oerrno;
	return -1;
}

/*
 * util_is_absolute_path -- check if the path is absolute
 */
int
util_is_absolute_path(const char *path)
{
	LOG(3, "path: %s", path);

	if (path == NULL || path[0] == '\0')
		return 0;

	if (path[0] == '\\' || path[1] == ':')
		return 1;

	return 0;
}

/*
 * util_file_mkdir -- creates new dir
 */
int
util_file_mkdir(const char *path, mode_t mode)
{
	/*
	 * On windows we cannot create read only dir so mode
	 * parameter is useless.
	 */
	UNREFERENCED_PARAMETER(mode);
	LOG(3, "path: %s mode: %d", path, mode);
	return _mkdir(path);
}

/*
 * util_file_dir_open -- open a directory
 */
int
util_file_dir_open(struct dir_handle *handle, const char *path)
{
	/* init handle */
	handle->handle = NULL;
	handle->path = path;
	return 0;
}

/*
 * util_file_dir_next - read next file in directory
 */
int
util_file_dir_next(struct dir_handle *handle, struct file_info *info)
{
	WIN32_FIND_DATAA data;
	if (handle->handle == NULL) {
		handle->handle = FindFirstFileA(handle->path, &data);
		if (handle->handle == NULL)
			return FALSE;

	} else {
		if (FindNextFileA(handle->handle, &data) == 0) {
			return FALSE;
		}

	}
	strcpy(info->filename, data.cFileName);
	info->is_dir = data.dwFileAttributes ==	FILE_ATTRIBUTE_DIRECTORY;

	return TRUE;
}

/*
 * util_file_dir_close -- close a directory
 */
int
util_file_dir_close(struct dir_handle *handle)
{
	return FindClose(handle->handle);
}

/*
 * util_file_dir_close -- remove directory
 */
int
util_file_dir_remove(const char *path)
{
	return RemoveDirectoryA(path) == 0 ? -1 : 0;
}
