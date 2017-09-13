/*
 * Copyright 2014-2017, Intel Corporation
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
 * util.h -- internal definitions for util module
 */

#ifndef NVML_UTIL_H
#define NVML_UTIL_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>

#ifdef _MSC_VER
#include <intrin.h> /* popcnt, bitscan */
#endif

#include <sys/param.h>

extern unsigned long long Pagesize;
extern unsigned long long Mmap_align;

#define PAGE_ALIGNED_DOWN_SIZE(size) ((size) & ~(Pagesize - 1))
#define PAGE_ALIGNED_UP_SIZE(size)\
	PAGE_ALIGNED_DOWN_SIZE((size) + (Pagesize - 1))
#define IS_PAGE_ALIGNED(size) (((size) & (Pagesize - 1)) == 0)
#define PAGE_ALIGN_UP(addr) ((void *)PAGE_ALIGNED_UP_SIZE((uintptr_t)(addr)))

#define MMAP_ALIGN_UP(size) (((size) + Mmap_align - 1) & ~(Mmap_align - 1))
#define MMAP_ALIGN_DOWN(size) ((size) & ~(Mmap_align - 1))

#define ADDR_SUM(vp, lp) ((void *)((char *)(vp) + lp))

#define util_alignof(t) offsetof(struct {char _util_c; t _util_m; }, _util_m)
#define FORMAT_PRINTF(a, b) __attribute__((__format__(__printf__, (a), (b))))

/*
 * overridable names for malloc & friends used by this library
 */
typedef void *(*Malloc_func)(size_t size);
typedef void (*Free_func)(void *ptr);
typedef void *(*Realloc_func)(void *ptr, size_t size);
typedef char *(*Strdup_func)(const char *s);

extern Malloc_func Malloc;
extern Free_func Free;
extern Realloc_func Realloc;
extern Strdup_func Strdup;
extern void *Zalloc(size_t sz);

void util_init(void);
int util_is_zeroed(const void *addr, size_t len);
int util_checksum(void *addr, size_t len, uint64_t *csump, int insert);
int util_parse_size(const char *str, size_t *sizep);
char *util_fgets(char *buffer, int max, FILE *stream);
char *util_getexecname(char *path, size_t pathlen);
char *util_part_realpath(const char *path);
int util_compare_file_inodes(const char *path1, const char *path2);
void *util_aligned_malloc(size_t alignment, size_t size);
void util_aligned_free(void *ptr);
struct tm *util_localtime(const time_t *timep);

#ifdef _WIN32
char *util_toUTF8(const wchar_t *wstr);
wchar_t *util_toUTF16(const char *wstr);
void util_free_UTF8(char *str);
void util_free_UTF16(wchar_t *str);
int util_toUTF16_buff(const char *in, wchar_t *out, size_t out_size);
int util_toUTF8_buff(const wchar_t *in, char *out, size_t out_size);
#endif

#define UTIL_MAX_ERR_MSG 128
void util_strerror(int errnum, char *buff, size_t bufflen);

void util_set_alloc_funcs(
		void *(*malloc_func)(size_t size),
		void (*free_func)(void *ptr),
		void *(*realloc_func)(void *ptr, size_t size),
		char *(*strdup_func)(const char *s));

/*
 * Macro calculates number of elements in given table
 */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))
#endif

/*
 * util_setbit -- setbit macro substitution which properly deals with types
 */
static inline void
util_setbit(uint8_t *b, uint32_t i)
{
	b[i / 8] = (uint8_t)(b[i / 8] | (uint8_t)(1 << (i % 8)));
}

/*
 * util_clrbit -- clrbit macro substitution which properly deals with types
 */
static inline void
util_clrbit(uint8_t *b, uint32_t i)
{
	b[i / 8] = (uint8_t)(b[i / 8] & (uint8_t)(~(1 << (i % 8))));
}

#define util_isset(a, i) isset(a, i)
#define util_isclr(a, i) isclr(a, i)

#define util_flag_isset(a, f) ((a) & (f))
#define util_flag_isclr(a, f) (((a) & (f)) == 0)

/*
 * util_bool_compare_and_swap -- perform an atomic compare and swap
 * util_fetch_and_* -- perform an operation atomically, return old value
 * util_synchronize -- issue a full memory barrier
 * util_popcount -- count number of set bits
 * util_lssb_index -- return index of least significant set bit,
 *			undefined on zero
 * util_mssb_index -- return index of most significant set bit
 *			undefined on zero
 *
 * XXX assertions needed on (value != 0) in both versions of bitscans
 *
 */

#ifndef _MSC_VER
/*
 * https://gcc.gnu.org/onlinedocs/gcc/_005f_005fsync-Builtins.html
 * https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
 * https://clang.llvm.org/docs/LanguageExtensions.html#builtin-functions
 */
#define util_bool_compare_and_swap32 __sync_bool_compare_and_swap
#define util_bool_compare_and_swap64 __sync_bool_compare_and_swap
#define util_fetch_and_add32 __sync_fetch_and_add
#define util_fetch_and_add64 __sync_fetch_and_add
#define util_fetch_and_sub32 __sync_fetch_and_sub
#define util_fetch_and_sub64 __sync_fetch_and_sub
#define util_fetch_and_and32 __sync_fetch_and_and
#define util_fetch_and_and64 __sync_fetch_and_and
#define util_fetch_and_or32 __sync_fetch_and_or
#define util_fetch_and_or64 __sync_fetch_and_or
#define util_synchronize __sync_synchronize
#define util_popcount(value) ((unsigned char)__builtin_popcount(value))
#define util_popcount64(value) ((unsigned char)__builtin_popcountll(value))
#define util_lssb_index(value) ((unsigned char)__builtin_ctz(value))
#define util_lssb_index64(value) ((unsigned char)__builtin_ctzll(value))
#define util_mssb_index(value) ((unsigned char)(31 - __builtin_clz(value)))
#define util_mssb_index64(value) ((unsigned char)(63 - __builtin_clzll(value)))

