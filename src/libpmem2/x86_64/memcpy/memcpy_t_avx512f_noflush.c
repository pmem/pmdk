// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2020, Intel Corporation */

#define flush64b noflush64b
#define flush noflush
#define EXPORTED_SYMBOL memmove_mov_avx512f_noflush
#include "memcpy_t_avx512f.h"
