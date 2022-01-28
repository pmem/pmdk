// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2022, Intel Corporation */

#include <string.h>
#include <xmmintrin.h>

#include "auto_flush.h"
#include "cpu.h"
#include "flush.h"
#include "memcpy_memset.h"
#include "os.h"
#include "out.h"
#include "pmem2_arch.h"
#include "valgrind_internal.h"

#define MOVNT_THRESHOLD	256

size_t Movnt_threshold = MOVNT_THRESHOLD;

/*
 * memory_barrier -- (internal) issue the fence instruction
 */
static void
memory_barrier(void)
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

#if SSE2_AVAILABLE || AVX_AVAILABLE || AVX512F_AVAILABLE || MOVDIR64B_AVAILABLE
#define PMEM2_F_MEM_MOVNT (PMEM2_F_MEM_WC | PMEM2_F_MEM_NONTEMPORAL)
#define PMEM2_F_MEM_MOV   (PMEM2_F_MEM_WB | PMEM2_F_MEM_TEMPORAL)

#define MEMCPY_TEMPLATE(isa, flush, perfbarrier) \
static void *\
memmove_nodrain_##isa##_##flush##perfbarrier(void *dest, const void *src, \
		size_t len, unsigned flags, flush_func flushf)\
{\
	/* suppress unused-parameter errors */\
	SUPPRESS_UNUSED(flushf);\
\
	if (len == 0 || src == dest)\
		return dest;\
\
	if (flags & PMEM2_F_MEM_NOFLUSH) \
		memmove_mov_##isa##_noflush(dest, src, len); \
	else if (flags & PMEM2_F_MEM_MOVNT)\
		memmove_movnt_##isa ##_##flush##perfbarrier(dest, src, len);\
	else if (flags & PMEM2_F_MEM_MOV)\
		memmove_mov_##isa##_##flush(dest, src, len);\
	else if (len < Movnt_threshold)\
		memmove_mov_##isa##_##flush(dest, src, len);\
	else\
		memmove_movnt_##isa##_##flush##perfbarrier(dest, src, len);\
\
	return dest;\
}

#define MEMCPY_TEMPLATE_EADR(isa, perfbarrier) \
static void *\
memmove_nodrain_##isa##_eadr##perfbarrier(void *dest, const void *src, \
		size_t len, unsigned flags, flush_func flushf)\
{\
	/* suppress unused-parameter errors */\
	SUPPRESS_UNUSED(flushf);\
\
	if (len == 0 || src == dest)\
		return dest;\
\
	if (flags & PMEM2_F_MEM_NOFLUSH)\
		memmove_mov_##isa##_noflush(dest, src, len);\
	else if (flags & PMEM2_F_MEM_NONTEMPORAL)\
		memmove_movnt_##isa##_empty##perfbarrier(dest, src, len);\
	else\
		memmove_mov_##isa##_empty(dest, src, len);\
\
	return dest;\
}

#define MEMSET_TEMPLATE(isa, flush, perfbarrier)\
static void *\
memset_nodrain_##isa##_##flush##perfbarrier(void *dest, int c, size_t len, \
		unsigned flags, flush_func flushf)\
{\
	/* suppress unused-parameter errors */\
	SUPPRESS_UNUSED(flushf);\
\
	if (len == 0)\
		return dest;\
\
	if (flags & PMEM2_F_MEM_NOFLUSH) \
		memset_mov_##isa##_noflush(dest, c, len); \
	else if (flags & PMEM2_F_MEM_MOVNT)\
		memset_movnt_##isa##_##flush##perfbarrier(dest, c, len);\
	else if (flags & PMEM2_F_MEM_MOV)\
		memset_mov_##isa##_##flush(dest, c, len);\
	else if (len < Movnt_threshold)\
		memset_mov_##isa##_##flush(dest, c, len);\
	else\
		memset_movnt_##isa##_##flush##perfbarrier(dest, c, len);\
\
	return dest;\
}

#define MEMSET_TEMPLATE_EADR(isa, perfbarrier) \
static void *\
memset_nodrain_##isa##_eadr##perfbarrier(void *dest, int c, size_t len, \
		unsigned flags, flush_func flushf)\
{\
	/* suppress unused-parameter errors */\
	SUPPRESS_UNUSED(flushf);\
\
	if (len == 0)\
		return dest;\
\
	if (flags & PMEM2_F_MEM_NOFLUSH)\
		memset_mov_##isa##_noflush(dest, c, len);\
	else if (flags & PMEM2_F_MEM_NONTEMPORAL)\
		memset_movnt_##isa##_empty##perfbarrier(dest, c, len);\
	else\
		memset_mov_##isa##_empty(dest, c, len);\
\
	return dest;\
}
#endif

