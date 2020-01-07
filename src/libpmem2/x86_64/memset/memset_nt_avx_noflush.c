// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2020, Intel Corporation */

#define flush noflush
#define EXPORTED_SYMBOL memset_movnt_avx_noflush
#define maybe_barrier barrier_after_ntstores
#include "memset_nt_avx.h"
