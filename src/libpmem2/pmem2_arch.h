/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2022, Intel Corporation */

/*
 * pmem2_arch.h -- core-arch interface
 */
#ifndef PMEM2_ARCH_H
#define PMEM2_ARCH_H

#include <stddef.h>
#include "libpmem2.h"
#include "util.h"
#include "valgrind_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

struct pmem2_arch_info;
struct memmove_nodrain;
struct memset_nodrain;

typedef void (*fence_func)(void);
typedef void (*flush_func)(const void *, size_t);
typedef void *(*memmove_nodrain_func)(void *pmemdest, const void *src,
		size_t len, unsigned flags, flush_func flush,
		const struct memmove_nodrain *memmove_funcs);
typedef void *(*memset_nodrain_func)(void *pmemdest, int c, size_t len,
		unsigned flags, flush_func flush,
		const struct memset_nodrain *memset_funcs);
typedef void (*memmove_func)(char *pmemdest, const char *src, size_t len);
typedef void (*memset_func)(char *pmemdest, int c, size_t len);

struct memmove_nodrain {
	struct {
		memmove_func noflush;
		memmove_func flush;
		memmove_func empty;
	} t; /* temporal */
	struct {
		memmove_func noflush;
		memmove_func flush;
		memmove_func empty;
	} nt; /* nontemporal */
};

struct memset_nodrain {
	struct {
		memset_func noflush;
		memset_func flush;
		memset_func empty;
	} t; /* temporal */
	struct {
		memset_func noflush;
		memset_func flush;
		memset_func empty;
	} nt; /* nontemporal */
};

struct pmem2_arch_info {
	struct memmove_nodrain memmove_funcs;
	struct memset_nodrain memset_funcs;
	memmove_nodrain_func memmove_nodrain;
	memmove_nodrain_func memmove_nodrain_eadr;
	memset_nodrain_func memset_nodrain;
	memset_nodrain_func memset_nodrain_eadr;
	flush_func flush;
	fence_func fence;
	int flush_has_builtin_fence;
};

void pmem2_arch_init(struct pmem2_arch_info *info);

/*
 * flush_empty_nolog -- (internal) do not flush the CPU cache
 */
static force_inline void
flush_empty_nolog(const void *addr, size_t len)
{
	/* NOP, but tell pmemcheck about it */
	VALGRIND_DO_FLUSH(addr, len);
}

void *memmove_nodrain_generic(void *pmemdest, const void *src, size_t len,
		unsigned flags, flush_func flush,
		const struct memmove_nodrain *memmove_funcs);
void *memset_nodrain_generic(void *pmemdest, int c, size_t len, unsigned flags,
		flush_func flush, const struct memset_nodrain *memset_funcs);

#ifdef __cplusplus
}
#endif

#endif
