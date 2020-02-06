// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * memset_common.h -- header file for common memset utilities
 */
#ifndef MEMSET_COMMON_H
#define MEMSET_COMMON_H 1

#include "unittest.h"
#include "file.h"

typedef void *(*memset_fn)(void *pmemdest, int c, size_t len, unsigned flags);

extern union persist {
	int is_pmem;
	pmem2_persist_fn persist_fn;
} persist;

extern unsigned Flags[10];

void
do_memset(int fd, char *dest, const char *file_name, size_t dest_off,
	size_t bytes, memset_fn fn, unsigned flags, union persist p);

void do_persist(union persist p, const void *addr, size_t len);

#endif
