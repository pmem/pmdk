/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2017-2020, Intel Corporation */

#ifndef PMEM2_MEMSET_AVX512F_H
#define PMEM2_MEMSET_AVX512F_H

#include <stddef.h>

#include "memset_avx.h"

static force_inline void
memset_small_avx512f(char *dest, __m256i ymm, size_t len, flush_fn flush)
{
	/* We can't do better than AVX here. */
	memset_small_avx(dest, ymm, len, flush);
}

#endif
