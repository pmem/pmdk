/*
 * Copyright (c) 2014-2015, Intel Corporation
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
 * pmem.c -- pmem entry points for libpmem
 *
 *
 * PERSISTENT MEMORY INSTRUCTIONS ON X86
 *
 * The primary feature of this library is to provide a way to flush
 * changes to persistent memory as outlined below (note that many
 * of the decisions below are made at initialization time, and not
 * repeated every time a flush is requested).
 *
 * To flush a range to pmem when CLWB is available:
 *
 *	CLWB for each cache line in the given range.
 *
 *	SFENCE to ensure the CLWBs above have completed.
 *
 *	PCOMMIT to mark pmem stores in the memory subsystem.
 *
 *	SFENCE to ensure the stores marked by PCOMMIT above have completed.
 *
 * To flush a range to pmem when CLFLUSHOPT is available and CLWB is not
 * (same as above but issue CLFLUSHOPT instead of CLWB):
 *
 *	CLFLUSHOPT for each cache line in the given range.
 *
 *	SFENCE to ensure the CLWBs above have completed.
 *
 *	PCOMMIT to mark pmem stores in the memory subsystem.
 *
 *	SFENCE to ensure the stores marked by PCOMMIT above have completed.
 *
 * To flush a range to pmem when neither CLFLUSHOPT or CLWB are available
 * (same as above but fences surrounding CLFLUSH are not required):
 *
 *	CLFLUSH for each cache line in the given range.
 *
 *	PCOMMIT to mark pmem stores in the memory subsystem.
 *
 *	SFENCE to ensure the stores marked by PCOMMIT above have completed.
 *
 * To flush a range to pmem when the caller has explicitly assumed
 * responsibility for draining HW stores in the memory subsystem
 * (by choosing to depend on ADR, or by assuming responsibility to issue
 * PCOMMIT/SFENCE at some point):
 *
 * 	Same as above flows but omit the final PCOMMIT and SFENCE.
 *
 * To memcpy a range of memory to pmem when MOVNT is available:
 *
 *	Copy any non-64-byte portion of the destination using MOV.
 *
 *	Use the non-PCOMMIT flush flow above for the copied portion.
 *
 *	Copy using MOVNTDQ, up to any non-64-byte aligned end portion.
 *	(The MOVNT instructions bypass the cache, so no flush is required.)
 *
 *	Copy any unaligned end portion using MOV.
 *
 *	Use the flush flow above for the copied portion (including PCOMMIT).
 *
 * To memcpy a range of memory to pmem when MOVNT is not available:
 *
 *	Just pass the call to the normal memcpy() followed by pmem_persist().
 *
 * To memset a non-trivial sized range of memory to pmem:
 *
 *	Same as the memcpy cases above but store the given value instead
 *	of reading values from the source.
 *
 *
 * INTERFACES FOR FLUSHING TO PERSISTENT MEMORY
 *
 * Given the flows above, three interfaces are provided for flushing a range
 * so that the caller has the ability to separate the steps when necessary,
 * but otherwise leaves the detection of available instructions to the libpmem:
 *
 * pmem_persist(addr, len)
 *
 *	This is the common case, which just calls the two other functions:
 *
 *		pmem_flush(addr, len);
 *		pmem_drain();
 *
 * pmem_flush(addr, len)
 *
 *	CLWB or CLFLUSHOPT or CLFLUSH for each cache line
 *
 * pmem_drain()
 *
 *	SFENCE unless using CLFLUSH
 *
 *	PCOMMIT
 *
 *	SFENCE
 *
 * When PCOMMIT is unavailable, either because the platform doesn't support it
 * or because it has been inhibited by the caller by setting PMEM_NO_PCOMMIT=1,
 * the pmem_drain() function degenerates into this:
 *
 * pmem_drain()
 *
 *	SFENCE unless using CLFLUSH
 *
 *
 * INTERFACES FOR COPYING/SETTING RANGES OF MEMORY
 *
 * Given the flows above, the following interfaces are provided for the
 * memmove/memcpy/memset operations to persistent memory:
 *
 * pmem_memmove_nodrain()
 *
 * 	Checks for overlapped ranges to determine whether to copy from
 * 	the beginning of the range or the from the end.  If MOVNT instructions
 * 	are available, uses the memory copy flow described above, otherwise
 * 	calls the libc memmove() followed by pmem_flush().
 *
 * pmem_memcpy_nodrain()
 *
 * 	Calls pmem_memset_nodrain().
 *
 * pmem_memset_nodrain()
 *
 * 	If MOVNT instructions are available, uses the memset flow described
 * 	above, otherwise calls the libc memset() followed by pmem_flush().
 *
 * pmem_memmove()
 * pmem_memcpy()
 * pmem_memset()
 *
 * 	Calls the appropriate _nodrain() function followed by pmem_drain().
 *
 *
 * DECISIONS MADE AT INITIALIZATION TIME
 *
 * As much as possible, all decisions described above are made at library
 * initialization time.  This is achieved using function pointers that are
 * setup by pmem_init() when the library loads.
 *
 * 	Func_predrain_fence is used by pmem_drain() to call one of:
 * 		predrain_fence_empty()
 * 		predrain_fence_sfence()
 *
 *	Func_drain is used by pmem_drain() to call one of:
 *		drain_no_pcommit()
 *		drain_pcommit()
 *
 *	Func_flush is used by pmem_flush() to call one of:
 *		flush_clwb()
 *		flush_clflushopt()
 *		flush_clflush()
 *
 *	Func_memmove_nodrain is used by memmove_nodrain() to call one of:
 *		memmove_nodrain_normal()
 *		memmove_nodrain_movnt()
 *
 *	Func_memset_nodrain is used by memset_nodrain() to call one of:
 *		memset_nodrain_normal()
 *		memset_nodrain_movnt()
 */