#if SSE2_AVAILABLE
MEMCPY_TEMPLATE(sse2, clflush, _nobarrier)
MEMCPY_TEMPLATE(sse2, clflushopt, _nobarrier)
MEMCPY_TEMPLATE(sse2, clwb, _nobarrier)
MEMCPY_TEMPLATE_EADR(sse2, _nobarrier)

MEMSET_TEMPLATE(sse2, clflush, _nobarrier)
MEMSET_TEMPLATE(sse2, clflushopt, _nobarrier)
MEMSET_TEMPLATE(sse2, clwb, _nobarrier)
MEMSET_TEMPLATE_EADR(sse2, _nobarrier)

MEMCPY_TEMPLATE(sse2, clflush, _wcbarrier)
MEMCPY_TEMPLATE(sse2, clflushopt, _wcbarrier)
MEMCPY_TEMPLATE(sse2, clwb, _wcbarrier)
MEMCPY_TEMPLATE_EADR(sse2, _wcbarrier)

MEMSET_TEMPLATE(sse2, clflush, _wcbarrier)
MEMSET_TEMPLATE(sse2, clflushopt, _wcbarrier)
MEMSET_TEMPLATE(sse2, clwb, _wcbarrier)
MEMSET_TEMPLATE_EADR(sse2, _wcbarrier)
#endif

#if AVX_AVAILABLE
MEMCPY_TEMPLATE(avx, clflush, _nobarrier)
MEMCPY_TEMPLATE(avx, clflushopt, _nobarrier)
MEMCPY_TEMPLATE(avx, clwb, _nobarrier)
MEMCPY_TEMPLATE_EADR(avx, _nobarrier)

MEMSET_TEMPLATE(avx, clflush, _nobarrier)
MEMSET_TEMPLATE(avx, clflushopt, _nobarrier)
MEMSET_TEMPLATE(avx, clwb, _nobarrier)
MEMSET_TEMPLATE_EADR(avx, _nobarrier)

MEMCPY_TEMPLATE(avx, clflush, _wcbarrier)
MEMCPY_TEMPLATE(avx, clflushopt, _wcbarrier)
MEMCPY_TEMPLATE(avx, clwb, _wcbarrier)
MEMCPY_TEMPLATE_EADR(avx, _wcbarrier)

MEMSET_TEMPLATE(avx, clflush, _wcbarrier)
MEMSET_TEMPLATE(avx, clflushopt, _wcbarrier)
MEMSET_TEMPLATE(avx, clwb, _wcbarrier)
MEMSET_TEMPLATE_EADR(avx, _wcbarrier)
#endif

#if AVX512F_AVAILABLE
MEMCPY_TEMPLATE(avx512f, clflush, /* cstyle wa */)
MEMCPY_TEMPLATE(avx512f, clflushopt, /* */)
MEMCPY_TEMPLATE(avx512f, clwb, /* */)
MEMCPY_TEMPLATE_EADR(avx512f, /* */)

MEMSET_TEMPLATE(avx512f, clflush, /* */)
MEMSET_TEMPLATE(avx512f, clflushopt, /* */)
MEMSET_TEMPLATE(avx512f, clwb, /* */)
MEMSET_TEMPLATE_EADR(avx512f, /* */)
#endif

#if MOVDIR64B_AVAILABLE
MEMCPY_TEMPLATE(movdir64b, clflush, /* cstyle wa */)
MEMCPY_TEMPLATE(movdir64b, clflushopt, /* */)
MEMCPY_TEMPLATE(movdir64b, clwb, /* */)
MEMCPY_TEMPLATE_EADR(movdir64b, /* */)

MEMSET_TEMPLATE(movdir64b, clflush, /* */)
MEMSET_TEMPLATE(movdir64b, clflushopt, /* */)
MEMSET_TEMPLATE(movdir64b, clwb, /* */)
MEMSET_TEMPLATE_EADR(movdir64b, /* */)
#endif

enum memcpy_impl {
	MEMCPY_INVALID,
	MEMCPY_SSE2,
	MEMCPY_AVX,
	MEMCPY_AVX512F,
	MEMCPY_MOVDIR64B
};

/*
 * use_sse2_memcpy_memset -- (internal) SSE2 detected, use it if possible
 */
