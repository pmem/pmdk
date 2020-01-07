// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

#define flush64b pmem_clwb
#define flush flush_clwb_nolog
#define EXPORTED_SYMBOL memmove_mov_avx512f_clwb
#include "memcpy_t_avx512f.h"
