/*
 * Copyright 2020, Intel Corporation
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
 * movnt_align_common.h -- header file for common movnt_align test utilities
 */
#ifndef MOVNT_ALIGN_COMMON_H
#define MOVNT_ALIGN_COMMON_H 1

#include "unittest.h"
#include "file.h"

#define CACHELINE 64
#define N_BYTES (Ut_pagesize * 2)

extern char *Src;
extern char *Dst;
extern char *Scratch;

extern unsigned Flags[10];

typedef void *(*mem_fn)(void *, const void *, size_t);

typedef void *pmem_memcpy_fn(void *pmemdest, const void *src, size_t len,
		unsigned flags);
typedef void *pmem_memmove_fn(void *pmemdest, const void *src, size_t len,
		unsigned flags);
typedef void *pmem_memset_fn(void *pmemdest, int c, size_t len, unsigned flags);

void check_memmove(size_t doff, size_t soff, size_t len, pmem_memmove_fn fn,
	unsigned flags);
void check_memcpy(size_t doff, size_t soff, size_t len, pmem_memcpy_fn fn,
	unsigned flags);
void check_memset(size_t off, size_t len, pmem_memset_fn fn, unsigned flags);

#endif