#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <xmmintrin.h>

#include "pmem.h"
#include "util.h"
#include "out.h"

/*
 * The x86 memory instructions are new enough that the compiler
 * intrinsic functions are not always available.  The intrinsic
 * functions are defined here in terms of asm statements for now.
 */
#define	_mm_clflushopt(addr)\
	asm volatile(".byte 0x66; clflush %0" : "+m" (*(volatile char *)addr));
#define	_mm_clwb(addr)\
	asm volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *)addr));
#define	_mm_pcommit()\
	asm volatile(".byte 0x66, 0x0f, 0xae, 0xf8");

#define	FLUSH_ALIGN 64 /* 64B cache line size */

#define	ALIGN_SHIFT	6
#define	ALIGN_MASK	(FLUSH_ALIGN - 1)

#define	CHUNK_SIZE	128 /* 16*8 */
#define	CHUNK_SHIFT	7
#define	CHUNK_MASK	(CHUNK_SIZE - 1)

#define	DWORD_SIZE	4
#define	DWORD_SHIFT	2
#define	DWORD_MASK	(DWORD_SIZE - 1)


#define	PROCMAXLEN	2048 /* maximum expected line length in /proc files */

static int Has_hw_drain;

/*
 * pmem_has_hw_drain -- return whether or not HW drain (PCOMMIT) was found
 */
int
pmem_has_hw_drain(void)
{
	return Has_hw_drain;
}

/*
 * predrain_fence_empty -- (internal) issue the pre-drain fence instruction
 */
static void
predrain_fence_empty(void)
{
	/* way too chatty for LOG level 3 */
	LOG(15, NULL);

	/* nothing to do (because CLFLUSH did it for us) */
}

/*
 * predrain_fence_sfence -- (internal) issue the pre-drain fence instruction
 */
static void
predrain_fence_sfence(void)
{
	/* way too chatty for LOG level 3 */
	LOG(15, NULL);

	_mm_sfence();	/* ensure CLWB or CLFLUSHOPT completes before PCOMMIT */
}

/*
 * pmem_drain() calls through Func_predrain_fence to do the fence.  Although
 * initialized to predrain_fence_empty(), once the existence of the CLWB or
 * CLFLUSHOPT feature is confirmed by pmem_init() at library initialization
 * time, Func_predrain_fence is set to predrain_fence_sfence().  That's the
 * most common case on modern hardware that supports persistent memory.
 */
static void (*Func_predrain_fence)(void) = predrain_fence_empty;

/*
 * drain_no_pcommit -- (internal) wait for PM stores to drain, empty version
 */
static void
drain_no_pcommit(void)
{
	/* way too chatty for LOG level 3 */
	LOG(15, NULL);

	Func_predrain_fence();

	/* caller assumed responsibility for the rest */
}

/*
 * drain_pcommit -- (internal) wait for PM stores to drain, pcommit version
 */
static void
drain_pcommit(void)
{
	/* way too chatty for LOG level 3 */
	LOG(15, NULL);

	Func_predrain_fence();
	_mm_pcommit();
	_mm_sfence();
}

