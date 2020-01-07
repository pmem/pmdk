// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

#define flush flush_clflushopt_nolog
#define EXPORTED_SYMBOL memmove_movnt_sse2_clflushopt
#define maybe_barrier no_barrier_after_ntstores
#include "memcpy_nt_sse2.h"
