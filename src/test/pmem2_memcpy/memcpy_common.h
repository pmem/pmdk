/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020-2021, Intel Corporation */

/*
 * memcpy_common.h -- header file for common memcpy utilities
 */
#ifndef MEMCPY_COMMON_H
#define MEMCPY_COMMON_H 1

#include "unittest.h"
#include "file.h"

typedef void *(*memcpy_fn)(void *pmemdest, const void *src, size_t len,
		unsigned flags);
typedef void *(*set_memcpy_fn)(struct pmemset *set, void *pmemdest,
		const void *src, size_t len, unsigned flags);

typedef void (*persist_fn)(const void *ptr, size_t len);
typedef int (*set_persist_fn)(struct pmemset *set, const void *ptr, size_t len);

extern unsigned Flags[10];

void do_memcpy(int fd, char *dest, int dest_off, char *src, int src_off,
    size_t bytes, size_t mapped_len, const char *file_name, memcpy_fn fn,
    unsigned flags, persist_fn p, struct pmemset *set,
    set_persist_fn sp, set_memcpy_fn sm);

#endif
