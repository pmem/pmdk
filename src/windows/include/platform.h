/*
 * Copyright 2015-2019, Intel Corporation
 * Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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

#pragma warning(disable : 4996)
#pragma warning(disable : 4200) /* allow flexible array member */
#pragma warning(disable : 4819) /* non unicode characteres */

#ifdef __cplusplus
extern "C" {
#endif

/* Prevent PMDK compilation for 32-bit platforms */
#if defined(_WIN32) && !defined(_WIN64)
#error "32-bit builds of PMDK are not supported!"
#endif

#define _CRT_RAND_S		/* rand_s() */

#include <windows.h>
#include <stdint.h>
#include <time.h>
#include <io.h>
#include <process.h>
#include <fcntl.h>
#include <sys/types.h>
#include <malloc.h>
#include <signal.h>
#include <intrin.h>
#include <direct.h>

/* use uuid_t definition from util.h */
#ifdef uuid_t
#undef uuid_t
#endif

/* a few trivial substitutions */
#define PATH_MAX MAX_PATH
#define __thread __declspec(thread)
#define __func__ __FUNCTION__
#ifdef _DEBUG
#define DEBUG
#endif

/*
 * The inline keyword is available only in VC++.
 * https://msdn.microsoft.com/en-us/library/bw1hbe6y.aspx
 */
#ifndef __cplusplus
#define inline __inline
#endif

/* XXX - no equivalents in VC++ */
#define __attribute__(a)
#define __builtin_constant_p(cnd) 0

/*
 * missing definitions
 */

/* errno.h */
#define ELIBACC 79 /* cannot access a needed shared library */

/* sys/stat.h */
#define S_IRUSR S_IREAD
#define S_IWUSR S_IWRITE
#define S_IRGRP S_IRUSR
#define S_IWGRP S_IWUSR

#define O_SYNC 0

typedef int mode_t;

#define fchmod(fd, mode) 0	/* XXX - dummy */
#define setlinebuf(fp) setvbuf(fp, NULL, _IOLBF, BUFSIZ);

/* unistd.h */
typedef long long os_off_t;
typedef long long ssize_t;

int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);

/* fcntl.h */
int posix_fallocate(int fd, os_off_t offset, os_off_t len);

/* string.h */
#define strtok_r strtok_s

/* time.h */
#define CLOCK_MONOTONIC 1
#define CLOCK_REALTIME 2

int clock_gettime(int id, struct timespec *ts);

/* signal.h */
typedef unsigned long long sigset_t; /* one bit for each signal */
C_ASSERT(NSIG <= sizeof(sigset_t) * 8);

struct sigaction {
	void (*sa_handler) (int signum);
	/* void (*sa_sigaction)(int, siginfo_t *, void *); */
	sigset_t sa_mask;
	int sa_flags;
	void (*sa_restorer) (void);
};

__inline int
sigemptyset(sigset_t *set)
{
	*set = 0;
	return 0;
}

__inline int
sigfillset(sigset_t *set)
{
	*set = ~0;
	return 0;
}

__inline int
sigaddset(sigset_t *set, int signum)
{
	if (signum <= 0 || signum >= NSIG) {
		errno = EINVAL;
		return -1;
	}
	*set |= (1ULL << (signum - 1));
	return 0;
}

__inline int
sigdelset(sigset_t *set, int signum)
{
	if (signum <= 0 || signum >= NSIG) {
		errno = EINVAL;
		return -1;
	}
	*set &= ~(1ULL << (signum - 1));
	return 0;
}

__inline int
sigismember(const sigset_t *set, int signum)
{
	if (signum <= 0 || signum >= NSIG) {
		errno = EINVAL;
		return -1;
	}
	return ((*set & (1ULL << (signum - 1))) ? 1 : 0);
}

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
const void (WINAPI *_##func)(void) = (const void (WINAPI *)(void))func;

#define MSVC_DESTR(func) \
void func(void); \
static void _##func##_reg(void) { atexit(func); }; \
MSVC_CONSTR(_##func##_reg)

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_H */
