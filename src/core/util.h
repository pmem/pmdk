/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2023, Intel Corporation */
/*
 * Copyright (c) 2016-2020, Microsoft Corporation. All rights reserved.
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

#ifndef PMDK_UTIL_H
#define PMDK_UTIL_H 1

#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/param.h>

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned long long Pagesize;
extern unsigned long long Mmap_align;

#if defined(__x86_64) || defined(_M_X64) || defined(__aarch64__) || \
	defined(__riscv) || defined(__loongarch64)
#define CACHELINE_SIZE 64ULL
#elif defined(__PPC64__)
#define CACHELINE_SIZE 128ULL
#else
#error unable to recognize architecture at compile time
#endif

#define PAGE_ALIGNED_DOWN_SIZE(size) ((size) & ~(Pagesize - 1))
#define PAGE_ALIGNED_UP_SIZE(size)\
	PAGE_ALIGNED_DOWN_SIZE((size) + (Pagesize - 1))
#define IS_PAGE_ALIGNED(size) (((size) & (Pagesize - 1)) == 0)
#define IS_MMAP_ALIGNED(size) (((size) & (Mmap_align - 1)) == 0)
#define PAGE_ALIGN_UP(addr) ((void *)PAGE_ALIGNED_UP_SIZE((uintptr_t)(addr)))

#define ALIGN_UP(size, align) (((size) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(size, align) ((size) & ~((align) - 1))

#define ADDR_SUM(vp, lp) ((void *)((char *)(vp) + (lp)))

#define util_alignof(t) offsetof(struct {char _util_c; t _util_m; }, _util_m)
#define FORMAT_PRINTF(a, b) __attribute__((__format__(__printf__, (a), (b))))

void util_init(void);
int util_is_zeroed(const void *addr, size_t len);
uint64_t util_checksum_compute(void *addr, size_t len, uint64_t *csump,
		size_t skip_off);
int util_checksum(void *addr, size_t len, uint64_t *csump,
		int insert, size_t skip_off);
uint64_t util_checksum_seq(const void *addr, size_t len, uint64_t csum);
int util_parse_size(const char *str, size_t *sizep);
char *util_fgets(char *buffer, int max, FILE *stream);
char *util_getexecname(char *path, size_t pathlen);
char *util_part_realpath(const char *path);
int util_compare_file_inodes(const char *path1, const char *path2);
int util_tmpfile(const char *dir, const char *templ, int flags);
void *util_aligned_malloc(size_t alignment, size_t size);
void util_aligned_free(void *ptr);
struct tm *util_localtime(const time_t *timep, struct tm *tm);
int util_safe_strcpy(char *dst, const char *src, size_t max_length);
void util_emit_log(const char *lib, const char *func, int order);
char *util_readline(FILE *fh);
int util_snprintf(char *str, size_t size,
	const char *format, ...) FORMAT_PRINTF(3, 4);

#define UTIL_MAX_ERR_MSG 128
void util_strerror(int errnum, char *buff, size_t bufflen);
void util_strwinerror(unsigned long err, char *buff, size_t bufflen);

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

#define force_inline __attribute__((always_inline)) inline
#define NORETURN __attribute__((noreturn))

/*
 * compiler_barrier -- issues a compiler barrier
 */
static force_inline void
compiler_barrier(void)
{
	asm volatile("" ::: "memory");
}

typedef uint64_t ua_uint64_t __attribute__((aligned(1)));
typedef uint32_t ua_uint32_t __attribute__((aligned(1)));
typedef uint16_t ua_uint16_t __attribute__((aligned(1)));

#define util_get_not_masked_bits(x, mask) ((x) & ~(mask))

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
 * util_is_pow2 -- returns !0 when there's only 1 bit set in v, 0 otherwise
 */
static force_inline int
util_is_pow2(uint64_t v)
{
	return v && !(v & (v - 1));
}

/*
 * util_div_ceil -- divides a by b and rounds up the result
 */
static force_inline unsigned
util_div_ceil(unsigned a, unsigned b)
{
	return (unsigned)(((unsigned long)a + b - 1) / b);
}

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

/*
 * ISO C11 -- 7.17.1.4
 * memory_order - an enumerated type whose enumerators identify memory ordering
 * constraints.
 */
typedef enum {
	memory_order_relaxed = __ATOMIC_RELAXED,
	memory_order_consume = __ATOMIC_CONSUME,
	memory_order_acquire = __ATOMIC_ACQUIRE,
	memory_order_release = __ATOMIC_RELEASE,
	memory_order_acq_rel = __ATOMIC_ACQ_REL,
	memory_order_seq_cst = __ATOMIC_SEQ_CST
} memory_order;

