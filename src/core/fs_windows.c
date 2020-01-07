// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

/*
 * fs_windows.c -- file system traversal windows implementation
 */

#include <windows.h>
#include "alloc.h"
#include "fs.h"
#include "out.h"
#include "util.h"

struct fs {
	size_t dirlen;
	WIN32_FIND_DATAW ffd;
	HANDLE hFind;
	int first_done;
	const char *dir;

	struct fs_entry entry;
};

/*
 * fs_new -- creates fs traversal instance
 */
struct fs *
fs_new(const char *path)
{
	size_t pathlen = strlen(path);
	char *search_path = Malloc(strlen(path) + sizeof("\\*\0"));
	if (search_path == NULL)
		goto error_spath_alloc;

	strcpy(search_path, path);
	strcpy(search_path + pathlen, "\\*\0");

	wchar_t *pathw = util_toUTF16(search_path);
	if (pathw == NULL)
		goto error_path_alloc;

	struct fs *f = Zalloc(sizeof(*f));
	if (f == NULL)
		goto error_fs_alloc;

	f->first_done = 0;

	f->hFind = FindFirstFileW(pathw, &f->ffd);
	if (f->hFind == INVALID_HANDLE_VALUE)
		goto error_fff;

	f->dir = path;
	f->dirlen = pathlen;
	util_free_UTF16(pathw);
	Free(search_path);

	return f;

error_fff:
	Free(f);
error_fs_alloc:
	util_free_UTF16(pathw);
error_path_alloc:
	Free(search_path);
error_spath_alloc:
	return NULL;
}

/*
 * fs_read -- reads an entry from the fs path
 */
struct fs_entry *
fs_read(struct fs *f)
{
	util_free_UTF8((char *)f->entry.name);
	Free((char *)f->entry.path);
	f->entry.name = NULL;
	f->entry.path = NULL;

	if (f->first_done) {
		if (FindNextFileW(f->hFind, &f->ffd) == 0)
			return NULL;
	} else {
		f->first_done = 1;
	}

	if (f->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		f->entry.type = FS_ENTRY_DIRECTORY;
	else
		f->entry.type = FS_ENTRY_FILE;

	f->entry.name = util_toUTF8(f->ffd.cFileName);
	if (f->entry.name == NULL)
		return NULL;
	f->entry.namelen = strnlen(f->entry.name, MAX_PATH);

	f->entry.pathlen = f->dirlen + f->entry.namelen + 1;
	char *path = Zalloc(f->entry.pathlen + 1);
	if (path == NULL) {
		util_free_UTF8((char *)f->entry.name);
		return NULL;
	}
	strcpy(path, f->dir);
	path[f->dirlen] = '\\';
	strcpy(path + f->dirlen + 1, f->entry.name);
	f->entry.path = path;
	f->entry.level = 1;

	return &f->entry;
}

/*
 * fs_delete -- deletes a fs traversal instance
 */
void
fs_delete(struct fs *f)
{
	util_free_UTF8((char *)f->entry.name);
	Free((char *)f->entry.path);

	FindClose(f->hFind);
	Free(f);
}