/*
 * pmem_drain() calls through Func_drain to do the work.  Although
 * initialized to drain_no_pcommit(), once the existence of the pcommit
 * feature is confirmed by pmem_init() at library initialization time,
 * Func_drain is set to drain_pcommit().  That's the most common case
 * on modern hardware that supports persistent memory.
 */
static void (*Func_drain)(void) = drain_no_pcommit;

/*
 * pmem_drain -- wait for any PM stores to drain from HW buffers
 */
void
pmem_drain(void)
{
	Func_drain();
}

/*
 * flush_clflush -- (internal) flush the CPU cache, using clflush
 */
static void
flush_clflush(void *addr, size_t len)
{
	/* way too chatty for LOG level 3 */
	LOG(15, "addr %p len %zu", addr, len);

	uintptr_t uptr;

	/*
	 * Loop through cache-line-size (typically 64B) aligned chunks
	 * covering the given range.
	 */
	for (uptr = (uintptr_t)addr & ~(FLUSH_ALIGN - 1);
		uptr < (uintptr_t)addr + len; uptr += FLUSH_ALIGN)
		_mm_clflush((char *)uptr);
}

/*
 * flush_clwb -- (internal) flush the CPU cache, using clwb
 */
static void
flush_clwb(void *addr, size_t len)
{
	/* way too chatty for LOG level 3 */
	LOG(15, "addr %p len %zu", addr, len);

	uintptr_t uptr;

	/*
	 * Loop through cache-line-size (typically 64B) aligned chunks
	 * covering the given range.
	 */
	for (uptr = (uintptr_t)addr & ~(FLUSH_ALIGN - 1);
		uptr < (uintptr_t)addr + len; uptr += FLUSH_ALIGN) {
		_mm_clwb((char *)uptr);
	}
}

/*
 * flush_clflushopt -- (internal) flush the CPU cache, using clflushopt
 */
static void
flush_clflushopt(void *addr, size_t len)
{
	/* way too chatty for LOG level 3 */
	LOG(15, "addr %p len %zu", addr, len);

	uintptr_t uptr;

	/*
	 * Loop through cache-line-size (typically 64B) aligned chunks
	 * covering the given range.
	 */
	for (uptr = (uintptr_t)addr & ~(FLUSH_ALIGN - 1);
		uptr < (uintptr_t)addr + len; uptr += FLUSH_ALIGN) {
		_mm_clflushopt((char *)uptr);
	}
}

/*
 * pmem_flush() calls through Func_flush to do the work.  Although
 * initialized to flush_clflush(), once the existence of the clflushopt
 * feature is confirmed by pmem_init() at library initialization time,
 * Func_flush is set to flush_clflushopt().  That's the most common case
 * on modern hardware that supports persistent memory.
 */
static void (*Func_flush)(void *, size_t) = flush_clflush;

/*
 * pmem_flush -- flush processor cache for the given range
 */
void
pmem_flush(void *addr, size_t len)
{
	Func_flush(addr, len);
}

/*
 * pmem_persist -- make any cached changes to a range of pmem persistent
 */
void
pmem_persist(void *addr, size_t len)
{
	/* way too chatty for LOG level 3 */
	LOG(15, "addr %p len %zu", addr, len);

	pmem_flush(addr, len);
	pmem_drain();
}

/*
 * pmem_msync -- flush to persistence via msync
 *
 * Using msync() means this routine is less optimal for pmem (but it
 * still works) but it also works for any memory mapped file, unlike
 * pmem_persist() which is only safe where pmem_is_pmem() returns true.
 */
int
pmem_msync(void *addr, size_t len)
{
	/* way too chatty for LOG level 3 */
	LOG(15, "addr %p len %zu", addr, len);

	/*
	 * msync requires len to be a multiple of pagesize, so
	 * adjust addr and len to represent the full 4k chunks
	 * covering the given range.
	 */

	/* increase len by the amount we gain when we round addr down */
	len += (uintptr_t)addr & (Pagesize - 1);

	/* round addr down to page boundary */
	uintptr_t uptr = (uintptr_t)addr & ~(Pagesize - 1);

	int ret;
	if ((ret = msync((void *)uptr, len, MS_SYNC)) < 0)
		LOG(1, "!msync");

	return ret;
}

/*
 * pmem_align -- align destination address on 16 byte boundary.
 */