#else

/*
 * https://msdn.microsoft.com/en-us/library/hh977022.aspx
 */

static __inline int
bool_compare_and_swap32_VC(volatile LONG *ptr,
		LONG oldval, LONG newval)
{
	LONG old = InterlockedCompareExchange(ptr, newval, oldval);
	return (old == oldval);
}

static __inline int
bool_compare_and_swap64_VC(volatile LONG64 *ptr,
		LONG64 oldval, LONG64 newval)
{
	LONG64 old = InterlockedCompareExchange64(ptr, newval, oldval);
	return (old == oldval);
}

#define util_bool_compare_and_swap32(p, o, n)\
	bool_compare_and_swap32_VC((LONG *)(p), (LONG)(o), (LONG)(n))
#define util_bool_compare_and_swap64(p, o, n)\
	bool_compare_and_swap64_VC((LONG64 *)(p), (LONG64)(o), (LONG64)(n))
#define util_fetch_and_add32(ptr, value)\
    InterlockedExchangeAdd((LONG *)(ptr), value)
#define util_fetch_and_add64(ptr, value)\
    InterlockedExchangeAdd64((LONG64 *)(ptr), value)
#define util_fetch_and_sub32(ptr, value)\
    InterlockedExchangeSubtract((LONG *)(ptr), value)
#define util_fetch_and_sub64(ptr, value)\
    InterlockedExchangeAdd64((LONG64 *)(ptr), -((LONG64)(value)))
#define util_fetch_and_and32(ptr, value)\
    InterlockedAnd((LONG *)(ptr), value)
#define util_fetch_and_and64(ptr, value)\
    InterlockedAnd64((LONG64 *)(ptr), value)
#define util_fetch_and_or32(ptr, value)\
    InterlockedOr((LONG *)(ptr), value)
#define util_fetch_and_or64(ptr, value)\
    InterlockedOr64((LONG64 *)(ptr), value)

static __inline void
util_synchronize(void)
{
	MemoryBarrier();
}

#define util_popcount(value) (unsigned char)__popcnt(value)
#define util_popcount64(value) (unsigned char)__popcnt64(value)

static __inline unsigned char
util_lssb_index(int value)
{
	unsigned long ret;
	_BitScanForward(&ret, value);
	return (unsigned char)ret;
}

static __inline unsigned char
util_lssb_index64(long long value)
{
	unsigned long ret;
	_BitScanForward64(&ret, value);
	return (unsigned char)ret;
}

static __inline unsigned char
util_mssb_index(int value)
{
	unsigned long ret;
	_BitScanReverse(&ret, value);
	return (unsigned char)ret;
}

static __inline unsigned char
util_mssb_index64(long long value)
{
	unsigned long ret;
	_BitScanReverse64(&ret, value);
	return (unsigned char)ret;
}

#endif

/*
 * util_get_printable_ascii -- convert non-printable ascii to dot '.'
 */
static inline char
util_get_printable_ascii(char c)
{
	return isprint((unsigned char)c) ? c : '.';
}

char *util_concat_str(const char *s1, const char *s2);

#if !defined(likely)
#if defined(__GNUC__)
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (!!(x))
#define unlikely(x) (!!(x))
#endif
#endif

#if defined(__CHECKER__)
#define COMPILE_ERROR_ON(cond)
#define ASSERT_COMPILE_ERROR_ON(cond)
#elif defined(_MSC_VER)
#define COMPILE_ERROR_ON(cond) C_ASSERT(!(cond))
/* XXX - can't be done with C_ASSERT() unless we have __builtin_constant_p() */
#define ASSERT_COMPILE_ERROR_ON(cond)
#else
#define COMPILE_ERROR_ON(cond) ((void)sizeof(char[(cond) ? -1 : 1]))
#define ASSERT_COMPILE_ERROR_ON(cond) COMPILE_ERROR_ON(cond)
#endif

#ifndef _MSC_VER
#define ATTR_CONSTRUCTOR __attribute__((constructor)) static
#define ATTR_DESTRUCTOR __attribute__((destructor)) static
#else
#define ATTR_CONSTRUCTOR
#define ATTR_DESTRUCTOR
#endif

#ifndef _MSC_VER
#define CONSTRUCTOR(fun) ATTR_CONSTRUCTOR
#else
#ifdef __cplusplus
#define CONSTRUCTOR(fun)		\
void fun();				\
struct _##fun {			\
	_##fun() {			\
		fun();			\
	}				\
}; static  _##fun foo;			\
static
#else
#define CONSTRUCTOR(fun) \
	MSVC_CONSTR(fun) \
	static
#endif
#endif

#ifdef __GNUC__
#define CHECK_FUNC_COMPATIBLE(func1, func2)\
	COMPILE_ERROR_ON(!__builtin_types_compatible_p(typeof(func1),\
								typeof(func2)))
#else
#define CHECK_FUNC_COMPATIBLE(func1, func2) do {} while (0)
#endif /* __GNUC__ */

#ifdef __cplusplus
}
#endif

#endif /* util.h */