static void
use_sse2_memcpy_memset(struct pmem2_arch_info *info, enum memcpy_impl *impl,
		int wc_workaround)
{
#if SSE2_AVAILABLE
	*impl = MEMCPY_SSE2;
	if (wc_workaround) {
		info->memmove_nodrain_eadr =
				memmove_nodrain_sse2_eadr_wcbarrier;
		if (info->flush == flush_clflush)
			info->memmove_nodrain =
				memmove_nodrain_sse2_clflush_wcbarrier;
		else if (info->flush == flush_clflushopt)
			info->memmove_nodrain =
				memmove_nodrain_sse2_clflushopt_wcbarrier;
		else if (info->flush == flush_clwb)
			info->memmove_nodrain =
				memmove_nodrain_sse2_clwb_wcbarrier;
		else
			ASSERT(0);

		info->memset_nodrain_eadr = memset_nodrain_sse2_eadr_wcbarrier;
		if (info->flush == flush_clflush)
			info->memset_nodrain =
				memset_nodrain_sse2_clflush_wcbarrier;
		else if (info->flush == flush_clflushopt)
			info->memset_nodrain =
				memset_nodrain_sse2_clflushopt_wcbarrier;
		else if (info->flush == flush_clwb)
			info->memset_nodrain =
				memset_nodrain_sse2_clwb_wcbarrier;
		else
			ASSERT(0);
	} else {
		info->memmove_nodrain_eadr =
				memmove_nodrain_sse2_eadr_nobarrier;
		if (info->flush == flush_clflush)
			info->memmove_nodrain =
				memmove_nodrain_sse2_clflush_nobarrier;
		else if (info->flush == flush_clflushopt)
			info->memmove_nodrain =
				memmove_nodrain_sse2_clflushopt_nobarrier;
		else if (info->flush == flush_clwb)
			info->memmove_nodrain =
				memmove_nodrain_sse2_clwb_nobarrier;
		else
			ASSERT(0);

		info->memset_nodrain_eadr =
				memset_nodrain_sse2_eadr_nobarrier;
		if (info->flush == flush_clflush)
			info->memset_nodrain =
				memset_nodrain_sse2_clflush_nobarrier;
		else if (info->flush == flush_clflushopt)
			info->memset_nodrain =
				memset_nodrain_sse2_clflushopt_nobarrier;
		else if (info->flush == flush_clwb)
			info->memset_nodrain =
				memset_nodrain_sse2_clwb_nobarrier;
		else
			ASSERT(0);
	}

#else
	SUPPRESS_UNUSED(info, impl);
	LOG(3, "sse2 disabled at build time");
#endif

}

/*
 * use_avx_memcpy_memset -- (internal) AVX detected, use it if possible
 */
static void
use_avx_memcpy_memset(struct pmem2_arch_info *info, enum memcpy_impl *impl,
		int wc_workaround)
{
#if AVX_AVAILABLE
	LOG(3, "avx supported");

	char *e = os_getenv("PMEM_AVX");
	if (e != NULL && strcmp(e, "0") == 0) {
		LOG(3, "PMEM_AVX set to 0");
		return;
	}

	LOG(3, "PMEM_AVX enabled");
	*impl = MEMCPY_AVX;

	if (wc_workaround) {
		info->memmove_nodrain_eadr =
				memmove_nodrain_avx_eadr_wcbarrier;
		if (info->flush == flush_clflush)
			info->memmove_nodrain =
				memmove_nodrain_avx_clflush_wcbarrier;
		else if (info->flush == flush_clflushopt)
			info->memmove_nodrain =
				memmove_nodrain_avx_clflushopt_wcbarrier;
		else if (info->flush == flush_clwb)
			info->memmove_nodrain =
				memmove_nodrain_avx_clwb_wcbarrier;
		else
			ASSERT(0);

		info->memset_nodrain_eadr =
				memset_nodrain_avx_eadr_wcbarrier;
		if (info->flush == flush_clflush)
			info->memset_nodrain =
				memset_nodrain_avx_clflush_wcbarrier;
		else if (info->flush == flush_clflushopt)
			info->memset_nodrain =
				memset_nodrain_avx_clflushopt_wcbarrier;
		else if (info->flush == flush_clwb)
			info->memset_nodrain =
				memset_nodrain_avx_clwb_wcbarrier;
		else
			ASSERT(0);
	} else {
		info->memmove_nodrain_eadr =
				memmove_nodrain_avx_eadr_nobarrier;
		if (info->flush == flush_clflush)
			info->memmove_nodrain =
				memmove_nodrain_avx_clflush_nobarrier;
		else if (info->flush == flush_clflushopt)
			info->memmove_nodrain =
				memmove_nodrain_avx_clflushopt_nobarrier;
		else if (info->flush == flush_clwb)
			info->memmove_nodrain =
				memmove_nodrain_avx_clwb_nobarrier;
		else
			ASSERT(0);

		info->memset_nodrain_eadr =
				memset_nodrain_avx_eadr_nobarrier;
		if (info->flush == flush_clflush)
			info->memset_nodrain =
				memset_nodrain_avx_clflush_nobarrier;
		else if (info->flush == flush_clflushopt)
			info->memset_nodrain =
				memset_nodrain_avx_clflushopt_nobarrier;
		else if (info->flush == flush_clwb)
			info->memset_nodrain =
				memset_nodrain_avx_clwb_nobarrier;
		else
			ASSERT(0);
	}
#else
	SUPPRESS_UNUSED(info, impl);
	LOG(3, "avx supported, but disabled at build time");
#endif
}

