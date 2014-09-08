/*
 * Copyright (c) 2013, Intel Corporation
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "libpmem.h"
#include "pmem.h"
#include "util.h"
#include "out.h"

#define	FLUSH_ALIGN 64

/* default persist function is pmem_persist() */
Persist_func Persist = pmem_persist;

/*
 * pmem_set_persist_func -- allow override of persist_func used libpmem
 */
void
pmem_set_persist_func(void (*persist_func)(void *addr, size_t len, int flags))
{
	LOG(3, "persist %p", persist_func);

	Persist = (persist_func == NULL) ? pmem_persist : persist_func;
}

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
 * pmem_flush -- flush processor cache for the given range
 */
void
pmem_flush(void *addr, size_t len, int flags)
{
	uintptr_t uptr;

	/* loop through 64B-aligned chunks covering the given range */
	for (uptr = (uintptr_t)addr & ~(FLUSH_ALIGN - 1);
			uptr < (uintptr_t)addr + len; uptr += 64)
		__builtin_ia32_clflush((void *)uptr);
}

/*
 * pmem_persist -- make any cached changes to a range of PMEM persistent
 */
void
pmem_persist(void *addr, size_t len, int flags)
{
	pmem_flush(addr, len, flags);
	__builtin_ia32_sfence();
	pmem_drain();
}

/*
 * is_pmem_always -- debug/test version of pmem_is_pmem(), always true
 */
static int
is_pmem_always(void *addr, size_t len)
{
	LOG(3, "always true");
	return 1;
}

/*
 * is_pmem_never -- debug/test version of pmem_is_pmem(), never true
 */
static int
is_pmem_never(void *addr, size_t len)
{
	LOG(3, "never true");
	return 0;
}

/*
 * is_pmem_proc -- use /proc to implement pmem_is_pmem()
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
	char *line = NULL;	/* for getline() */
	size_t linelen;		/* for getline() */
	char *lo = NULL;	/* beginning of current range in smaps file */
	char *hi = NULL;	/* end of current range in smaps file */
	int needmm = 0;		/* looking for mm flag for current range */
	while (getline(&line, &linelen, fp) != -1) {
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

	Free(line);
	fclose(fp);

	LOG(3, "returning %d", retval);
	return retval;
}

/* pmem_is_pmem() calls this function to do the work */
static int (*is_pmem_func)(void *addr, size_t len) = is_pmem_proc;

/*
 * pmem_init -- load-time initialization for pmem.c
 *
 * Called automatically by the run-time loader.
 */
__attribute__((constructor))
static void
pmem_init(void)
{
	out_init(LOG_PREFIX, LOG_LEVEL_VAR, LOG_FILE_VAR);
	LOG(3, NULL);
	util_init();

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
			is_pmem_func = is_pmem_never;
		else if (val == 1)
			is_pmem_func = is_pmem_always;
	}
}

/*
 * pmem_is_pmem -- return true if entire range is persistent Memory
 */
int
pmem_is_pmem(void *addr, size_t len)
{
	LOG(3, "addr %p len %zu", addr, len);

	return (*is_pmem_func)(addr, len);
}