/*
 * ISO C11 -- 7.17.7.2 The atomic_load generic functions
 * Integer width specific versions as supplement for:
 *
 *
 * #include <stdatomic.h>
 * C atomic_load(volatile A *object);
 * C atomic_load_explicit(volatile A *object, memory_order order);
 *
 * The atomic_load interface doesn't return the loaded value, but instead
 * copies it to a specified address -- see comments at the MSVC version.
 *
 * Also, instead of generic functions, two versions are available:
 * for 32 bit fundamental integers, and for 64 bit ones.
 */
#define util_atomic_load_explicit32 __atomic_load
#define util_atomic_load_explicit64 __atomic_load

/*
 * ISO C11 -- 7.17.7.1 The atomic_store generic functions
 * Integer width specific versions as supplement for:
 *
 * #include <stdatomic.h>
 * void atomic_store(volatile A *object, C desired);
 * void atomic_store_explicit(volatile A *object, C desired,
 *                            memory_order order);
 */
#define util_atomic_store_explicit32 __atomic_store_n
#define util_atomic_store_explicit64 __atomic_store_n

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

/* ISO C11 -- 7.17.7 Operations on atomic types */
#define util_atomic_load32(object, dest)\
	util_atomic_load_explicit32(object, dest, memory_order_seq_cst)
#define util_atomic_load64(object, dest)\
	util_atomic_load_explicit64(object, dest, memory_order_seq_cst)

#define util_atomic_store32(object, desired)\
	util_atomic_store_explicit32(object, desired, memory_order_seq_cst)
#define util_atomic_store64(object, desired)\
	util_atomic_store_explicit64(object, desired, memory_order_seq_cst)

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
#else
#define COMPILE_ERROR_ON(cond) ((void)sizeof(char[(cond) ? -1 : 1]))
#define ASSERT_COMPILE_ERROR_ON(cond) COMPILE_ERROR_ON(cond)
#endif

#define ATTR_CONSTRUCTOR __attribute__((constructor)) static
#define ATTR_DESTRUCTOR __attribute__((destructor)) static

#define CONSTRUCTOR(fun) ATTR_CONSTRUCTOR

#ifdef __GNUC__
#define CHECK_FUNC_COMPATIBLE(func1, func2)\
	COMPILE_ERROR_ON(!__builtin_types_compatible_p(typeof(func1),\
								typeof(func2)))
#else
#define CHECK_FUNC_COMPATIBLE(func1, func2) do {} while (0)
#endif /* __GNUC__ */

/* macro for counting the number of varargs (up to 9) */
#define COUNT(...)\
	COUNT_I(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#define COUNT_I(_, _9, _8, _7, _6, _5, _4, _3, _2,  X, ...) X

/* concatenation macro */
#define GLUE(A, B) GLUE_I(A, B)
#define GLUE_I(A, B) A##B

/* macro for suppresing errors from unused variables (up to 9) */
#define SUPPRESS_UNUSED(...)\
	GLUE(SUPPRESS_ARG_, COUNT(__VA_ARGS__))(__VA_ARGS__)
#define SUPPRESS_ARG_1(X) (void) X
#define SUPPRESS_ARG_2(X, ...) SUPPRESS_ARG_1(X); SUPPRESS_ARG_1(__VA_ARGS__)
#define SUPPRESS_ARG_3(X, ...) SUPPRESS_ARG_1(X); SUPPRESS_ARG_2(__VA_ARGS__)
#define SUPPRESS_ARG_4(X, ...) SUPPRESS_ARG_1(X); SUPPRESS_ARG_3(__VA_ARGS__)
#define SUPPRESS_ARG_5(X, ...) SUPPRESS_ARG_1(X); SUPPRESS_ARG_4(__VA_ARGS__)
#define SUPPRESS_ARG_6(X, ...) SUPPRESS_ARG_1(X); SUPPRESS_ARG_5(__VA_ARGS__)
#define SUPPRESS_ARG_7(X, ...) SUPPRESS_ARG_1(X); SUPPRESS_ARG_6(__VA_ARGS__)
#define SUPPRESS_ARG_8(X, ...) SUPPRESS_ARG_1(X); SUPPRESS_ARG_7(__VA_ARGS__)
#define SUPPRESS_ARG_9(X, ...) SUPPRESS_ARG_1(X); SUPPRESS_ARG_8(__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* util.h */
