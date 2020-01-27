// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * memcpy_common.h -- header file for common memcpy utilities
 */
#ifndef MEMCPY_COMMON_H
#define MEMCPY_COMMON_H 1

#include "unittest.h"
#include "file.h"

typedef void *(*memcpy_fn)(void *pmemdest, const void *src, size_t len,
		unsigned flags);

extern union persist {
	int is_pmem;
	pmem2_persist_fn persist_fn;
} persist;

extern unsigned Flags[10];

void do_memcpy(int fd, char *dest, int dest_off, char *src, int src_off,
    size_t bytes, size_t mapped_len, const char *file_name, memcpy_fn fn,
    unsigned flags, union persist p);

void do_persist(union persist p, const void *addr, size_t len);

#endif