static void
pmem_align(void **dest, const void *src, size_t *cnt, int direction)
{

	int i;

	if ((*cnt = ((uint64_t)*dest & MOVNT_MASK)) == 0) {
		return;
	}
	if (direction == UP)
		*cnt = MOVNT_SIZE - *cnt;

	if (*cnt != 0 && direction == UP) {
		/* dest is unaligned */
		uint8_t	*d8 = (uint8_t *)*dest;
		const uint8_t *s8 = (uint8_t *)src;
		for (i = 0; i < *cnt; i++) {
			*d8 = *s8;
			d8++;
			s8++;
		}
		*dest += *cnt;
		pmem_flush(*dest, *cnt);
	} else {
		uint8_t	*d8 = (uint8_t *)*dest;
		const uint8_t *s8 = (uint8_t *)src;
		for (i = 0; i < *cnt; i++) {
			d8--;
			s8--;
			*d8 = *s8;
		}
		*dest -= *cnt;
		pmem_flush(*dest, *cnt);
	}
}

/*
 * is_pmem_always -- (internal) always true version of pmem_is_pmem()
 */
static int
is_pmem_always(void *addr, size_t len)
{
	LOG(3, NULL);

	return 1;
}

/*
 * is_pmem_never -- (internal) never true version of pmem_is_pmem()
 */
static int
is_pmem_never(void *addr, size_t len)
{
	LOG(3, NULL);

	return 0;
}

/*
 * is_pmem_proc -- (internal) use /proc to implement pmem_is_pmem()
 *
 * This function returns true only if the entire range can be confirmed
 * as being direct access persistent memory.  Finding any part of the
 * range is not direct access, or failing to look up the information
 * because it is unmapped or because any sort of error happens, just
 * results in returning false.
 *
 * This function works by lookup up the range in /proc/self/smaps and
 * verifying the "mixed map" vmflag is set for that range.  While this
 * isn't exactly the same as direct access, there is no DAX flag in
 * the vmflags and the mixed map flag is only true on regular files when
 * DAX is in-use, so it serves the purpose.
 *
 * The range passed in may overlap with multiple entries in the smaps list
 * so this function loops through the smaps entries until the entire range
 * is verified as direct access, or until it is clear the answer is false
 * in which case it stops the loop and returns immediately.
 */
static int
is_pmem_proc(void *addr, size_t len)
{
	char *caddr = addr;

	FILE *fp;
	if ((fp = fopen("/proc/self/smaps", "r")) == NULL) {
		LOG(1, "!/proc/self/smaps");
		return 0;
	}

	int retval = 0;		/* assume false until proven otherwise */
	char line[PROCMAXLEN];	/* for fgets() */
	char *lo = NULL;	/* beginning of current range in smaps file */
	char *hi = NULL;	/* end of current range in smaps file */
	int needmm = 0;		/* looking for mm flag for current range */
	while (fgets(line, PROCMAXLEN, fp) != NULL) {
		static const char vmflags[] = "VmFlags:";
		static const char mm[] = " mm";

		/* check for range line */
		if (sscanf(line, "%p-%p", &lo, &hi) == 2) {
			if (needmm) {
				/* last range matched, but no mm flag found */
				LOG(4, "never found mm flag");
				break;
			} else if (caddr < lo) {
				/* never found the range for caddr */
				LOG(4, "no match for addr %p", caddr);
				break;
			} else if (caddr < hi) {
				/* start address is in this range */
				size_t rangelen = hi - caddr;

				/* remember that matching has started */
				needmm = 1;

				/* calculate remaining range to search for */
				if (len > rangelen) {
					len -= rangelen;
					caddr += rangelen;
					LOG(4, "matched %zu bytes in range "
							"%p-%p, %zu left over",
							rangelen, lo, hi, len);
				} else {
					len = 0;
					LOG(4, "matched all bytes in range "
							"%p-%p", lo, hi);
				}
			}
		} else if (needmm && strncmp(line, vmflags,
					sizeof (vmflags) - 1) == 0) {
			if (strstr(&line[sizeof (vmflags) - 1], mm) != NULL) {
				LOG(4, "mm flag found");
				if (len == 0) {
					/* entire range matched */
					retval = 1;
					break;
				}
				needmm = 0;	/* saw what was needed */
			} else {
				/* mm flag not set for some or all of range */
				LOG(4, "range has no mm flag");
				break;
			}
		}
	}

	fclose(fp);

	LOG(3, "returning %d", retval);
	return retval;
}

