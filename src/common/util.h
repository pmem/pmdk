/*
 * Copyright 2014-2016, Intel Corporation
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

extern unsigned long long Pagesize;
extern unsigned long long Mmap_align;

#define PAGE_ALIGNED_DOWN_SIZE(size) ((size) & ~(Pagesize - 1))
#define PAGE_ALIGNED_UP_SIZE(size)\
	PAGE_ALIGNED_DOWN_SIZE((size) + (Pagesize - 1))
#define IS_PAGE_ALIGNED(size) (((size) & (Pagesize - 1)) == 0)
#define PAGE_ALIGN_UP(addr) ((void *)PAGE_ALIGNED_UP_SIZE((uintptr_t)(addr)))

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
 * util_compare_and_swap -- perform an atomic compare and swap
 */
#ifndef _MSC_VER
#define util_bool_compare_and_swap32 __sync_bool_compare_and_swap
#define util_bool_compare_and_swap64 __sync_bool_compare_and_swap
#else
static __inline int
__sync_bool_compare_and_swap32(volatile LONG *ptr,
		LONG oldval, LONG newval)
{
	LONG old = InterlockedCompareExchange(ptr, newval, oldval);
	return (old == oldval);
}

static __inline int
__sync_bool_compare_and_swap64(volatile LONG64 *ptr,
		LONG64 oldval, LONG64 newval)
{
	LONG64 old = InterlockedCompareExchange64(ptr, newval, oldval);
	return (old == oldval);
}

#define util_bool_compare_and_swap32(p, o, n)\
	__sync_bool_compare_and_swap32((LONG *)(p), (LONG)(o), (LONG)(n))
#define util_bool_compare_and_swap64(p, o, n)\
	__sync_bool_compare_and_swap64((LONG64 *)(p), (LONG64)(o), (LONG64)(n))
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

#ifndef _WIN32
#define DIR_SEPARATOR '/'
#else
#define DIR_SEPARATOR '\\'
#endif

#ifndef _MSC_VER
#define COMPILE_ERROR_ON(cond) ((void)sizeof(char[(cond) ? -1 : 1]))
#define ASSERT_COMPILE_ERROR_ON(cond) COMPILE_ERROR_ON(cond)
#else
#define COMPILE_ERROR_ON(cond) C_ASSERT(!(cond))
/* XXX - can't be done with C_ASSERT() unless we have __builtin_constant_p() */
#define ASSERT_COMPILE_ERROR_ON(cond)
#endif

#ifndef _MSC_VER
#define ATTR_CONSTRUCTOR __attribute__((constructor)) static
#define ATTR_DESTRUCTOR __attribute__((destructor)) static
#else
#define ATTR_CONSTRUCTOR
#define ATTR_DESTRUCTOR
#endif

#ifdef __GNUC__
#define CHECK_FUNC_COMPATIBLE(func1, func2)\
	COMPILE_ERROR_ON(!__builtin_types_compatible_p(typeof(func1),\
								typeof(func2)))
#else
#define CHECK_FUNC_COMPATIBLE(func1, func2) do {} while (0)
#endif /* __GNUC__ */

#define PERSIST_GENERIC(is_pmem, addr, len) do {\
	void *raddr = addr; size_t rlen = len;\
	if (is_pmem) \
		pmem_persist(raddr, rlen);\
	else\
		pmem_msync(raddr, rlen);\
} while (0)

#define PERSIST_GENERIC_AUTO(addr, len) do {\
	void *raddr = addr; size_t rlen = len;\
	if (pmem_is_pmem(raddr, rlen)) \
		pmem_persist(raddr, rlen);\
	else\
		pmem_msync(raddr, rlen);\
} while (0)

#ifdef __cplusplus
}
#endif

#endif /* util.h */
