// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "cpu.h"
#include "flush.h"
#include "memcpy_memset.h"
#include "memset_movdir64b.h"

void
memset_mov_movdir64b_noflush(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	#if AVX512F_AVAILABLE
		if (is_cpu_avx512f_present()) {
			memset_mov_avx512f_noflush(dest, c, len);
			return;
		}
	#endif

	#if AVX_AVAILABLE
		if (is_cpu_avx_present()) {
			memset_mov_avx_noflush(dest, c, len);
			return;
		}
	#endif

	memset_mov_sse2_noflush(dest, c, len);
}

void
memset_mov_movdir64b_empty(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	#if AVX512F_AVAILABLE
		if (is_cpu_avx512f_present()) {
			memset_mov_avx512f_empty(dest, c, len);
			return;
		}
	#endif

	#if AVX_AVAILABLE
		if (is_cpu_avx_present()) {
			memset_mov_avx_empty(dest, c, len);
			return;
		}
	#endif

	memset_mov_sse2_empty(dest, c, len);
}

void
memset_mov_movdir64b_clflush(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	#if AVX512F_AVAILABLE
		if (is_cpu_avx512f_present()) {
			memset_mov_avx512f_clflush(dest, c, len);
			return;
		}
	#endif

	#if AVX_AVAILABLE
		if (is_cpu_avx_present()) {
			memset_mov_avx_clflush(dest, c, len);
			return;
		}
	#endif

	memset_mov_sse2_clflush(dest, c, len);
}

void
memset_mov_movdir64b_clflushopt(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	#if AVX512F_AVAILABLE
		if (is_cpu_avx512f_present()) {
			memset_mov_avx512f_clflushopt(dest, c, len);
			return;
		}
	#endif

	#if AVX_AVAILABLE
		if (is_cpu_avx_present()) {
			memset_mov_avx_clflushopt(dest, c, len);
			return;
		}
	#endif

	memset_mov_sse2_clflushopt(dest, c, len);
}

void
memset_mov_movdir64b_clwb(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	#if AVX512F_AVAILABLE
		if (is_cpu_avx512f_present()) {
			memset_mov_avx512f_clwb(dest, c, len);
			return;
		}
	#endif

	#if AVX_AVAILABLE
		if (is_cpu_avx_present()) {
			memset_mov_avx_clwb(dest, c, len);
			return;
		}
	#endif

	memset_mov_sse2_clwb(dest, c, len);
}
