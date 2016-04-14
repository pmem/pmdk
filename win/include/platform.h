/*
 * Copyright 2015-2016, Intel Corporation
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
 * platform.h -- dirty hacks to compile linux code on windows
 */

#pragma once

/* use 64-bit off_t */
typedef long long off_t;
typedef long _off_t; /* DO NOT override _off_t definition !!! */
#define	_OFF_T_DEFINED

#include <windows.h>
#include <stdint.h>
#include <io.h>
#include <process.h>
#include <listentry.h>
#include <fcntl.h>
#include <sys/types.h>
#include <malloc.h>



#define	PATH_MAX MAX_PATH
#define	__thread __declspec(thread)
#define	__attribute__(a)
#define	__func__ __FUNCTION__
#define	restrict __restrict
#define	typeof decltype
#define	__typeof__ decltype

typedef int mode_t;
typedef long long int ssize_t;

/* XXX - dummy */
typedef int sigset_t;
typedef int DIR;


__inline int
__builtin_clzll(uint64_t val)
{
	DWORD lz = 0;

	if (BitScanReverse64(&lz, val))
		return 63 - (int)lz;
	else
		return 64;
}

__inline uint32_t
__sync_fetch_and_or(volatile uint32_t *a, uint32_t val) {
	return InterlockedOr((LONG *)a, (LONG)val);
}

__inline uint64_t
__sync_fetch_and_and(volatile uint64_t *a, uint64_t val) {
	return InterlockedAnd64((LONG64 *)a, (LONG64)val);
}

__inline uint32_t
__sync_fetch_and_add(volatile uint32_t *a, uint32_t val) {
	return InterlockedExchangeAdd(a, val);
}

__inline uint64_t
__sync_fetch_and_add64(volatile uint64_t *a, uint64_t val) {
	return InterlockedExchangeAdd64((LONG64 *)a, (LONG64)val);
}

__inline long
__sync_bool_compare_and_swap(volatile uint64_t *ptr,
				uint64_t oldval, uint64_t newval) {
	uint64_t old = InterlockedCompareExchange64((volatile LONG64 *)ptr,
		(LONG64)newval, (LONG64)oldval);
	return (old == oldval);
}

__inline void
__sync_synchronize()
{
	MemoryBarrier();
}


/* sys/stat.h */
#define	S_IRUSR S_IREAD
#define	S_IWUSR S_IWRITE


#define	fchmod(fd, mode) 0
#define	setlinebuf(o)
#define	strsep(line, sep) NULL

#define unlink _unlink

#define	SIG_BLOCK 0
#define	SIG_SETMASK 0


__inline int
sigfillset(sigset_t *set)
{
	(void) set;
	return 0;
}

__inline int
sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
	(void) how;
	(void) set;
	(void) oldset;
	return 0;
}


int mkstemp(char *temp);
int posix_fallocate(int fd, off_t offset, off_t size);


/*
 * helper macros for library constructor/destructor functions declaration
 */
#define MSVC_CONSTR(func) \
__pragma(comment(linker,"/include:_" #func)) \
__pragma(section(".CRT$XCU",read)) \
__declspec(allocate(".CRT$XCU")) \
const void (WINAPI *_##func)(void) = func;

#define MSVC_DESTR(func) \
static void _##func##_reg(void) { atexit(func); }; \
MSVC_CONSTR(_##func##_reg)
