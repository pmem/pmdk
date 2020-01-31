/*
 * Copyright 2017-2018, Intel Corporation
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
 * fs_posix.c -- file system traversal Posix implementation
 */

#include <fts.h>
#include "util.h"
#include "out.h"
#include "vec.h"
#include "fs.h"

struct fs {
	FTS *ft;
	struct fs_entry entry;
};

/*
 * fs_new -- creates fs traversal instance
 */
struct fs *
fs_new(const char *path)
{
	struct fs *f = Zalloc(sizeof(*f));
	if (f == NULL)
		goto error_fs_alloc;

	const char *paths[2] = {path, NULL};
	f->ft = fts_open((char * const *)paths, FTS_COMFOLLOW | FTS_XDEV, NULL);
	if (f->ft == NULL)
		goto error_fts_open;

	return f;

error_fts_open:
	Free(f);
error_fs_alloc:
	return NULL;
}

/*
 * fs_read -- reads an entry from the fs path
 */
struct fs_entry *
fs_read(struct fs *f)
{
	FTSENT *entry = fts_read(f->ft);
	if (entry == NULL)
		return NULL;

	switch (entry->fts_info) {
	case FTS_D:
		f->entry.type = FS_ENTRY_DIRECTORY;
		break;
	case FTS_F:
		f->entry.type = FS_ENTRY_FILE;
		break;
	case FTS_SL:
		f->entry.type = FS_ENTRY_SYMLINK;
		break;
	default:
		f->entry.type = FS_ENTRY_OTHER;
		break;
	}

	f->entry.name = entry->fts_name;
	f->entry.namelen = entry->fts_namelen;
	f->entry.path = entry->fts_path;
	f->entry.pathlen = entry->fts_pathlen;
	f->entry.level = entry->fts_level;

	return &f->entry;
}

/*
 * fs_delete -- deletes a fs traversal instance
 */
void
fs_delete(struct fs *f)
{
	fts_close(f->ft);
	Free(f);
}
