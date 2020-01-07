// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

#define flush64b flush64b_empty
#define flush flush_empty_nolog
#define EXPORTED_SYMBOL memset_mov_avx_empty
#include "memset_t_avx.h"
