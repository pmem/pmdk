// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

#define flush64b pmem_clflushopt
#define flush flush_clflushopt_nolog
#define EXPORTED_SYMBOL memmove_mov_avx_clflushopt
#include "memcpy_t_avx.h"
