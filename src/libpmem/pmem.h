/*
 * Copyright 2014-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * pmem.h -- internal definitions for libpmem
 */

#define PMEM_LOG_PREFIX "libpmem"
#define PMEM_LOG_LEVEL_VAR "PMEM_LOG_LEVEL"
#define PMEM_LOG_FILE_VAR "PMEM_LOG_FILE"

void pmem_init(void);

int is_pmem_detect(const void *addr, size_t len);
void *pmem_map_register(int fd, size_t len, const char *path, int is_dev_dax);

#ifndef AVX512F_AVAILABLE
#ifdef _MSC_VER
#define AVX512F_AVAILABLE 0
#else
#define AVX512F_AVAILABLE 1
#endif
#endif

#ifndef AVX_AVAILABLE
#define AVX_AVAILABLE 1
#endif

#ifndef SSE2_AVAILABLE
#define SSE2_AVAILABLE 1
#endif

#if SSE2_AVAILABLE
void memmove_mov_sse2(char *dest, const char *src, size_t len);
void memmove_movnt_sse2(char *dest, const char *src, size_t len);
void memset_mov_sse2(char *dest, int c, size_t len);
void memset_movnt_sse2(char *dest, int c, size_t len);
#endif

#if AVX_AVAILABLE
void memmove_mov_avx(char *dest, const char *src, size_t len);
void memmove_movnt_avx(char *dest, const char *src, size_t len);
void memset_mov_avx(char *dest, int c, size_t len);
void memset_movnt_avx(char *dest, int c, size_t len);
#endif

#if AVX512F_AVAILABLE
void memmove_mov_avx512f(char *dest, const char *src, size_t len);
void memmove_movnt_avx512f(char *dest, const char *src, size_t len);
void memset_mov_avx512f(char *dest, int c, size_t len);
void memset_movnt_avx512f(char *dest, int c, size_t len);
#endif

extern size_t Movnt_threshold;

#if defined(_WIN32) && (NTDDI_VERSION >= NTDDI_WIN10_RS1)
typedef BOOL (WINAPI *PQVM)(
		HANDLE, const void *,
		enum WIN32_MEMORY_INFORMATION_CLASS, PVOID,
		SIZE_T, PSIZE_T);

extern PQVM Func_qvmi;
#endif
