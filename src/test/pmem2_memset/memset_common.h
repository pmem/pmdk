// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * memset_common.h -- header file for common memset utilities
 */
#ifndef MEMSET_COMMON_H
#define MEMSET_COMMON_H 1

#include "unittest.h"
#include "file.h"

extern unsigned Flags[10];

typedef void *(*memset_fn)(void *pmemdest, int c, size_t len, unsigned flags);

typedef void (*persist_fn)(const void *ptr, size_t len);

void
do_memset(int fd, char *dest, const char *file_name, size_t dest_off,
	size_t bytes, memset_fn fn, unsigned flags, persist_fn p);

#endif
