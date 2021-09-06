/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2018-2020, Intel Corporation */

/*
 * set_badblocks.h - poolset part of bad blocks API
 */

#ifndef PMDK_SET_BADBLOCKS_H
#define PMDK_SET_BADBLOCKS_H 1

#include "set.h"

#ifdef __cplusplus
extern "C" {
#endif

int badblocks_check_poolset(struct pool_set *set, int create);
int badblocks_clear_poolset(struct pool_set *set, int create);

char *badblocks_recovery_file_alloc(const char *file,
					unsigned rep, unsigned part);
int badblocks_recovery_file_exists(struct pool_set *set);

#ifdef __cplusplus
}
#endif

#endif /* PMDK_SET_BADBLOCKS_H */
