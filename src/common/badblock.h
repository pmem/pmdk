// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * badblock.h - common part of bad blocks API
 */

#ifndef PMDK_BADBLOCK_POOLSET_H
#define PMDK_BADBLOCK_POOLSET_H 1

#include "set.h"

#ifdef __cplusplus
extern "C" {
#endif

struct badblocks *badblocks_new(void);
void badblocks_delete(struct badblocks *bbs);

int badblocks_check_poolset(struct pool_set *set, int create);
int badblocks_clear_poolset(struct pool_set *set, int create);

char *badblocks_recovery_file_alloc(const char *file,
					unsigned rep, unsigned part);
int badblocks_recovery_file_exists(struct pool_set *set);

#ifdef __cplusplus
}
#endif

#endif /* PMDK_BADBLOCK_POOLSET_H */
