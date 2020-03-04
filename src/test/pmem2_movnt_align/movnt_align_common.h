// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * movnt_align_common.h -- header file for common movnt_align test utilities
 */
#ifndef MOVNT_ALIGN_COMMON_H
#define MOVNT_ALIGN_COMMON_H 1

#include "unittest.h"
#include "file.h"

#define N_BYTES (Ut_pagesize * 2)

extern char *Src;
extern char *Dst;
extern char *Scratch;

extern unsigned Flags[10];

typedef void *(*mem_fn)(void *, const void *, size_t);

typedef void *pmem_memcpy_fn(void *pmemdest, const void *src, size_t len,
		unsigned flags);
typedef void *pmem_memmove_fn(void *pmemdest, const void *src, size_t len,
		unsigned flags);
typedef void *pmem_memset_fn(void *pmemdest, int c, size_t len, unsigned flags);

void check_memmove(size_t doff, size_t soff, size_t len, pmem_memmove_fn fn,
	unsigned flags);
void check_memcpy(size_t doff, size_t soff, size_t len, pmem_memcpy_fn fn,
	unsigned flags);
void check_memset(size_t off, size_t len, pmem_memset_fn fn, unsigned flags);

#endif
