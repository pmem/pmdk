/*
 * Copyright 2014-2016, Intel Corporation
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
 * pmem_linux.c -- pmem utilities with OS-specific implementation
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

#include "pmem.h"
#include "out.h"

#define PROCMAXLEN 2048 /* maximum expected line length in /proc files */

enum parse_res {
	RES_ERROR = -1,		/* error when parsing */
	RES_FOUND = 0,		/* range found and has mm flag */
	RES_FOUND_NO_FLAG,	/* range found but no mm flag */
	RES_NOT_FOUND,		/* range not found */
	RES_AGAIN, /* range not found in smaps but found by mincore(2) */
};

/*
 * is_page_mapped -- (internal) checks if specified memory page is mapped
 * using mincore(2)
 *
 * returns 1 if page is mapped, 0 if not and -1 if cannot determine.
 */
static int
is_page_mapped(const void *addr)
{
	ASSERTne(Pagesize, 0);

	/* round addr down to page boundary */
	void *ptr = (void *)((uintptr_t)addr & ~((uintptr_t)Pagesize - 1));
	unsigned char vec;

	int ret = mincore(ptr, Pagesize, &vec);

	if (!ret) {
		/* specified memory range is mapped */
		return 1;
	}

	if (errno == ENOMEM) {
		/* specified memory range is not fully mapped */
		return 0;
	}

	/* error when determining */
	return -1;
}

/*
 * is_pmem_proc_parse -- (internal) parse /proc/self/smaps and check if
 * memory range is from pmem
 */
static enum parse_res
is_pmem_proc_parse(const void **addr, size_t *lenp)
{
	const char *caddr = *addr;
	size_t len = *lenp;

	LOG(4, "addr %p len %zu", caddr, len);

	FILE *fp;
	if ((fp = fopen("/proc/self/smaps", "r")) == NULL) {
		ERR("!/proc/self/smaps");
		return RES_ERROR;
	}

	/* assume 'not found' until proven otherwise */
	enum parse_res res = RES_NOT_FOUND;
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
				res = RES_FOUND_NO_FLAG;
				break;
			} else if (caddr < lo) {
				/* never found the range for caddr */
				break;
			} else if (caddr < hi) {
				/* start address is in this range */
				size_t rangelen = (size_t)(hi - caddr);

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
					sizeof(vmflags) - 1) == 0) {
			if (strstr(&line[sizeof(vmflags) - 1], mm) != NULL) {
				LOG(4, "mm flag found");
				if (len == 0) {
					/* entire range matched */
					res = RES_FOUND;
					break;
				}
				needmm = 0;	/* saw what was needed */
			} else {
				/* mm flag not set for some or all of range */
				LOG(4, "range has no mm flag");
				res = RES_FOUND_NO_FLAG;
				break;
			}
		}
	}

	fclose(fp);

	if (res == RES_NOT_FOUND) {
		int mapped = is_page_mapped(caddr);
		LOG(4, "no match for addr %p mapped %d",
				caddr, mapped);
		if (mapped == 0) /* memory is not mapped */
			res = RES_NOT_FOUND;
		else if (mapped == 1) /* memory is mapped */
			res = RES_AGAIN;
		else
			res = RES_ERROR;
	}

	*addr = caddr;
	*lenp = len;

	LOG(4, "returning %d", res);
	return res;
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
 * the vmflags d the mixed map flag is only true on regular files when
 * DAX is in-use, so it serves the purpose.
 *
 * The range passed in may overlap with multiple entries in the smaps list
 * so this function loops through the smaps entries until the entire range
 * is verified as direct access, or until it is clear the answer is false
 * in which case it stops the loop and returns immediately.
 *
 * NOTE: There is an implementation of workaround for an issue with reading
 * from /proc/self/smaps while other thread is modifying process mappings.
 * It may happen that some mapping can be not visible in /proc/self/smaps
 * even if the mapping exists. The workaround rereads the /proc/self/smaps
 * file if a given memory range (or part of it) is not visible but the
 * mapping exists according to mincore(2).
 */
int
is_pmem_proc(const void *addr, size_t len)
{
	enum parse_res res;
	while ((res = is_pmem_proc_parse(&addr, &len)) == RES_AGAIN)
		;

	int retval = (res == RES_FOUND) ? 1 : 0;

	LOG(3, "returning %d", retval);

	return retval;
}
