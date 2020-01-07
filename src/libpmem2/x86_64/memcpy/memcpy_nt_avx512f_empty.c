// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

#define flush flush_empty_nolog
#define EXPORTED_SYMBOL memmove_movnt_avx512f_empty
#define maybe_barrier barrier_after_ntstores
#include "memcpy_nt_avx512f.h"