/*
 * pmem_is_pmem() calls through Func_is_pmem to do the work.  Although
 * initialized to is_pmem_never(), once the existence of the clflush
 * feature is confirmed by pmem_init() at library initialization time,
 * Func_is_pmem is set to is_pmem_proc().  That's the most common case
 * on modern hardware.
 */
static int (*Func_is_pmem)(void *addr, size_t len) = is_pmem_never;

/*
 * pmem_is_pmem -- return true if entire range is persistent Memory
 */
int
pmem_is_pmem(void *addr, size_t len)
{
	return Func_is_pmem(addr, len);
}

/*
 * pmem_map -- map the entire file for read/write access
 */
void *
pmem_map(int fd)
{
	LOG(3, "fd %d", fd);

	struct stat stbuf;
	if (fstat(fd, &stbuf) < 0) {
		LOG(1, "!fstat");
		return NULL;
	}

	void *addr;
	if ((addr = util_map(fd, stbuf.st_size, 0)) == NULL)
		return NULL;    /* util_map() set errno, called LOG */

	LOG(3, "returning %p", addr);
	return addr;
}

/*
 * memmove_nodrain_normal -- (internal) memmove to pmem without hw drain
 */
static void *
memmove_nodrain_normal(void *pmemdest, const void *src, size_t len)
{
	/* way too chatty for LOG level 3 */
	LOG(15, "pmemdest %p src %p len %zu", pmemdest, src, len);

	return memmove(pmemdest, src, len);

}

/*
 * memcpy_nodrain_normal -- (internal) memmcpy to pmem without hw drain
 */
static void *
memcpy_nodrain_normal(void *pmemdest, const void *src, size_t len)
{
	return memcpy(pmemdest, src, len);
}

/*
 * nodrain_movnt -- (internal) memXXX worker function without hw drain, movnt
 */

static void *
nodrain_movnt(void *pmemdest, void *src, size_t len)
{
	/* way too chatty for LOG level 3 */
	LOG(15, "pmemdest %p src %p len %zu", pmemdest, src, len);

	__m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
	size_t i;
	__m128i *d;
	__m128i *s;
	void *tempdest = pmemdest;
	size_t cnt;

	if (len == 0)
		return NULL;

	if (src == pmemdest)
		return pmemdest;

	d = (__m128i *)pmemdest;
	s = (__m128i *)src;

	cnt = len >> CHUNK_SHIFT;
	for (i = 0; i < cnt; i++) {
		xmm0 = _mm_loadu_si128(s);
		xmm1 = _mm_loadu_si128(s + 1);
		xmm2 = _mm_loadu_si128(s + 2);
		xmm3 = _mm_loadu_si128(s + 3);
		xmm4 = _mm_loadu_si128(s + 4);
		xmm5 = _mm_loadu_si128(s + 5);
		xmm6 = _mm_loadu_si128(s + 6);
		xmm7 = _mm_loadu_si128(s + 7);
		s += 8;
		/* Store double quadword with non-temporal hint */
		_mm_stream_si128(d,	xmm0);
		_mm_stream_si128(d + 1,	xmm1);
		_mm_stream_si128(d + 2,	xmm2);
		_mm_stream_si128(d + 3,	xmm3);
		_mm_stream_si128(d + 4,	xmm4);
		_mm_stream_si128(d + 5,	xmm5);
		_mm_stream_si128(d + 6,	xmm6);
		_mm_stream_si128(d + 7,	xmm7);
		d += 8;
	}

	/* Copy the tail (<128 bytes) in 16 bytes chunks */
	len &= CHUNK_MASK;
	if (len != 0) {
		cnt = len >> MOVNT_SHIFT;
		for (i = 0; i < cnt; i++) {
			xmm0 = _mm_loadu_si128(s);
			_mm_stream_si128(d, xmm0);
			s++;
			d++;
		}
	}

	/* Copy the last bytes (<16). First dwords then bytes */
	len &= MOVNT_MASK;
	if (len != 0) {
		cnt = len >> DWORD_SHIFT;
		int32_t	*d32 = (int32_t *)d;
		int32_t	*s32 = (int32_t *)s;

		for (i = 0; i < cnt; i++) {
			_mm_stream_si32(d32, *s32);
			d32++;
			s32++;
		}

		cnt =	len & DWORD_MASK;
		uint8_t	*d8 = (uint8_t *)d32;
		const uint8_t	*s8 = (uint8_t *)s32;

		for (i = 0; i < cnt; i++) {
			*d8 = *s8;
			d8++;
			s8++;
		}
		pmem_flush(d8, cnt);
	}
	return tempdest;
}

