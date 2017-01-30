/*
 * Copyright 2014-2017, Intel Corporation
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
 * mmap.h -- internal definitions for mmap module
 */

#ifndef NVML_MMAP_H
#define NVML_MMAP_H 1
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "out.h"

extern int Mmap_no_random;
extern void *Mmap_hint;

void *util_map(int fd, size_t len, int flags, int rdonly, size_t req_align);
int util_unmap(void *addr, size_t len);

void *util_map_tmpfile(const char *dir, size_t size, size_t req_align);

/*
 * macros for micromanaging range protections for the debug version
 */
#ifdef DEBUG

#define RANGE(addr, len, is_dax, type) do {\
	if (!is_dax) ASSERT(util_range_##type(addr, len) >= 0);\
} while (0)

#else

#define RANGE(addr, len, is_dax, type) do {} while (0)

#endif

#define RANGE_RO(addr, len, is_dax) RANGE(addr, len, is_dax, ro)
#define RANGE_RW(addr, len, is_dax) RANGE(addr, len, is_dax, rw)
#define RANGE_NONE(addr, len, is_dax) RANGE(addr, len, is_dax, none)


void util_mmap_init(void);

int util_range_ro(void *addr, size_t len);
int util_range_rw(void *addr, size_t len);
int util_range_none(void *addr, size_t len);

char *util_map_hint_unused(void *addr, size_t len, size_t align);
char *util_map_hint(size_t len, size_t req_align);

#define MEGABYTE ((uintptr_t)1 << 20)
#define GIGABYTE ((uintptr_t)1 << 30)

/*
 * util_map_hint_align -- choose the desired mapping alignment
 *
 * Use 2MB/1GB page alignment only if the mapping length is at least
 * twice as big as the page size.
 */
static inline size_t
util_map_hint_align(size_t len, size_t req_align)
{
	size_t align = Mmap_align;
	if (req_align)
		align = req_align;
	else if (len >= 2 * GIGABYTE)
		align = GIGABYTE;
	else if (len >= 4 * MEGABYTE)
		align = 2 * MEGABYTE;
	return align;
}

#ifdef __cplusplus
}
#endif
#endif
