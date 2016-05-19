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
 * platform.h -- dirty hacks to compile Linux code on Windows using VC++
 *
 * This is included to each source file using "/FI" (forced include) option.
 *
 * XXX - it is a subject for refactoring
 */

#ifndef PLATFORM_H
#define PLATFORM_H 1

/*
 * Define off_t before windows.h is included!
 * XXX - make sure it has no side-effects
 */
typedef long long off_t;	/* use 64-bit off_t */
typedef long _off_t;		/* NOTE: _off_t must be defined as 'long'! */
#define _OFF_T_DEFINED

#include <windows.h>
#include <stdint.h>
#include <io.h>
#include <process.h>
#include <fcntl.h>
#include <sys/types.h>
#include <malloc.h>

/* use uuid_t definition from util.h */
#ifdef uuid_t
#undef uuid_t
#endif

/* a few trivial substitutions */
#define PATH_MAX MAX_PATH
#define __thread __declspec(thread)
#define __func__ __FUNCTION__
#define __typeof__ decltype

/* XXX - no equivalents in VC++ */
#define __attribute__(a)
#define __builtin_constant_p(cnd) 0

/*
 * atomics
 */

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
__sync_fetch_and_or(volatile uint32_t *a, uint32_t val)
{
	return InterlockedOr((LONG *)a, (LONG)val);
}

__inline uint64_t
__sync_fetch_and_and(volatile uint64_t *a, uint64_t val)
{
	return InterlockedAnd64((LONG64 *)a, (LONG64)val);
}

__inline uint32_t
__sync_fetch_and_add(volatile uint32_t *a, uint32_t val)
{
	return InterlockedExchangeAdd(a, val);
}

__inline uint64_t
__sync_fetch_and_add64(volatile uint64_t *a, uint64_t val)
{
	return InterlockedExchangeAdd64((LONG64 *)a, (LONG64)val);
}

__inline long
__sync_bool_compare_and_swap(volatile uint64_t *ptr,
				uint64_t oldval, uint64_t newval)
{
	uint64_t old = InterlockedCompareExchange64((volatile LONG64 *)ptr,
		(LONG64)newval, (LONG64)oldval);
	return (old == oldval);
}

__inline void
__sync_synchronize()
{
	MemoryBarrier();
}

/*
 * missing definitions
 */

/* sys/stat.h */
#define S_IRUSR S_IREAD
#define S_IWUSR S_IWRITE

typedef int mode_t;

#define fchmod(fd, mode) 0	/* XXX - dummy */
#define setlinebuf(fp) setvbuf(fp, NULL, _IOLBF, BUFSIZ);

/* unistd.h */
typedef long long int ssize_t;

/* stdlib.h */
int mkstemp(char *temp);

/* fcntl.h */
int posix_fallocate(int fd, off_t offset, off_t size);

/* string.h */
#define strtok_r strtok_s

/* sched.h */

/*
 * sched_yield -- yield the processor
 */
__inline int
sched_yield(void)
{
	SwitchToThread();
	return 0;		/* always succeeds */
}

/*
 * helper macros for library ctor/dtor function declarations
 */
#define MSVC_CONSTR(func) \
void func(void); \
__pragma(comment(linker, "/include:_" #func)) \
__pragma(section(".CRT$XCU", read)) \
__declspec(allocate(".CRT$XCU")) \
const void (WINAPI *_##func)(void) = func;

#define MSVC_DESTR(func) \
void func(void); \
static void _##func##_reg(void) { atexit(func); }; \
MSVC_CONSTR(_##func##_reg)

#endif /* PLATFORM_H */
