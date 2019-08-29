/*
 * Copyright 2018, Intel Corporation
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
 * memops_generic.c -- architecture-independent memmove & memset fallback
 *
 * This fallback is needed to fulfill guarantee that pmem_mem[cpy|set|move]
 * will use at least 8-byte stores (for 8-byte aligned buffers and sizes),
 * even when accelerated implementation is missing or disabled.
 * This guarantee is needed to maintain correctness eg in pmemobj.
 * Libc may do the same, but this behavior is not documented, so we can't rely
 * on that.
 */

#include <stddef.h>

#include "out.h"
#include "pmem.h"
#include "libpmem.h"
#include "util.h"

/*
 * cpy128 -- (internal) copy 128 bytes from src to dst
 */
static force_inline void
cpy128(uint64_t *dst, const uint64_t *src)
{
	/*
	 * We use atomics here just to be sure compiler will not split stores.
	 * Order of stores doesn't matter.
	 */
	uint64_t tmp[16];
	util_atomic_load_explicit64(&src[0], &tmp[0], memory_order_relaxed);
	util_atomic_load_explicit64(&src[1], &tmp[1], memory_order_relaxed);
	util_atomic_load_explicit64(&src[2], &tmp[2], memory_order_relaxed);
	util_atomic_load_explicit64(&src[3], &tmp[3], memory_order_relaxed);
	util_atomic_load_explicit64(&src[4], &tmp[4], memory_order_relaxed);
	util_atomic_load_explicit64(&src[5], &tmp[5], memory_order_relaxed);
	util_atomic_load_explicit64(&src[6], &tmp[6], memory_order_relaxed);
	util_atomic_load_explicit64(&src[7], &tmp[7], memory_order_relaxed);
	util_atomic_load_explicit64(&src[8], &tmp[8], memory_order_relaxed);
	util_atomic_load_explicit64(&src[9], &tmp[9], memory_order_relaxed);
	util_atomic_load_explicit64(&src[10], &tmp[10], memory_order_relaxed);
	util_atomic_load_explicit64(&src[11], &tmp[11], memory_order_relaxed);
	util_atomic_load_explicit64(&src[12], &tmp[12], memory_order_relaxed);
	util_atomic_load_explicit64(&src[13], &tmp[13], memory_order_relaxed);
	util_atomic_load_explicit64(&src[14], &tmp[14], memory_order_relaxed);
	util_atomic_load_explicit64(&src[15], &tmp[15], memory_order_relaxed);

	util_atomic_store_explicit64(&dst[0], tmp[0], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[1], tmp[1], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[2], tmp[2], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[3], tmp[3], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[4], tmp[4], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[5], tmp[5], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[6], tmp[6], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[7], tmp[7], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[8], tmp[8], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[9], tmp[9], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[10], tmp[10], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[11], tmp[11], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[12], tmp[12], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[13], tmp[13], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[14], tmp[14], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[15], tmp[15], memory_order_relaxed);
}

/*
 * cpy64 -- (internal) copy 64 bytes from src to dst
 */
static force_inline void
cpy64(uint64_t *dst, const uint64_t *src)
{
	/*
	 * We use atomics here just to be sure compiler will not split stores.
	 * Order of stores doesn't matter.
	 */
	uint64_t tmp[8];
	util_atomic_load_explicit64(&src[0], &tmp[0], memory_order_relaxed);
	util_atomic_load_explicit64(&src[1], &tmp[1], memory_order_relaxed);
	util_atomic_load_explicit64(&src[2], &tmp[2], memory_order_relaxed);
	util_atomic_load_explicit64(&src[3], &tmp[3], memory_order_relaxed);
	util_atomic_load_explicit64(&src[4], &tmp[4], memory_order_relaxed);
	util_atomic_load_explicit64(&src[5], &tmp[5], memory_order_relaxed);
	util_atomic_load_explicit64(&src[6], &tmp[6], memory_order_relaxed);
	util_atomic_load_explicit64(&src[7], &tmp[7], memory_order_relaxed);

	util_atomic_store_explicit64(&dst[0], tmp[0], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[1], tmp[1], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[2], tmp[2], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[3], tmp[3], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[4], tmp[4], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[5], tmp[5], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[6], tmp[6], memory_order_relaxed);
	util_atomic_store_explicit64(&dst[7], tmp[7], memory_order_relaxed);
}

/*
 * cpy8 -- (internal) copy 8 bytes from src to dst
 */
static force_inline void
cpy8(uint64_t *dst, const uint64_t *src)
{
	uint64_t tmp;
	util_atomic_load_explicit64(src, &tmp, memory_order_relaxed);
	util_atomic_store_explicit64(dst, tmp, memory_order_relaxed);
}

/*
 * store8 -- (internal) store 8 bytes
 */
static force_inline void
store8(uint64_t *dst, uint64_t c)
{
	util_atomic_store_explicit64(dst, c, memory_order_relaxed);
}

/*
 * memmove_nodrain_generic -- generic memmove to pmem without hw drain
 */
