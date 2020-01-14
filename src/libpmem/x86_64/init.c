/*
 * Copyright 2014-2020, Intel Corporation
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

#include <string.h>
#include <xmmintrin.h>
#include "libpmem.h"

#include "cpu.h"
#include "flush.h"
#include "memcpy_memset.h"
#include "os.h"
#include "out.h"
#include "pmem.h"
#include "valgrind_internal.h"

#define MOVNT_THRESHOLD	256

size_t Movnt_threshold = MOVNT_THRESHOLD;

/*
 * predrain_fence_empty -- (internal) issue the pre-drain fence instruction
 */
static void
predrain_fence_empty(void)
{
	LOG(15, NULL);

	VALGRIND_DO_FENCE;
	/* nothing to do (because CLFLUSH did it for us) */
}

/*
 * predrain_memory_barrier -- (internal) issue the pre-drain fence instruction
 */
static void
predrain_memory_barrier(void)
{
	LOG(15, NULL);
	_mm_sfence();	/* ensure CLWB or CLFLUSHOPT completes */
}

/*
 * flush_clflush -- (internal) flush the CPU cache, using clflush
 */
static void
flush_clflush(const void *addr, size_t len)
{
	LOG(15, "addr %p len %zu", addr, len);

	flush_clflush_nolog(addr, len);
}

/*
 * flush_clflushopt -- (internal) flush the CPU cache, using clflushopt
 */
static void
flush_clflushopt(const void *addr, size_t len)
{
	LOG(15, "addr %p len %zu", addr, len);

	flush_clflushopt_nolog(addr, len);
}

/*
 * flush_clwb -- (internal) flush the CPU cache, using clwb
 */
static void
flush_clwb(const void *addr, size_t len)
{
	LOG(15, "addr %p len %zu", addr, len);

	flush_clwb_nolog(addr, len);
}

/*
 * flush_empty -- (internal) do not flush the CPU cache
 */
static void
flush_empty(const void *addr, size_t len)
{
	LOG(15, "addr %p len %zu", addr, len);

	flush_empty_nolog(addr, len);
}

#if SSE2_AVAILABLE || AVX_AVAILABLE || AVX512F_AVAILABLE
#define PMEM_F_MEM_MOVNT (PMEM_F_MEM_WC | PMEM_F_MEM_NONTEMPORAL)
#define PMEM_F_MEM_MOV   (PMEM_F_MEM_WB | PMEM_F_MEM_TEMPORAL)

#define MEMCPY_TEMPLATE(isa, flush) \
static void *\
memmove_nodrain_##isa##_##flush(void *dest, const void *src, size_t len, \
		unsigned flags)\
{\
	if (len == 0 || src == dest)\
		return dest;\
\
	if (flags & PMEM_F_MEM_NOFLUSH) \
		memmove_mov_##isa##_noflush(dest, src, len); \
	else if (flags & PMEM_F_MEM_MOVNT)\
		memmove_movnt_##isa ##_##flush(dest, src, len);\
	else if (flags & PMEM_F_MEM_MOV)\
		memmove_mov_##isa##_##flush(dest, src, len);\
	else if (len < Movnt_threshold)\
		memmove_mov_##isa##_##flush(dest, src, len);\
	else\
		memmove_movnt_##isa##_##flush(dest, src, len);\
\
	return dest;\
}

#define MEMSET_TEMPLATE(isa, flush)\
static void *\
memset_nodrain_##isa##_##flush(void *dest, int c, size_t len, unsigned flags)\
{\
	if (len == 0)\
		return dest;\
\
	if (flags & PMEM_F_MEM_NOFLUSH) \
		memset_mov_##isa##_noflush(dest, c, len); \
	else if (flags & PMEM_F_MEM_MOVNT)\
		memset_movnt_##isa##_##flush(dest, c, len);\
	else if (flags & PMEM_F_MEM_MOV)\
		memset_mov_##isa##_##flush(dest, c, len);\
	else if (len < Movnt_threshold)\
		memset_mov_##isa##_##flush(dest, c, len);\
	else\
		memset_movnt_##isa##_##flush(dest, c, len);\
\
	return dest;\
}
#endif

#if SSE2_AVAILABLE
MEMCPY_TEMPLATE(sse2, clflush)
MEMCPY_TEMPLATE(sse2, clflushopt)
MEMCPY_TEMPLATE(sse2, clwb)
MEMCPY_TEMPLATE(sse2, empty)