/*
 * use_avx512f_memcpy_memset -- (internal) AVX512F detected, use it if possible
 */
static void
use_avx512f_memcpy_memset(struct pmem2_arch_info *info,
		enum memcpy_impl *impl)
{
#if AVX512F_AVAILABLE
	LOG(3, "avx512f supported");

	char *e = os_getenv("PMEM_AVX512F");
	if (e != NULL && strcmp(e, "0") == 0) {
		LOG(3, "PMEM_AVX512F set to 0");
		return;
	}

	LOG(3, "PMEM_AVX512F enabled");
	*impl = MEMCPY_AVX512F;

	info->memmove_nodrain_eadr = memmove_nodrain_avx512f_eadr;
	if (info->flush == flush_clflush)
		info->memmove_nodrain = memmove_nodrain_avx512f_clflush;
	else if (info->flush == flush_clflushopt)
		info->memmove_nodrain = memmove_nodrain_avx512f_clflushopt;
	else if (info->flush == flush_clwb)
		info->memmove_nodrain = memmove_nodrain_avx512f_clwb;
	else
		ASSERT(0);

	info->memset_nodrain_eadr = memset_nodrain_avx512f_eadr;
	if (info->flush == flush_clflush)
		info->memset_nodrain = memset_nodrain_avx512f_clflush;
	else if (info->flush == flush_clflushopt)
		info->memset_nodrain = memset_nodrain_avx512f_clflushopt;
	else if (info->flush == flush_clwb)
		info->memset_nodrain = memset_nodrain_avx512f_clwb;
	else
		ASSERT(0);
#else
	SUPPRESS_UNUSED(info, impl);
	LOG(3, "avx512f supported, but disabled at build time");
#endif
}

/*
 * use_movdir64b_memcpy_memset -- (internal) movdir64b detected, use it if
 *                                           possible
 */
static void
use_movdir64b_memcpy_memset(struct pmem2_arch_info *info,
		enum memcpy_impl *impl)
{
#if MOVDIR64B_AVAILABLE
	LOG(3, "movdir64b supported");

	char *e = os_getenv("PMEM_MOVDIR64B");
	if (e != NULL && strcmp(e, "0") == 0) {
		LOG(3, "PMEM_MOVDIR64B set to 0");
		return;
	}

	LOG(3, "PMEM_MOVDIR64B enabled");
	*impl = MEMCPY_MOVDIR64B;

	info->memmove_nodrain_eadr = memmove_nodrain_movdir64b_eadr;
	if (info->flush == flush_clflush)
		info->memmove_nodrain = memmove_nodrain_movdir64b_clflush;
	else if (info->flush == flush_clflushopt)
		info->memmove_nodrain = memmove_nodrain_movdir64b_clflushopt;
	else if (info->flush == flush_clwb)
		info->memmove_nodrain = memmove_nodrain_movdir64b_clwb;
	else
		ASSERT(0);

	info->memset_nodrain_eadr = memset_nodrain_movdir64b_eadr;
	if (info->flush == flush_clflush)
		info->memset_nodrain = memset_nodrain_movdir64b_clflush;
	else if (info->flush == flush_clflushopt)
		info->memset_nodrain = memset_nodrain_movdir64b_clflushopt;
	else if (info->flush == flush_clwb)
		info->memset_nodrain = memset_nodrain_movdir64b_clwb;
	else
		ASSERT(0);
#else
	SUPPRESS_UNUSED(info, impl);
	LOG(3, "movdir64b supported, but disabled at build time");
#endif
}

