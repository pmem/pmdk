// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

#define flush64b _mm_clflush
#define flush flush_clflush_nolog
#define EXPORTED_SYMBOL memmove_mov_avx512f_clflush
#include "memcpy_t_avx512f.h"
