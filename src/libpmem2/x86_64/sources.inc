# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2020, Intel Corporation

#
# src/libpmem2/x86_64/sources.inc -- list of files for libpmem2/x86_64
#

LIBPMEM2_ARCH_SOURCE = init.c\
	cpu.c\
	memcpy_nt_avx_clflush.c\
	memcpy_nt_avx_clflushopt.c\
	memcpy_nt_avx_clwb.c\
	memcpy_nt_avx_empty.c\
	memcpy_nt_avx_noflush.c\
	memcpy_nt_sse2_clflush.c\
	memcpy_nt_sse2_clflushopt.c\
	memcpy_nt_sse2_clwb.c\
	memcpy_nt_sse2_empty.c\
	memcpy_nt_sse2_noflush.c\
	memset_nt_avx_clflush.c\
	memset_nt_avx_clflushopt.c\
	memset_nt_avx_clwb.c\
	memset_nt_avx_empty.c\
	memset_nt_avx_noflush.c\
	memset_nt_sse2_clflush.c\
	memset_nt_sse2_clflushopt.c\
	memset_nt_sse2_clwb.c\
	memset_nt_sse2_empty.c\
	memset_nt_sse2_noflush.c\
	memcpy_t_avx_clflush.c\
	memcpy_t_avx_clflushopt.c\
	memcpy_t_avx_clwb.c\
	memcpy_t_avx_empty.c\
	memcpy_t_avx_noflush.c\
	memcpy_t_sse2_clflush.c\
	memcpy_t_sse2_clflushopt.c\
	memcpy_t_sse2_clwb.c\
	memcpy_t_sse2_empty.c\
	memcpy_t_sse2_noflush.c\
	memset_t_avx_clflush.c\
	memset_t_avx_clflushopt.c\
	memset_t_avx_clwb.c\
	memset_t_avx_empty.c\
	memset_t_avx_noflush.c\
	memset_t_sse2_clflush.c\
	memset_t_sse2_clflushopt.c\
	memset_t_sse2_clwb.c\
	memset_t_sse2_empty.c\
	memset_t_sse2_noflush.c

AVX512F_PROG="\#include <immintrin.h>\n\#include <stdint.h>\nint main(){ uint64_t v[8]; __m512i zmm0 = _mm512_loadu_si512((__m512i *)&v); return 0;}"
AVX512F_AVAILABLE := $(shell printf $(AVX512F_PROG) |\
	$(CC) $(CFLAGS) -x c -mavx512f -o /dev/null - 2>/dev/null && echo y || echo n)

ifeq ($(AVX512F_AVAILABLE), y)
LIBPMEM2_ARCH_SOURCE += memcpy_nt_avx512f_clflush.c\
	memcpy_nt_avx512f_clflushopt.c\
	memcpy_nt_avx512f_clwb.c\
	memcpy_nt_avx512f_empty.c\
	memcpy_nt_avx512f_noflush.c\
	memset_nt_avx512f_clflush.c\
	memset_nt_avx512f_clflushopt.c\
	memset_nt_avx512f_clwb.c\
	memset_nt_avx512f_empty.c\
	memset_nt_avx512f_noflush.c\
	memcpy_t_avx512f_clflush.c\
	memcpy_t_avx512f_clflushopt.c\
	memcpy_t_avx512f_clwb.c\
	memcpy_t_avx512f_empty.c\
	memcpy_t_avx512f_noflush.c\
	memset_t_avx512f_clflush.c\
	memset_t_avx512f_clflushopt.c\
	memset_t_avx512f_clwb.c\
	memset_t_avx512f_empty.c\
	memset_t_avx512f_noflush.c
endif