/*
 * memmove_nodrain_movnt -- (internal) memmove to pmem without hw drain, movnt
 */
static void *
memmove_nodrain_movnt(void *pmemdest, const void *src, size_t len)
{

	int	i;
	uint64_t chunk_size;
	uint64_t num_chunks;
	size_t cnt = 0;
	void *dest1 = pmemdest;
	void *src1 = (void *)src;
	int mod = 0;

	/* way too chatty for LOG level 3 */
	LOG(15, "pmemdest %p src %p len %zu", pmemdest, src, len);

	/*
	 * The way this works is:
	 * 	Calculate the non-overlapping size
	 * 	Break the memmove into these chunk sizes
	 *	Determine if we are copying forward/backward
	 * This handles all cases, including overlap. Consider the following:
	 *
	 * 	src
	 * high addr				dest
	 *					high addr
	 * ------
	 * |	|s3-----------------------------------------------V
	 * ------						--------
	 * |	|s2-----------------------------|		|	|d3
	 * ------				|-------|	--------
	 * |	|s1---------------------------- 	|------>|	|d2
	 * ------				|		--------
	 *					|-------------->|	|d1
	 * **** 						--------
	 *
	 * In this case we copy down, starting with s1 to d1, so that we
	 * never overwrite the source before moving it. Any leftover
	 * bytes are handled after moving the chunk size data.
	 * This works similarly when src < dest.
	 */
	if (src > pmemdest)
		chunk_size = util_nonoverlap_range((void *)src1, dest1, len);
	else
		chunk_size = util_nonoverlap_range(dest1, src1, len);

	/*
	 * The most common case is non-overlapping addresses.
	 */
	if (chunk_size == len) {
		pmem_align(&dest1, src1, &cnt, UP);
		src1 += cnt;
		len -= cnt;
		return (pmemdest = (nodrain_movnt(dest1, src1, len)));
	}

	/* overlapping addresses, possible unaligned dest. */
	if ((uint64_t)src1 > (uint64_t)dest1) {
		pmem_align(&dest1, src1, &cnt, UP);
		len -= cnt;
		src1 += cnt;

		/* Align the chunk size */
		if (chunk_size > MOVNT_SIZE) {
			if ((mod = (chunk_size % MOVNT_SIZE)) != 0)
				chunk_size -= mod;
		}
		num_chunks = len/chunk_size;

		for (i = 0; i < num_chunks; i ++) {
			nodrain_movnt(dest1, src1, chunk_size);
			src1 += chunk_size;
			len -= chunk_size;
			dest1 += chunk_size;
		}

		if (len != 0)
			dest1 = nodrain_movnt(dest1, src1, len);
	} else {


		if (chunk_size	> MOVNT_SIZE) {
			if ((mod = (chunk_size % MOVNT_SIZE)) != 0)
				chunk_size -= mod;
		}

		dest1 = dest1 + len;
		src1 = src1 + len;
		pmem_align(&dest1, src1, &cnt, DOWN);
		src1 -= cnt;
		len -= cnt;

		/* Reduce len by 1 to not exceed the mapped memory. */
		len -= 1;

		/* Start at the 1st chunk address so we can copy up */
		dest1 -= chunk_size;
		src1 -= chunk_size;

		num_chunks = len/chunk_size;

		for (i = 0; i < num_chunks - 1; i ++) {
			nodrain_movnt(dest1, src1, chunk_size);
			src1 -= chunk_size;
			dest1 -= chunk_size;
			len -= chunk_size;
		}

		if (len != 0)
			dest1 = nodrain_movnt(dest1, src1, len);
	}
	return pmemdest;
}

/*
 * memcpy_nodrain_movnt -- (internal) memcpy to pmem without hw drain, movnt
 */
static void *
memcpy_nodrain_movnt(void *pmemdest, const void *src, size_t len)
{

	size_t cnt;
	void *dest1 = pmemdest;

	/* way too chatty for LOG level 3 */
	LOG(15, "pmemdest %p src %p len %zu", pmemdest, src, len);

	pmem_align(&dest1, src, &cnt, UP);
	src += cnt;
	len -= cnt;

	/*
	 * If the pmemdest and src addresses overlap the behavior
	 * is undefined.
	 */
	dest1 = nodrain_movnt(dest1, (void *)src, len);
	return (pmemdest);
}

