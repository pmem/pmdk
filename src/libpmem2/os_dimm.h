// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2020, Intel Corporation */

/*
 * os_dimm.h -- DIMMs API based on the ndctl library
 */

#ifndef PMDK_OS_DIMM_H
#define PMDK_OS_DIMM_H 1

#include <string.h>
#include <stdint.h>

#include "os_badblock.h"

#ifdef __cplusplus
extern "C" {
#endif

int os_dimm_files_namespace_badblocks(const char *path, struct badblocks *bbs);
int os_dimm_devdax_clear_badblocks_all(const char *path);
int os_dimm_devdax_clear_badblocks(const char *path, struct badblocks *bbs);

#ifdef __cplusplus
}
#endif

#endif
