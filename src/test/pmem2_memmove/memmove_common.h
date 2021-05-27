/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020-2021, Intel Corporation */

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

typedef void *(*memmove_fn)(void *pmemdest, const void *src, size_t len,
		unsigned flags);
typedef void *(*set_memmove_fn)(struct pmemset *set, void *pmemdest,
		void *src, size_t len, unsigned flags);

typedef void (*persist_fn)(const void *ptr, size_t len);
typedef int (*set_persist_fn)(struct pmemset *set, void *ptr, size_t len);

void do_memmove(char *dst, char *src, const char *file_name,
		size_t dest_off, size_t src_off, size_t bytes,
		memmove_fn fn, unsigned flags, persist_fn p,
		struct pmemset *set, set_persist_fn sp,
		set_memmove_fn sm);

void verify_contents(const char *file_name, int test, const char *buf1,
		const char *buf2, size_t len);

#endif