MEMSET_TEMPLATE(sse2, clflush)
MEMSET_TEMPLATE(sse2, clflushopt)
MEMSET_TEMPLATE(sse2, clwb)
MEMSET_TEMPLATE(sse2, empty)
#endif

#if AVX_AVAILABLE
MEMCPY_TEMPLATE(avx, clflush)
MEMCPY_TEMPLATE(avx, clflushopt)
MEMCPY_TEMPLATE(avx, clwb)
MEMCPY_TEMPLATE(avx, empty)

MEMSET_TEMPLATE(avx, clflush)
MEMSET_TEMPLATE(avx, clflushopt)
MEMSET_TEMPLATE(avx, clwb)
MEMSET_TEMPLATE(avx, empty)
#endif

#if AVX512F_AVAILABLE
MEMCPY_TEMPLATE(avx512f, clflush)
MEMCPY_TEMPLATE(avx512f, clflushopt)
MEMCPY_TEMPLATE(avx512f, clwb)
MEMCPY_TEMPLATE(avx512f, empty)

MEMSET_TEMPLATE(avx512f, clflush)
MEMSET_TEMPLATE(avx512f, clflushopt)
MEMSET_TEMPLATE(avx512f, clwb)
MEMSET_TEMPLATE(avx512f, empty)
#endif

/*
 * memmove_nodrain_libc -- (internal) memmove to pmem using libc
 */
static void *
memmove_nodrain_libc(void *pmemdest, const void *src, size_t len,
		unsigned flags)
{
	LOG(15, "pmemdest %p src %p len %zu flags 0x%x", pmemdest, src, len,
			flags);
	(void) flags;

	memmove(pmemdest, src, len);
	pmem_flush_flags(pmemdest, len, flags);
	return pmemdest;
}

/*
 * memset_nodrain_libc -- (internal) memset to pmem using libc
 */
static void *
memset_nodrain_libc(void *pmemdest, int c, size_t len, unsigned flags)
{
	LOG(15, "pmemdest %p c 0x%x len %zu flags 0x%x", pmemdest, c, len,
			flags);
	(void) flags;

	memset(pmemdest, c, len);
	pmem_flush_flags(pmemdest, len, flags);
	return pmemdest;
}

enum memcpy_impl {
	MEMCPY_INVALID,
	MEMCPY_LIBC,
	MEMCPY_GENERIC,
	MEMCPY_SSE2,
	MEMCPY_AVX,
	MEMCPY_AVX512F
};

/*
 * use_sse2_memcpy_memset -- (internal) SSE2 detected, use it if possible
 */
static void
use_sse2_memcpy_memset(struct pmem_funcs *funcs, enum memcpy_impl *impl)
{
#if SSE2_AVAILABLE
	*impl = MEMCPY_SSE2;
	if (funcs->deep_flush == flush_clflush)
		funcs->memmove_nodrain = memmove_nodrain_sse2_clflush;
	else if (funcs->deep_flush == flush_clflushopt)
		funcs->memmove_nodrain = memmove_nodrain_sse2_clflushopt;
	else if (funcs->deep_flush == flush_clwb)
		funcs->memmove_nodrain = memmove_nodrain_sse2_clwb;
	else if (funcs->deep_flush == flush_empty)
		funcs->memmove_nodrain = memmove_nodrain_sse2_empty;
	else
		ASSERT(0);

	if (funcs->deep_flush == flush_clflush)
		funcs->memset_nodrain = memset_nodrain_sse2_clflush;
	else if (funcs->deep_flush == flush_clflushopt)
		funcs->memset_nodrain = memset_nodrain_sse2_clflushopt;
	else if (funcs->deep_flush == flush_clwb)
		funcs->memset_nodrain = memset_nodrain_sse2_clwb;
	else if (funcs->deep_flush == flush_empty)
		funcs->memset_nodrain = memset_nodrain_sse2_empty;
	else
		ASSERT(0);
#else
	LOG(3, "sse2 disabled at build time");
#endif

}

/*
 * use_avx_memcpy_memset -- (internal) AVX detected, use it if possible
 */
