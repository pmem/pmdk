/*
 * Copyright (c) 2014, Intel Corporation
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
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "libpmem.h"
#include "pmem.h"
#include "util.h"
#include "out.h"

#define	FLUSH_ALIGN 64

#define	PROCMAXLEN 2048 /* maximum expected line length in /proc files */

/*
 * pmem_drain -- wait for any PM stores to drain from HW buffers
 */
void
pmem_drain(void)
{
	/*
	 * Nothing to do here -- assumes an auto-drain feature like ADR.
	 *
	 * XXX handle drain for other platforms
	 */
}

/*
 * flush_clflush -- (internal) flush the CPU cache, using clflush
 */
static void
flush_clflush(void *addr, size_t len)
{
	uintptr_t uptr;

	/*
	 * Loop through cache-line-size (typically 64B) aligned chunks
	 * covering the given range.
	 */
	for (uptr = (uintptr_t)addr & ~(FLUSH_ALIGN - 1);
		uptr < (uintptr_t)addr + len; uptr += FLUSH_ALIGN)
		__builtin_ia32_clflush((char *)uptr);
}

/*
 * flush_clflushopt -- (internal) flush the CPU cache, using clflushopt
 */
static void
flush_clflushopt(void *addr, size_t len)
{
	uintptr_t uptr;

	/*
	 * Loop through cache-line-size (typically 64B) aligned chunks
	 * covering the given range.
	 */
	__builtin_ia32_sfence();
	for (uptr = (uintptr_t)addr & ~(FLUSH_ALIGN - 1);
		uptr < (uintptr_t)addr + len; uptr += FLUSH_ALIGN) {
		/*
		 * __builtin_ia32_clflushopt((char *)uptr);
		 *
		 * ...but that intrinsic isn't in all compilers yet,
		 * so insert the clflushopt instruction by adding the 0x66
		 * prefix byte to clflush.
		 */
		__asm__ volatile(".byte 0x66; clflush %P0" : "+m" (uptr));
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
	(*Func_flush)(addr, len);
}

/*
 * pmem_fence -- persistent memory store barrier
 */
void
pmem_fence(void)
{
	__builtin_ia32_sfence();
}

/*
 * pmem_persist -- make any cached changes to a range of pmem persistent
 */
void
pmem_persist(void *addr, size_t len)
{
	pmem_flush(addr, len);
	__builtin_ia32_sfence();
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
	LOG(5, "addr %p len %zu", addr, len);

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
 * is_pmem_always -- (internal) always true version of pmem_is_pmem()
 */
static int
is_pmem_always(void *addr, size_t len)
{
	LOG(3, "always true");
	return 1;
}

/*
 * is_pmem_never -- (internal) never true version of pmem_is_pmem()
 */
static int
is_pmem_never(void *addr, size_t len)
{
	LOG(3, "never true");
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
			static const char clflushopt[] = " clflushopt ";

			if (strncmp(flags, line, sizeof (flags) - 1) == 0) {

				/* change ending newline to space delimiter */
				char *nl = strrchr(line, '\n');
				if (nl)
					*nl = ' ';

				if (strstr(&line[sizeof (flags) - 1],
							clflush) != NULL) {
					Func_is_pmem = is_pmem_proc;
					LOG(3, "clflush supported");
				}

				if (strstr(&line[sizeof (flags) - 1],
							clflushopt) != NULL) {
					Func_flush = flush_clflushopt;
					LOG(3, "clflushopt supported");
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

/*
 * pmem_is_pmem -- return true if entire range is persistent Memory
 */
int
pmem_is_pmem(void *addr, size_t len)
{
	LOG(3, "addr %p len %zu", addr, len);

	return (*Func_is_pmem)(addr, len);
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
 * pmem_memmove_nodrain -- memmove to pmem without hw drain
 */
void *
pmem_memmove_nodrain(void *pmemdest, const void *src, size_t len)
{
	/* XXX stub version */
	return memmove(pmemdest, src, len);
}

/*
 * pmem_memcpy_nodrain -- memcpy to pmem without hw drain
 */
void *
pmem_memcpy_nodrain(void *pmemdest, const void *src, size_t len)
{
	return pmem_memmove_nodrain(pmemdest, src, len);
}

/*
 * pmem_memset_nodrain -- memset to pmem without hw drain
 */
void *
pmem_memset_nodrain(void *pmemdest, int c, size_t len)
{
	/* XXX stub version */
	return memset(pmemdest, c, len);
}

/*
 * pmem_memmove -- memmove to pmem
 */
void *
pmem_memmove(void *pmemdest, const void *src, size_t len)
{
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
	void *retval = pmem_memcpy_nodrain(pmemdest, src, len);
	pmem_drain();
	return retval;
}

/*
 * pmem_memset -- memset to pmem
 */
void *
pmem_memset(void *pmemdest, int c, size_t len)
{
	void *retval = pmem_memset_nodrain(pmemdest, c, len);
	pmem_drain();
	return retval;
}