void *
memmove_nodrain_generic(void *dst, const void *src, size_t len,
		unsigned flags)
{
	LOG(15, "pmemdest %p src %p len %zu flags 0x%x", dst, src, len,
			flags);
	char *cdst = dst;
	const char *csrc = src;
	size_t remaining;
	(void) flags;

	if ((uintptr_t)cdst - (uintptr_t)csrc >= len) {
		size_t cnt = (uint64_t)cdst & 7;
		if (cnt > 0) {
			cnt = 8 - cnt;

			if (cnt > len)
				cnt = len;

			for (size_t i = 0; i < cnt; ++i)
				cdst[i] = csrc[i];

			pmem_flush_flags(cdst, cnt, flags);

			cdst += cnt;
			csrc += cnt;
			len -= cnt;
		}

		uint64_t *dst8 = (uint64_t *)cdst;
		const uint64_t *src8 = (const uint64_t *)csrc;

		while (len >= 128 && CACHELINE_SIZE == 128) {
			cpy128(dst8, src8);
			pmem_flush_flags(dst8, 128, flags);
			len -= 128;
			dst8 += 16;
			src8 += 16;
		}

		while (len >= 64) {
			cpy64(dst8, src8);
			pmem_flush_flags(dst8, 64, flags);
			len -= 64;
			dst8 += 8;
			src8 += 8;
		}

		remaining = len;
		while (len >= 8) {
			cpy8(dst8, src8);
			len -= 8;
			dst8++;
			src8++;
		}

		cdst = (char *)dst8;
		csrc = (const char *)src8;

		for (size_t i = 0; i < len; ++i)
			*cdst++ = *csrc++;

		if (remaining)
			pmem_flush_flags(cdst - remaining, remaining, flags);
	} else {
		cdst += len;
		csrc += len;

		size_t cnt = (uint64_t)cdst & 7;
		if (cnt > 0) {
			if (cnt > len)
				cnt = len;

			cdst -= cnt;
			csrc -= cnt;
			len -= cnt;

			for (size_t i = cnt; i > 0; --i)
				cdst[i - 1] = csrc[i - 1];
			pmem_flush_flags(cdst, cnt, flags);
		}

		uint64_t *dst8 = (uint64_t *)cdst;
		const uint64_t *src8 = (const uint64_t *)csrc;

		while (len >= 128 && CACHELINE_SIZE == 128) {
			dst8 -= 16;
			src8 -= 16;
			cpy128(dst8, src8);
			pmem_flush_flags(dst8, 128, flags);
			len -= 128;
		}

		while (len >= 64) {
			dst8 -= 8;
			src8 -= 8;
			cpy64(dst8, src8);
			pmem_flush_flags(dst8, 64, flags);
			len -= 64;
		}

		remaining = len;
		while (len >= 8) {
			--dst8;
			--src8;
			cpy8(dst8, src8);
			len -= 8;
		}

		cdst = (char *)dst8;
		csrc = (const char *)src8;

		for (size_t i = len; i > 0; --i)
			*--cdst = *--csrc;

		if (remaining)
			pmem_flush_flags(cdst, remaining, flags);
	}

	return dst;
}

/*
 * memset_nodrain_generic -- generic memset to pmem without hw drain
 */
void *
memset_nodrain_generic(void *dst, int c, size_t len, unsigned flags)
{
	LOG(15, "pmemdest %p c 0x%x len %zu flags 0x%x", dst, c, len,
			flags);
	(void) flags;

	char *cdst = dst;
	size_t cnt = (uint64_t)cdst & 7;
	if (cnt > 0) {
		cnt = 8 - cnt;

		if (cnt > len)
			cnt = len;

		for (size_t i = 0; i < cnt; ++i)
			cdst[i] = (char)c;
		pmem_flush_flags(cdst, cnt, flags);

		cdst += cnt;
		len -= cnt;
	}

	uint64_t *dst8 = (uint64_t *)cdst;

	uint64_t u = (unsigned char)c;
	uint64_t tmp = (u << 56) | (u << 48) | (u << 40) | (u << 32) |
			(u << 24) | (u << 16) | (u << 8) | u;

	while (len >= 64) {
		store8(&dst8[0], tmp);
		store8(&dst8[1], tmp);
		store8(&dst8[2], tmp);
		store8(&dst8[3], tmp);
		store8(&dst8[4], tmp);
		store8(&dst8[5], tmp);
		store8(&dst8[6], tmp);
		store8(&dst8[7], tmp);
		pmem_flush_flags(dst8, 64, flags);
		len -= 64;
		dst8 += 8;
	}

	size_t remaining = len;
	while (len >= 8) {
		store8(dst8, tmp);
		len -= 8;
		dst8++;
	}

	cdst = (char *)dst8;

	for (size_t i = 0; i < len; ++i)
		*cdst++ = (char)c;

	if (remaining)
		pmem_flush_flags(cdst - remaining, remaining, flags);
	return dst;
}