static void
use_avx_memcpy_memset(struct pmem_funcs *funcs, enum memcpy_impl *impl)
{
#if AVX_AVAILABLE
	LOG(3, "avx supported");

	char *e = os_getenv("PMEM_AVX");
	if (e == NULL || strcmp(e, "1") != 0) {
		LOG(3, "PMEM_AVX not set or not == 1");
		return;
	}

	LOG(3, "PMEM_AVX enabled");
	*impl = MEMCPY_AVX;

	if (funcs->deep_flush == flush_clflush)
		funcs->memmove_nodrain = memmove_nodrain_avx_clflush;
	else if (funcs->deep_flush == flush_clflushopt)
		funcs->memmove_nodrain = memmove_nodrain_avx_clflushopt;
	else if (funcs->deep_flush == flush_clwb)
		funcs->memmove_nodrain = memmove_nodrain_avx_clwb;
	else if (funcs->deep_flush == flush_empty)
		funcs->memmove_nodrain = memmove_nodrain_avx_empty;
	else
		ASSERT(0);

	if (funcs->deep_flush == flush_clflush)
		funcs->memset_nodrain = memset_nodrain_avx_clflush;
	else if (funcs->deep_flush == flush_clflushopt)
		funcs->memset_nodrain = memset_nodrain_avx_clflushopt;
	else if (funcs->deep_flush == flush_clwb)
		funcs->memset_nodrain = memset_nodrain_avx_clwb;
	else if (funcs->deep_flush == flush_empty)
		funcs->memset_nodrain = memset_nodrain_avx_empty;
	else
		ASSERT(0);
#else
	LOG(3, "avx supported, but disabled at build time");
#endif
}

/*
 * use_avx512f_memcpy_memset -- (internal) AVX512F detected, use it if possible
 */
static void
use_avx512f_memcpy_memset(struct pmem_funcs *funcs, enum memcpy_impl *impl)
{
#if AVX512F_AVAILABLE
	LOG(3, "avx512f supported");

	char *e = os_getenv("PMEM_AVX512F");
	if (e == NULL || strcmp(e, "1") != 0) {
		LOG(3, "PMEM_AVX512F not set or not == 1");
		return;
	}

	LOG(3, "PMEM_AVX512F enabled");
	*impl = MEMCPY_AVX512F;

	if (funcs->deep_flush == flush_clflush)
		funcs->memmove_nodrain = memmove_nodrain_avx512f_clflush;
	else if (funcs->deep_flush == flush_clflushopt)
		funcs->memmove_nodrain = memmove_nodrain_avx512f_clflushopt;
	else if (funcs->deep_flush == flush_clwb)
		funcs->memmove_nodrain = memmove_nodrain_avx512f_clwb;
	else if (funcs->deep_flush == flush_empty)
		funcs->memmove_nodrain = memmove_nodrain_avx512f_empty;
	else
		ASSERT(0);

	if (funcs->deep_flush == flush_clflush)
		funcs->memset_nodrain = memset_nodrain_avx512f_clflush;
	else if (funcs->deep_flush == flush_clflushopt)
		funcs->memset_nodrain = memset_nodrain_avx512f_clflushopt;
	else if (funcs->deep_flush == flush_clwb)
		funcs->memset_nodrain = memset_nodrain_avx512f_clwb;
	else if (funcs->deep_flush == flush_empty)
		funcs->memset_nodrain = memset_nodrain_avx512f_empty;
	else
		ASSERT(0);
#else
	LOG(3, "avx512f supported, but disabled at build time");
#endif
}

/*
 * pmem_get_cpuinfo -- configure libpmem based on CPUID
 */
