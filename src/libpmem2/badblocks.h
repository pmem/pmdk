// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2020, Intel Corporation */

/*
 * badblocks.h -- bad blocks API based on the ndctl library
 */

#ifndef PMDK_BADBLOCKS_H
#define PMDK_BADBLOCKS_H 1

#include <string.h>
#include <stdint.h>

#include "os_badblock.h"

#ifdef __cplusplus
extern "C" {
#endif

struct badblocks *badblocks_new(void);
void badblocks_delete(struct badblocks *bbs);

int badblocks_files_namespace_badblocks(const char *path,
	struct badblocks *bbs);
int badblocks_devdax_clear_badblocks_all(const char *path);
int badblocks_devdax_clear_badblocks(const char *path, struct badblocks *bbs);

#ifdef __cplusplus
}
#endif

#endif /* PMDK_BADBLOCKS_H */
