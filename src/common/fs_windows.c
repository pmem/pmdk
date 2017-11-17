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
 * fs_windows.c -- file system traversal linux implementation
 */

#include <windows.h>
#include "util.h"
#include "out.h"
#include "vec.h"
#include "fs.h"

struct fs {
	WIN32_FIND_DATAW ffd;
	HANDLE hFind;
	int first_done;
	VEC(, struct fs_entry) entries;
	const char *dir;
	size_t dirlen;
};

/*
 * fs_open -- creates fs traversal instance
 */
struct fs *
fs_new(const char *path)
{
	wchar_t *pathw = util_toUTF16(path);
	if (pathw == NULL)
		goto error_path_alloc;

	struct fs *f = Malloc(sizeof(*f));
	if (f == NULL)
		goto error_fs_alloc;

	VEC_INIT(&f->entries);

	f->first_done = 0;

	f->hFind = FindFirstFileW(pathw, &f->ffd);
	if (f->hFind == INVALID_HANDLE_VALUE)
		goto error_fff;

	f->dir = path;
	f->dirlen = strlen(path);
	util_free_UTF16(pathw);

	return f;

error_fff:
	Free(f);
error_fs_alloc:
	util_free_UTF16(pathw);
error_path_alloc:
	return NULL;
}

/*
 * fs_read -- reads an entry from the fs path
 */
struct fs_entry *
fs_read(struct fs *f)
{
	if (f->first_done) {
		if (FindNextFileW(f->hFind, &f->ffd) == 0)
			return NULL;
	} else {
		f->first_done = 1;
	}

	struct fs_entry e;
	if (f->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		e.type = FS_ENTRY_DIRECTORY;
	else
		e.type = FS_ENTRY_FILE;

	e.name = util_toUTF8(f->ffd.cFileName);
	if (e.name == NULL)
		return NULL;
	e.namelen = strnlen(e.name, MAX_PATH);

	e.pathlen = sizeof(e.namelen) + f->dirlen + sizeof('\\') + sizeof('\0');
	char *path = Zalloc(e.pathlen);
	if (path == NULL) {
		util_free_UTF8((char *)e.name);
		return NULL;
	}
	strcpy(path, f->dir);
	path[f->dirlen + 1] = '\\';
	strcpy(path + f->dirlen + 2, e.name);
	e.path = path;

	VEC_PUSH_BACK(&f->entries, e);

	return &VEC_BACK(&f->entries);
}

/*
 * fs_delete -- deletes a fs traversal instance
 */
void
fs_delete(struct fs *f)
{
	struct fs_entry *e;
	VEC_FOREACH_BY_PTR(e, &f->entries) {
		util_free_UTF8((char *)e->name);
		Free((char *)e->path);
	}
	VEC_DELETE(&f->entries);
	FindClose(f->hFind);
	Free(f);
}
