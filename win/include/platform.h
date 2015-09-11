/*
 * Copyright (c) 2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * platform.h -- dirty hacks to compile linux code on windows
 */

#include <windows.h>
#include <stdint.h>
#include <io.h>
#include <process.h>

#define	PATH_MAX MAX_PATH
#define	__thread __declspec(thread)
#define	__attribute__(a)
#define	__func__ __FUNCTION__
#define	restrict __restrict
#define	mode_t int
#define	ssize_t long long int


__inline int
__builtin_clzll(uint64_t val)
{
	unsigned long lz = 0;

	if (_BitScanReverse64(&lz, val))
		return 63 - (int)lz;
	else
		return 64;
}

__inline uint64_t
__sync_fetch_and_and(volatile uint64_t *a, uint64_t val) {
	return _InterlockedAnd64(a, val);
}

__inline uint64_t
__sync_fetch_and_add(volatile uint64_t *a, uint64_t val) {
	return _InterlockedExchangeAdd64(a, val);
}

__inline int
__sync_bool_compare_and_swap(volatile uint64_t *ptr, uint64_t oldval, uint64_t newval) {
	return (int)_InterlockedCompareExchange64(ptr, oldval, newval);
}

#define	LOCK_EX 0
#define	LOCK_NB 0

#define	flock(fd, flags) 0
#define	fchmod(fd, mode) 0
#define	setlinebuf(o)
#define	strsep(line, sep) NULL
#define	mkstemp(n) 0
#define	posix_fallocate(fd, p, size) 0