static void
pmem_cpuinfo_to_funcs(struct pmem_funcs *funcs, enum memcpy_impl *impl)
{
	LOG(3, NULL);

	if (is_cpu_clflush_present()) {
		funcs->is_pmem = is_pmem_detect;
		LOG(3, "clflush supported");
	}

	if (is_cpu_clflushopt_present()) {
		LOG(3, "clflushopt supported");

		char *e = os_getenv("PMEM_NO_CLFLUSHOPT");
		if (e && strcmp(e, "1") == 0) {
			LOG(3, "PMEM_NO_CLFLUSHOPT forced no clflushopt");
		} else {
			funcs->deep_flush = flush_clflushopt;
			funcs->predrain_fence = predrain_memory_barrier;
		}
	}

	if (is_cpu_clwb_present()) {
		LOG(3, "clwb supported");

		char *e = os_getenv("PMEM_NO_CLWB");
		if (e && strcmp(e, "1") == 0) {
			LOG(3, "PMEM_NO_CLWB forced no clwb");
		} else {
			funcs->deep_flush = flush_clwb;
			funcs->predrain_fence = predrain_memory_barrier;
		}
	}

	char *ptr = os_getenv("PMEM_NO_MOVNT");
	if (ptr && strcmp(ptr, "1") == 0) {
		LOG(3, "PMEM_NO_MOVNT forced no movnt");
	} else {
		use_sse2_memcpy_memset(funcs, impl);

		if (is_cpu_avx_present())
			use_avx_memcpy_memset(funcs, impl);

		if (is_cpu_avx512f_present())
			use_avx512f_memcpy_memset(funcs, impl);
	}
}

/*
 * pmem_init_funcs -- initialize architecture-specific list of pmem operations
 */
void
pmem_init_funcs(struct pmem_funcs *funcs)
{
	LOG(3, NULL);

	funcs->predrain_fence = predrain_fence_empty;
	funcs->deep_flush = flush_clflush;
	funcs->is_pmem = NULL;
	funcs->memmove_nodrain = memmove_nodrain_generic;
	funcs->memset_nodrain = memset_nodrain_generic;
	enum memcpy_impl impl = MEMCPY_GENERIC;

	char *ptr = os_getenv("PMEM_NO_GENERIC_MEMCPY");
	if (ptr) {
		long long val = atoll(ptr);

		if (val) {
			funcs->memmove_nodrain = memmove_nodrain_libc;
			funcs->memset_nodrain = memset_nodrain_libc;
			impl = MEMCPY_LIBC;
		}
	}

	pmem_cpuinfo_to_funcs(funcs, &impl);

	/*
	 * For testing, allow overriding the default threshold
	 * for using non-temporal stores in pmem_memcpy_*(), pmem_memmove_*()
	 * and pmem_memset_*().
	 * It has no effect if movnt is not supported or disabled.
	 */
	ptr = os_getenv("PMEM_MOVNT_THRESHOLD");
	if (ptr) {
		long long val = atoll(ptr);

		if (val < 0) {
			LOG(3, "Invalid PMEM_MOVNT_THRESHOLD");
		} else {
			LOG(3, "PMEM_MOVNT_THRESHOLD set to %zu", (size_t)val);
			Movnt_threshold = (size_t)val;
		}
	}

	int flush;
	char *e = os_getenv("PMEM_NO_FLUSH");
	if (e && (strcmp(e, "1") == 0)) {
		flush = 0;
		LOG(3, "Forced not flushing CPU_cache");
	} else if (e && (strcmp(e, "0") == 0)) {
		flush = 1;
		LOG(3, "Forced flushing CPU_cache");
	} else if (pmem_has_auto_flush() == 1) {
		flush = 0;
		LOG(3, "Not flushing CPU_cache, eADR detected");
	} else {
		flush = 1;
		LOG(3, "Flushing CPU cache");
	}

	if (flush) {
		funcs->flush = funcs->deep_flush;
	} else {
		funcs->flush = flush_empty;
		funcs->predrain_fence = predrain_memory_barrier;
	}

	if (funcs->deep_flush == flush_clwb)
		LOG(3, "using clwb");
	else if (funcs->deep_flush == flush_clflushopt)
		LOG(3, "using clflushopt");
	else if (funcs->deep_flush == flush_clflush)
		LOG(3, "using clflush");
	else
		FATAL("invalid deep flush function address");

	if (funcs->flush == flush_empty)
		LOG(3, "not flushing CPU cache");
	else if (funcs->flush != funcs->deep_flush)
		FATAL("invalid flush function address");

	if (impl == MEMCPY_AVX512F)
		LOG(3, "using movnt AVX512F");
	else if (impl == MEMCPY_AVX)
		LOG(3, "using movnt AVX");
	else if (impl == MEMCPY_SSE2)
		LOG(3, "using movnt SSE2");
	else if (impl == MEMCPY_LIBC)
		LOG(3, "using libc memmove");
	else if (impl == MEMCPY_GENERIC)
		LOG(3, "using generic memmove");
	else
		FATAL("invalid memcpy impl");
}
