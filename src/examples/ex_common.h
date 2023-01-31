/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2023, Intel Corporation */

/*
 * ex_common.h -- examples utilities
 */
#ifndef EX_COMMON_H
#define EX_COMMON_H

#include <stdint.h>

#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>

#define CREATE_MODE_RW (S_IWUSR | S_IRUSR)

/*
 * file_exists -- checks if file exists
 */
static inline int
file_exists(char const *file)
{
	return access(file, F_OK);
}

/*
 * find_last_set_64 -- returns last set bit position or -1 if set bit not found
 */
static inline int
find_last_set_64(uint64_t val)
{
	return 64 - __builtin_clzll(val) - 1;
}

#ifdef __cplusplus
}
#endif
#endif /* ex_common.h */
