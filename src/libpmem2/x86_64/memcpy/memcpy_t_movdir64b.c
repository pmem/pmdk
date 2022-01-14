// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "cpu.h"
#include "flush.h"
#include "memcpy_memset.h"
#include "memcpy_movdir64b.h"

void
memmove_mov_movdir64b_noflush(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	#if AVX512F_AVAILABLE
		if (is_cpu_avx512f_present()) {
			memmove_mov_avx512f_noflush(dest, src, len);
			return;
		}
	#endif

	#if AVX_AVAILABLE
		if (is_cpu_avx_present()) {
			memmove_mov_avx_noflush(dest, src, len);
			return;
		}
	#endif

	memmove_mov_sse2_noflush(dest, src, len);
}

void
memmove_mov_movdir64b_empty(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	#if AVX512F_AVAILABLE
		if (is_cpu_avx512f_present()) {
			memmove_mov_avx512f_empty(dest, src, len);
			return;
		}
	#endif

	#if AVX_AVAILABLE
		if (is_cpu_avx_present()) {
			memmove_mov_avx_empty(dest, src, len);
			return;
		}
	#endif

	memmove_mov_sse2_empty(dest, src, len);
}

void
memmove_mov_movdir64b_clflush(char *dest, const char *src, size_t len)
{
	#if AVX512F_AVAILABLE
		if (is_cpu_avx512f_present()) {
			memmove_mov_avx512f_clflush(dest, src, len);
			return;
		}
	#endif

	#if AVX_AVAILABLE
		if (is_cpu_avx_present()) {
			memmove_mov_avx_clflush(dest, src, len);
			return;
		}
	#endif

	memmove_mov_sse2_clflush(dest, src, len);
}

void
memmove_mov_movdir64b_clflushopt(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	#if AVX512F_AVAILABLE
		if (is_cpu_avx512f_present()) {
			memmove_mov_avx512f_clflushopt(dest, src, len);
			return;
		}
	#endif

	#if AVX_AVAILABLE
		if (is_cpu_avx_present()) {
			memmove_mov_avx_clflushopt(dest, src, len);
			return;
		}
	#endif

	memmove_mov_sse2_clflushopt(dest, src, len);
}

void
memmove_mov_movdir64b_clwb(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	#if AVX512F_AVAILABLE
		if (is_cpu_avx512f_present()) {
			memmove_mov_avx512f_clwb(dest, src, len);
			return;
		}
	#endif

	#if AVX_AVAILABLE
		 if (is_cpu_avx_present()) {
			memmove_mov_avx_clwb(dest, src, len);
			return;
		}
	#endif

	memmove_mov_sse2_clwb(dest, src, len);
}
