// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2021, Intel Corporation */

/*
 * file_windows.c -- Windows emulation of Linux-specific system calls
 */

/*
 * XXX - The initial approach to PMDK for Windows port was to minimize the
 * amount of changes required in the core part of the library, and to avoid
 * preprocessor conditionals, if possible.  For that reason, some of the
 * Linux system calls that have no equivalents on Windows have been emulated
 * using Windows API.
 * Note that it was not a goal to fully emulate POSIX-compliant behavior
 * of mentioned functions.  They are used only internally, so current
 * implementation is just good enough to satisfy PMDK needs and to make it
 * work on Windows.
 */

#include <windows.h>
#include <sys/stat.h>
#include <sys/file.h>

#include "alloc.h"
#include "file.h"
#include "out.h"
#include "os.h"

/*
 * util_is_absolute_path -- check if the path is absolute
 */
int
util_is_absolute_path(const char *path)
{
	LOG(3, "path \"%s\"", path);

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
			return 1;
	} else {
		if (FindNextFileA(handle->handle, &data) == 0)
			return 1;
	}
	info->filename[NAME_MAX] = '\0';
	strncpy(info->filename, data.cFileName, NAME_MAX + 1);
	if (info->filename[NAME_MAX] != '\0')
		return -1; /* filename truncated */
	info->is_dir = data.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY;

	return 0;
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
 * util_file_dir_remove -- remove directory
 */
int
util_file_dir_remove(const char *path)
{
	return RemoveDirectoryA(path) == 0 ? -1 : 0;
}

/*
 * util_file_device_dax_alignment -- returns internal Device DAX alignment
 */
size_t
util_file_device_dax_alignment(const char *path)
{
	LOG(3, "path \"%s\"", path);

	return 0;
}

/*
 * util_ddax_region_find -- returns DEV dax region id that contains file
 */
int
util_ddax_region_find(const char *path, unsigned *region_id)
{
	LOG(3, "path \"%s\"", path);

	return -1;
}
