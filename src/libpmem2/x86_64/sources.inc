# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2022, Intel Corporation

#
# src/libpmem2/x86_64/sources.inc -- list of files for libpmem2/x86_64
#

LIBPMEM2_ARCH_SOURCE = init.c\
	cpu.c\
	memcpy_nt_avx.c\
	memcpy_nt_sse2.c\
	memset_nt_avx.c\
	memset_nt_sse2.c\
	memcpy_t_avx.c\
	memcpy_t_sse2.c\
	memset_t_avx.c\
	memset_t_sse2.c

AVX512F_PROG="\#include <immintrin.h>\n\#include <stdint.h>\nint main(){ uint64_t v[8]; __m512i zmm0 = _mm512_loadu_si512((__m512i *)&v); return 0;}"
AVX512F_AVAILABLE := $(shell printf $(AVX512F_PROG) |\
	$(CC) $(CFLAGS) -x c -mavx512f -o /dev/null - 2>/dev/null && echo y || echo n)

ifeq ($(AVX512F_AVAILABLE), y)
LIBPMEM2_ARCH_SOURCE += \
	memcpy_nt_avx512f.c\
	memset_nt_avx512f.c\
	memcpy_t_avx512f.c\
	memset_t_avx512f.c
endif

MOVDIR64B_PROG="\#include <immintrin.h>\n\#include <stdint.h>\nint main(){ uint64_t v, w; _movdir64b(&v, &w); return 0;}"
MOVDIR64B_AVAILABLE := $(shell printf $(MOVDIR64B_PROG) |\
	$(CC) $(CFLAGS) -x c -mmovdir64b -o /dev/null - 2>/dev/null && echo y || echo n)
MOVDIR64B_AVAILABLE=n # XXX delete this when movdir64b is ready to be upstreamed

ifeq ($(MOVDIR64B_AVAILABLE), y)
LIBPMEM2_ARCH_SOURCE += \
	memcpy_nt_movdir64b.c\
	memset_nt_movdir64b.c
endif
