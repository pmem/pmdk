// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

#define flush flush_clflush_nolog
#define EXPORTED_SYMBOL memset_movnt_avx512f_clflush
#define maybe_barrier barrier_after_ntstores
#include "memset_nt_avx512f.h"