/*
 * pmem_memmove_nodrain() calls through Func_memmove_nodrain to do the work.
 * Although initialized to memmove_nodrain_normal(), once the existence of the
 * sse2 feature is confirmed by pmem_init() at library initialization time,
 * Func_memmove_nodrain is set to memmove_nodrain_movnt().  That's the most
 * common case on modern hardware that supports persistent memory.
 */
static void *(*Func_memmove_nodrain)
	(void *pmemdest, const void *src, size_t len) = memmove_nodrain_normal;

/*
 * pmem_memcpy_nodrain() calls through Func_memcpy_nodrain to do the work.
 * Although initialized to memcpy_nodrain_normal(), once the existence of the
 * sse2 feature is confirmed by pmem_init() at library initialization time,
 * Func_memcpy_nodrain is set to memcpy_nodrain_movnt().  That's the most
 * common case on modern hardware that supports persistent memory.
 */
static void *(*Func_memcpy_nodrain)
	(void *pmemdest, const void *src, size_t len) = memcpy_nodrain_normal;

/*
 * pmem_memmove_nodrain -- memmove to pmem without hw drain
 */
void *
pmem_memmove_nodrain(void *pmemdest, const void *src, size_t len)
{
	return Func_memmove_nodrain(pmemdest, src, len);
}

/*
 * pmem_memcpy_nodrain -- memcpy to pmem without hw drain
 */
void *
pmem_memcpy_nodrain(void *pmemdest, const void *src, size_t len)
{
	/* way too chatty for LOG level 3 */
	LOG(15, "pmemdest %p src %p len %zu", pmemdest, src, len);
	return Func_memcpy_nodrain(pmemdest, src, len);
}

/*
 * pmem_memmove -- memmove to pmem
 */
void *
pmem_memmove(void *pmemdest, const void *src, size_t len)
{
	/* way too chatty for LOG level 3 */
	LOG(15, "pmemdest %p src %p len %zu", pmemdest, src, len);

	void *retval = pmem_memmove_nodrain(pmemdest, src, len);
	pmem_drain();
	return retval;
}

/*
 * pmem_memcpy -- memcpy to pmem
 */
void *
pmem_memcpy(void *pmemdest, const void *src, size_t len)
{
	/* way too chatty for LOG level 3 */
	LOG(15, "pmemdest %p src %p len %zu", pmemdest, src, len);

	void *retval = pmem_memcpy_nodrain(pmemdest, src, len);
	pmem_drain();
	return retval;
}

/*
 * memset_nodrain_normal -- (internal) memset to pmem without hw drain, normal
 */
static void *
memset_nodrain_normal(void *pmemdest, int c, size_t len)
{
	/* way too chatty for LOG level 3 */
	LOG(15, "pmemdest %p c 0x%x len %zu", pmemdest, c, len);

	return memset(pmemdest, c, len);
}

/*
 * memset_nodrain_movnt -- (internal) memset to pmem without hw drain, movnt
 */
static void *
memset_nodrain_movnt(void *pmemdest, int c, size_t len)
{
	int i;
	char buf[CHUNK_SIZE];
	void *dest1 = pmemdest;
	size_t cnt = 0;
	int num_chunks;

	/* way too chatty for LOG level 3 */
	LOG(15, "pmemdest %p c 0x%x len %zu", pmemdest, c, len);

	/*
	 * Align initial address. From there on set data in CHUNK_SIZE
	 * bytes.
	 */
	memset_nodrain_normal(buf, c, CHUNK_SIZE);
	pmem_align(&dest1, buf, &cnt, UP);
	len -= cnt;

	/* 128 byte chunks */
	num_chunks = len/CHUNK_SIZE;

	for (i = 0; i < num_chunks; i++) {
		nodrain_movnt(dest1, buf, CHUNK_SIZE);
		dest1 += CHUNK_SIZE;
		len -= CHUNK_SIZE;
	}

	if (len != 0)
		nodrain_movnt(dest1, buf, len);

	return pmemdest;
}

/*
 * pmem_memset_nodrain() calls through Func_memset_nodrain to do the work.
 * Although initialized to memset_nodrain_normal(), once the existence of the
 * sse2 feature is confirmed by pmem_init() at library initialization time,
 * Func_memset_nodrain is set to memset_nodrain_movnt().  That's the most
 * common case on modern hardware that supports persistent memory.
 */
static void *(*Func_memset_nodrain)
	(void *pmemdest, int c, size_t len) = memset_nodrain_normal;

/*
 * pmem_memset_nodrain -- memset to pmem without hw drain
 */
