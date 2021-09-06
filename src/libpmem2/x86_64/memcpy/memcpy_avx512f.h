/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2017-2020, Intel Corporation */

#ifndef PMEM2_MEMCPY_AVX512F_H
#define PMEM2_MEMCPY_AVX512F_H

#include <stddef.h>

#include "memcpy_avx.h"

static force_inline void
memmove_small_avx512f(char *dest, const char *src, size_t len, flush_fn flush)
{
	/* We can't do better than AVX here. */
	memmove_small_avx(dest, src, len, flush);
}

#endif
