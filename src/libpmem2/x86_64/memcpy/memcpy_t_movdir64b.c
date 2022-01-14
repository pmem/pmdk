// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "flush.h"
#include "memcpy_memset.h"
#include "memcpy_movdir64b.h"

void
memmove_mov_movdir64b_noflush(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_avx512f_noflush(dest, src, len);
}

void
memmove_mov_movdir64b_empty(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_avx512f_empty(dest, src, len);
}

void
memmove_mov_movdir64b_clflush(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_avx512f_clflush(dest, src, len);
}

void
memmove_mov_movdir64b_clflushopt(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_avx512f_clflushopt(dest, src, len);
}

void
memmove_mov_movdir64b_clwb(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_avx512f_clwb(dest, src, len);
}
