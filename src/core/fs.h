/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2017-2023, Intel Corporation */

/*
 * fs.h -- file system traversal abstraction layer
 */

#ifndef PMDK_FS_H
#define PMDK_FS_H 1

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

struct fs;

enum fs_entry_type {
	FS_ENTRY_FILE,
	FS_ENTRY_DIRECTORY,
	FS_ENTRY_SYMLINK,
	FS_ENTRY_OTHER,

	MAX_FS_ENTRY_TYPES
};

struct fs_entry {
	enum fs_entry_type type;

	const char *name;
	size_t namelen;

	const char *path;
	size_t pathlen;
	/* the depth of the traversal */
	/* Linux uses short. No harm in it being bigger */
	long level;

};

struct fs *fs_new(const char *path);
void fs_delete(struct fs *f);

/* this call invalidates the previous entry */
struct fs_entry *fs_read(struct fs *f);

#ifdef __cplusplus
}
#endif

#endif /* PMDK_FS_H */