void *
pmem_memset_nodrain(void *pmemdest, int c
, size_t len)
{
	return Func_memset_nodrain(pmemdest, c, len);
}

/*
 * pmem_memset -- memset to pmem
 */
void *
pmem_memset(void *pmemdest, int c, size_t len)
{
	/* way too chatty for LOG level 3 */
	LOG(15, "pmemdest %p c 0x%x len %zu", pmemdest, c, len);

	void *retval = pmem_memset_nodrain(pmemdest, c, len);
	pmem_drain();
	return retval;
}

/*
 * pmem_init -- load-time initialization for pmem.c
 *
 * Called automatically by the run-time loader.
 */
__attribute__((constructor))
static void
pmem_init(void)
{
	out_init(PMEM_LOG_PREFIX, PMEM_LOG_LEVEL_VAR, PMEM_LOG_FILE_VAR);
	LOG(3, NULL);
	util_init();

	/* detect supported cache flush features */
	FILE *fp;
	if ((fp = fopen("/proc/cpuinfo", "r")) == NULL) {
		LOG(1, "!/proc/cpuinfo");
	} else {
		char line[PROCMAXLEN];	/* for fgets() */

		while (fgets(line, PROCMAXLEN, fp) != NULL) {
			static const char flags[] = "flags\t\t: ";
			static const char clflush[] = " clflush ";
			static const char clwb[] = " clwb ";
			static const char clflushopt[] = " clflushopt ";
			static const char pcommit[] = " pcommit ";
			static const char sse2[] = " sse2 ";

			if (strncmp(flags, line, sizeof (flags) - 1) == 0) {
				/* start of list of flags */
				char *flags = &line[sizeof (flags) - 1];

				/* change ending newline to space delimiter */
				char *nl = strrchr(line, '\n');
				if (nl)
					*nl = ' ';

				if (strstr(flags, clflush) != NULL) {
					Func_is_pmem = is_pmem_proc;
					LOG(3, "clflush supported");
				}

				if (strstr(flags, clwb) != NULL) {
					LOG(3, "clwb supported");

					char *e = getenv("PMEM_NO_CLWB");
					if (e && strcmp(e, "1") == 0)
						LOG(3, "PMEM_NO_CLWB "
							"forced no clwb");
					else {
						Func_flush = flush_clwb;
						Func_predrain_fence =
							predrain_fence_sfence;
					}
				}

				if (strstr(flags, clflushopt) != NULL) {
					LOG(3, "clflushopt supported");

					char *e = getenv("PMEM_NO_CLFLUSHOPT");
					if (e && strcmp(e, "1") == 0)
						LOG(3, "PMEM_NO_CLFLUSHOPT "
							"forced no clflushopt");
					else {
						Func_flush = flush_clflushopt;
						Func_predrain_fence =
							predrain_fence_sfence;
					}
				}

				if (strstr(flags, pcommit) != NULL) {
					LOG(3, "pcommit supported");

					char *e = getenv("PMEM_NO_PCOMMIT");
					if (e && strcmp(e, "1") == 0)
						LOG(3, "PMEM_NO_PCOMMIT "
							"forced no pcommit");
					else {
						Func_drain = drain_pcommit;
						Has_hw_drain = 1;
					}
				}

				if (strstr(flags, sse2) != NULL) {
					LOG(3, "movnt supported");

					char *e = getenv("PMEM_NO_MOVNT");
					if (e && strcmp(e, "1") == 0)
						LOG(3, "PMEM_NO_MOVNT "
							"forced no movnt");
					else {
						Func_memmove_nodrain =
							memmove_nodrain_movnt;
						Func_memset_nodrain =
							memset_nodrain_movnt;
						Func_memcpy_nodrain =
							memcpy_nodrain_movnt;
					}
				}

				break;
			}
		}

		fclose(fp);
	}

	/*
	 * For debugging/testing, allow pmem_is_pmem() to be forced
	 * to always true or never true using environment variable
	 * PMEM_IS_PMEM_FORCE values of zero or one.
	 *
	 * This isn't #ifdef DEBUG because it has a trivial performance
	 * impact and it may turn out to be useful as a "chicken bit" for
	 * systems where pmem_is_pmem() isn't correctly detecting true
	 * persistent memory.
	 */
	char *ptr = getenv("PMEM_IS_PMEM_FORCE");
	if (ptr) {
		int val = atoi(ptr);

		if (val == 0)
			Func_is_pmem = is_pmem_never;
		else if (val == 1)
			Func_is_pmem = is_pmem_always;
	}
}
