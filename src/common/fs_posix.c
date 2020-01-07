// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

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
