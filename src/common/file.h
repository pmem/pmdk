// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2020, Intel Corporation */

/*
 * file.h -- internal definitions for file module
 */

#ifndef PMDK_FILE_H
#define PMDK_FILE_H 1

#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include "os.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define NAME_MAX _MAX_FNAME
#endif

struct file_info {
	char filename[NAME_MAX + 1];
	int is_dir;
};

struct dir_handle {
	const char *path;
#ifdef _WIN32
	HANDLE handle;
	char *_file;
#else
	DIR *dirp;
#endif
};

enum file_type {
	OTHER_ERROR = -2,
	NOT_EXISTS = -1,
	TYPE_NORMAL = 1,
	TYPE_DEVDAX = 2
};

int util_file_dir_open(struct dir_handle *a, const char *path);
int util_file_dir_next(struct dir_handle *a, struct file_info *info);
int util_file_dir_close(struct dir_handle *a);
int util_file_dir_remove(const char *path);
int util_file_exists(const char *path);
enum file_type util_stat_get_type(const os_stat_t *st);
enum file_type util_fd_get_type(int fd);
enum file_type util_file_get_type(const char *path);
int util_ddax_region_find(const char *path, unsigned *region_id);
ssize_t util_file_get_size(const char *path);
ssize_t util_fd_get_size(int fd);
size_t util_file_device_dax_alignment(const char *path);
void *util_file_map_whole(const char *path);
int util_file_zero(const char *path, os_off_t off, size_t len);
ssize_t util_file_pread(const char *path, void *buffer, size_t size,
	os_off_t offset);
ssize_t util_file_pwrite(const char *path, const void *buffer, size_t size,
	os_off_t offset);

int util_tmpfile(const char *dir, const char *templ, int flags);
int util_is_absolute_path(const char *path);

int util_file_create(const char *path, size_t size, size_t minsize);
int util_file_open(const char *path, size_t *size, size_t minsize, int flags);
int util_unlink(const char *path);
int util_unlink_flock(const char *path);
int util_file_mkdir(const char *path, mode_t mode);

int util_write_all(int fd, const char *buf, size_t count);

#ifndef _WIN32
#define util_read	read
#define util_write	write
#else
static inline ssize_t
util_read(int fd, void *buf, size_t count)
{
	/*
	 * Simulate short read, because Windows' _read uses "unsigned" as
	 * a type of the last argument and "int" as a return type.
	 * We have to limit "count" to what _read can return as a success,
	 * not what it can accept.
	 */
	if (count > INT_MAX)
		count = INT_MAX;
	return _read(fd, buf, (unsigned)count);
}

static inline ssize_t
util_write(int fd, const void *buf, size_t count)
{
	/*
	 * Simulate short write, because Windows' _write uses "unsigned" as
	 * a type of the last argument and "int" as a return type.
	 * We have to limit "count" to what _write can return as a success,
	 * not what it can accept.
	 */
	if (count > INT_MAX)
		count = INT_MAX;
	return _write(fd, buf, (unsigned)count);
}
#define S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#endif
#ifdef __cplusplus
}
#endif
#endif
