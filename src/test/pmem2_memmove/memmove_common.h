// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * memmove_common.h -- header file for common memmove_common test utilities
 */
#ifndef MEMMOVE_COMMON_H
#define MEMMOVE_COMMON_H 1

#include "unittest.h"
#include "file.h"

extern unsigned Flags[10];

#define USAGE() do { UT_FATAL("usage: %s file  b:length [d:{offset}] "\
	"[s:{offset}] [o:{0|1}]", argv[0]); } while (0)

extern union persist {
	int is_pmem;
	pmem2_persist_fn persist_fn;
} persist;

typedef void *(*memmove_fn)(void *pmemdest, const void *src, size_t len,
		unsigned flags);

void do_memmove(char *dst, char *src, const char *file_name,
		size_t dest_off, size_t src_off, size_t bytes,
		memmove_fn fn, unsigned flags, union persist p);

void verify_contents(const char *file_name, int test, const char *buf1,
		const char *buf2, size_t len);

void do_persist(union persist p, const void *addr, size_t len);

#endif