/*
 * pmem_get_cpuinfo -- configure libpmem based on CPUID
 */
static void
pmem_cpuinfo_to_funcs(struct pmem2_arch_info *info, enum memcpy_impl *impl)
{
	LOG(3, NULL);

	if (is_cpu_clflush_present()) {
		LOG(3, "clflush supported");

		info->flush = flush_clflush;
		info->flush_has_builtin_fence = 1;
		info->fence = memory_barrier;
	}

	if (is_cpu_clflushopt_present()) {
		LOG(3, "clflushopt supported");

		char *e = os_getenv("PMEM_NO_CLFLUSHOPT");
		if (e && strcmp(e, "1") == 0) {
			LOG(3, "PMEM_NO_CLFLUSHOPT forced no clflushopt");
		} else {
			info->flush = flush_clflushopt;
			info->flush_has_builtin_fence = 0;
			info->fence = memory_barrier;
		}
	}

	if (is_cpu_clwb_present()) {
		LOG(3, "clwb supported");

		char *e = os_getenv("PMEM_NO_CLWB");
		if (e && strcmp(e, "1") == 0) {
			LOG(3, "PMEM_NO_CLWB forced no clwb");
		} else {
			info->flush = flush_clwb;
			info->flush_has_builtin_fence = 0;
			info->fence = memory_barrier;
		}
	}

	/*
	 * XXX Disable this work around for Intel CPUs with optimized
	 * WC eviction.
	 */
	int wc_workaround = is_cpu_genuine_intel();

	char *ptr = os_getenv("PMEM_WC_WORKAROUND");
	if (ptr) {
		if (strcmp(ptr, "1") == 0) {
			LOG(3, "WC workaround forced to 1");
			wc_workaround = 1;
		} else if (strcmp(ptr, "0") == 0) {
			LOG(3, "WC workaround forced to 0");
			wc_workaround = 0;
		} else {
			LOG(3, "incorrect value of PMEM_WC_WORKAROUND (%s)",
				ptr);
		}
	}
	LOG(3, "WC workaround = %d", wc_workaround);

	ptr = os_getenv("PMEM_NO_MOVNT");
	if (ptr && strcmp(ptr, "1") == 0) {
		LOG(3, "PMEM_NO_MOVNT forced no movnt");
	} else {
		use_sse2_memcpy_memset(info, impl, wc_workaround);

		if (is_cpu_avx_present())
			use_avx_memcpy_memset(info, impl, wc_workaround);

		if (is_cpu_avx512f_present())
			use_avx512f_memcpy_memset(info, impl);

		if (is_cpu_movdir64b_present())
			use_movdir64b_memcpy_memset(info, impl);
	}
}

/*
 * pmem2_arch_init -- initialize architecture-specific list of pmem operations
 */
void
pmem2_arch_init(struct pmem2_arch_info *info)
{
	LOG(3, NULL);
	enum memcpy_impl impl = MEMCPY_INVALID;

	pmem_cpuinfo_to_funcs(info, &impl);

	/*
	 * For testing, allow overriding the default threshold
	 * for using non-temporal stores in pmem_memcpy_*(), pmem_memmove_*()
	 * and pmem_memset_*().
	 * It has no effect if movnt is not supported or disabled.
	 */
	const char *ptr = os_getenv("PMEM_MOVNT_THRESHOLD");
	if (ptr) {
		long long val = atoll(ptr);

		if (val < 0) {
			LOG(3, "Invalid PMEM_MOVNT_THRESHOLD");
		} else {
			LOG(3, "PMEM_MOVNT_THRESHOLD set to %zu", (size_t)val);
			Movnt_threshold = (size_t)val;
		}
	}

	if (info->flush == flush_clwb)
		LOG(3, "using clwb");
	else if (info->flush == flush_clflushopt)
		LOG(3, "using clflushopt");
	else if (info->flush == flush_clflush)
		LOG(3, "using clflush");
	else
		FATAL("invalid deep flush function address");

	if (impl == MEMCPY_MOVDIR64B)
		LOG(3, "using movnt MOVDIR64B");
	else if (impl == MEMCPY_AVX512F)
		LOG(3, "using movnt AVX512F");
	else if (impl == MEMCPY_AVX)
		LOG(3, "using movnt AVX");
	else if (impl == MEMCPY_SSE2)
		LOG(3, "using movnt SSE2");
}
