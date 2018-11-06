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

#ifdef __cplusplus
extern "C" {
#endif

#define B2SEC(n) ((n) >> 9)	/* convert bytes to sectors */
#define SEC2B(n) ((n) << 9)	/* convert sectors to bytes */

#define NO_HEALTHY_REPLICA ((int)(-1))

#define BB_NOT_SUPP \
	"checking bad blocks is not supported on this OS, please switch off the CHECK_BAD_BLOCKS compat feature using 'pmempool-feature'"

/*
 * 'struct badblock' is already defined in ndctl/libndctl.h,
 * so we cannot use this name.
 *
 * libndctl returns offset relative to the beginning of the region,
 * but in this structure we save offset relative to the beginning of:
 * - namespace (before os_badblocks_get())
 * and
 * - file (before sync_recalc_badblocks())
 * and
 * - pool (after sync_recalc_badblocks())
 */
struct bad_block {
	/*
	 * offset in bytes relative to the beginning of
	 *  - namespace (before os_badblocks_get())
	 * and
	 *  - file (before sync_recalc_badblocks())
	 * and
	 *  - pool (after sync_recalc_badblocks())
	 */
	unsigned long long offset;

	/* length in bytes */
	unsigned length;

	/* number of healthy replica to fix this bad block */
	int nhealthy;
};

struct badblocks {
	unsigned long long ns_resource;	/* address of the namespace */
	unsigned bb_cnt;		/* number of bad blocks */
	struct bad_block *bbv;		/* array of bad blocks */
};

long os_badblocks_count(const char *path);
int os_badblocks_get(const char *file, struct badblocks *bbs);
int os_badblocks_clear(const char *path, struct badblocks *bbs);
int os_badblocks_clear_all(const char *file);
int os_badblocks_check_file(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* PMDK_BADBLOCK_H */
