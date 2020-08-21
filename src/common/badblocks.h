/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2017-2020, Intel Corporation */

/*
 * badblocks.h -- bad blocks API based on the libpmem2 library
 */

#ifndef PMDK_BADBLOCKS_H
#define PMDK_BADBLOCKS_H 1

#include <string.h>
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
 * - namespace (before badblocks_get())
 * and
 * - file (before sync_recalc_badblocks())
 * and
 * - pool (after sync_recalc_badblocks())
 */
struct bad_block {
	/*
	 * offset in bytes relative to the beginning of
	 *  - namespace (before badblocks_get())
	 * and
	 *  - file (before sync_recalc_badblocks())
	 * and
	 *  - pool (after sync_recalc_badblocks())
	 */
	size_t offset;

	/* length in bytes */
	size_t length;

	/* number of healthy replica to fix this bad block */
	int nhealthy;
};

struct badblocks {
	unsigned bb_cnt;		/* number of bad blocks */
	struct bad_block *bbv;	/* array of bad blocks */
};

struct badblocks *badblocks_new(void);
void badblocks_delete(struct badblocks *bbs);

long badblocks_count(const char *path);
int badblocks_get(const char *file, struct badblocks *bbs);

int badblocks_clear(const char *path, struct badblocks *bbs);
int badblocks_clear_all(const char *file);

int badblocks_check_file(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* PMDK_BADBLOCKS_H */
