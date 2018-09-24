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
	/* XXX long on FreeBSD. Linux uses short. No harm in it being bigger */
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
