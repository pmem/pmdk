// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * os_badblock.h -- linux bad block API
 */

#ifndef PMDK_OS_BADBLOCK_H
#define PMDK_OS_BADBLOCK_H 1

#ifdef __cplusplus
extern "C" {
#endif

int badblocks_check_file(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* PMDK_OS_BADBLOCK_H */
