/*
 * Copyright 2018, Intel Corporation
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
 * os_badblock.h -- linux bad block API
 */

#ifndef PMDK_BADBLOCK_H
#define PMDK_BADBLOCK_H 1

#include <stdint.h>
#include <sys/types.h>

#define B2SEC(n) ((n) >> 9)	/* convert bytes to sectors */
#define SEC2B(n) ((n) << 9)	/* convert sectors to bytes */

/*
 * 'struct badblock' is already defined in ndctl/libndctl.h,
 * so we cannot use this name
 */
struct bad_block {
	unsigned long long offset;	/* in bytes */
	unsigned length;		/* in bytes */
};

struct badblocks {
	unsigned long long ns_resource;	/* address of the namespace */
	unsigned bb_cnt;		/* number of bad blocks */
	struct bad_block *bbv;		/* array of bad blocks */
};

long os_badblocks_count(const char *path);
int os_badblocks_get(const char *file, struct badblocks *bbs);
int os_badblocks_get_and_clear(const char *file, struct badblocks *bbs);
int os_badblocks_clear(const char *path);
int os_badblocks_check_file(const char *path);

#endif /* PMDK_BADBLOCK_H */
