// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

#define flush flush_empty_nolog
#define EXPORTED_SYMBOL memmove_movnt_sse2_empty
#define maybe_barrier barrier_after_ntstores
#include "memcpy_nt_sse2.h"
