// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

#define flush64b pmem_clwb
#define flush flush_clwb_nolog
#define EXPORTED_SYMBOL memset_mov_sse2_clwb
#include "memset_t_sse2.h"
