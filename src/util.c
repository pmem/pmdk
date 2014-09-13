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
 * util.c -- general utilities used in the library
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <unistd.h>
#include <stdint.h>
#include <endian.h>
#include <errno.h>
#include "util.h"
#include "out.h"

#define	PROCMAXLEN 2048 /* maximum expected line length in /proc files */

#define	GIGABYTE ((uintptr_t)1 << 30)
#define	TERABYTE ((uintptr_t)1 << 40)

/* library-wide page size */
unsigned long Pagesize;

/*
 * our versions of malloc & friends start off pointing to the libc versions
 */
Malloc_func Malloc = malloc;
Free_func Free = free;
Realloc_func Realloc = realloc;
Strdup_func Strdup = strdup;

/*
 * util_init -- initialize the utils
 *
 * This is called from the library initialization code.
 */
void
util_init(void)
{
	LOG(3, NULL);
	if (Pagesize == 0)
		Pagesize = (unsigned long) sysconf(_SC_PAGESIZE);
}

/*
 * util_set_alloc_funcs -- allow one to override malloc, etc.
 */
void
util_set_alloc_funcs(void *(*malloc_func)(size_t size),
		void (*free_func)(void *ptr),
		void *(*realloc_func)(void *ptr, size_t size),
		char *(*strdup_func)(const char *s))
{
	LOG(3, "malloc %p free %p realloc %p strdup %p",
			malloc_func, free_func, realloc_func, strdup_func);

	Malloc = (malloc_func == NULL) ? malloc : malloc_func;
	Free = (free_func == NULL) ? free : free_func;
	Realloc = (realloc_func == NULL) ? realloc : realloc_func;
	Strdup = (strdup_func == NULL) ? strdup : strdup_func;
}

/*
 * util_map_hint -- (internal) use /proc to determine a hint address for mmap()
 *
 * This is a helper function for util_map().  It opens up /proc/self/maps
 * and looks for the first unused address in the process address space that is:
 * - greater or equal 1TB,
 * - large enough to hold range of given length,
 * - 1GB aligned.
 *
 * Asking for aligned address like this will allow the DAX code to use large
 * mappings.  It is not an error if mmap() ignores the hint and chooses
 * different address.
 */
static char *
util_map_hint(size_t len)
{
	FILE *fp;
	if ((fp = fopen("/proc/self/maps", "r")) == NULL) {
		LOG(1, "!/proc/self/maps");
		return NULL;
	}

	char line[PROCMAXLEN];	/* for fgets() */
	char *lo = NULL;	/* beginning of current range in maps file */
	char *hi = NULL;	/* end of current range in maps file */
	char *raddr = (char *)TERABYTE;	/* ignore regions below 1TB */

	while (fgets(line, PROCMAXLEN, fp) != NULL) {
		/* check for range line */
		if (sscanf(line, "%p-%p", &lo, &hi) == 2) {
			LOG(4, "%p-%p", lo, hi);
			if (lo > raddr) {
				if (lo - raddr >= len) {
					LOG(4, "unused region of size %zu "
							"found at %p",
							lo - raddr, raddr);
					break;
				} else {
					LOG(4, "region is too small: %zu < %zu",
							lo - raddr, len);
				}
			}

			if (hi > raddr) {
				/* align to 1GB */
				raddr = (char *)roundup((uintptr_t)hi,
						GIGABYTE);
				LOG(4, "nearest aligned addr %p", raddr);
			}

			if (raddr == 0) {
				LOG(4, "end of address space reached");
				break;
			}
		}
	}

	/*
	 * Check for a case when this is the last unused range in the address
	 * space, but is not large enough. (very unlikely)
	 */
	if ((raddr != NULL) && (UINTPTR_MAX - (uintptr_t)raddr < len)) {
		LOG(4, "end of address space reached");
		raddr = NULL;
	}

	fclose(fp);

	LOG(3, "returning %p", raddr);
	return raddr;
}

/*
 * util_map -- memory map a file
 *
 * This is just a convenience function that calls mmap() with the
 * appropriate arguments and includes our trace points.
 *
 * If cow is set, the file is mapped copy-on-write.
 */
void *
util_map(int fd, size_t len, int cow)
{
	void *base;

	LOG(3, "fd %d len %zu cow %d", fd, len, cow);

	void *addr = util_map_hint(len);

	if ((base = mmap(addr, len, PROT_READ|PROT_WRITE,
			(cow) ? MAP_PRIVATE|MAP_NORESERVE : MAP_SHARED,
					fd, 0)) == MAP_FAILED) {
		LOG(1, "!mmap %zu bytes", len);
		return NULL;
	}

	LOG(3, "mapped at %p", base);

	return base;
}

/*
 * util_unmap -- unmap a file
 *
 * This is just a convenience function that calls munmap() with the
 * appropriate arguments and includes our trace points.
 */
int
util_unmap(void *addr, size_t len)
{
	LOG(3, "addr %p len %zu", addr, len);

	int retval = munmap(addr, len);

	if (retval < 0)
		LOG(1, "!munmap");

	return retval;
}

/*
 * util_checksum -- compute Fletcher64 checksum
 *
 * csump points to where the checksum lives, so that location
 * is treated as zeros while calculating the checksum.  If
 * insert is true, the calculated checksum is inserted into
 * the range at *csump.  Otherwise the calculated checksum is
 * checked against *csump and the result returned (true means
 * the range checksummed correctly).
 */
int
util_checksum(void *addr, size_t len, uint64_t *csump, int insert)
{
	uint32_t *p32 = addr;
	uint32_t *p32end = addr + len;
	uint32_t lo32 = 0;
	uint32_t hi32 = 0;
	uint64_t csum;

	while (p32 < p32end)
		if (p32 == (uint32_t *)csump) {
			/* lo32 += 0; treat first 32-bits as zero */
			p32++;
			hi32 += lo32;
			/* lo32 += 0; treat second 32-bits as zero */
			p32++;
			hi32 += lo32;
		} else {
			lo32 += *p32++;
			hi32 += lo32;
		}

	csum = (uint64_t)hi32 << 32 | lo32;

	if (insert) {
		*csump = csum;
		return 1;
	}

	return *csump == csum;
}

/*
 * util_convert_hdr -- convert header to host byte order & validate
 *
 * Returns true if header is valid, and all the integer fields are
 * converted to host byte order.  If the header is not valid, this
 * routine returns false and the header passed in is left in an
 * unknown state.
 */
int
util_convert_hdr(struct pool_hdr *hdrp)
{
	LOG(3, "hdrp %p", hdrp);

	/* to be valid, a header must have a major version of at least 1 */
	if ((hdrp->major = le32toh(hdrp->major)) == 0) {
		LOG(3, "invalid major version (0)");
		return 0;
	}
	hdrp->compat_features = le32toh(hdrp->compat_features);
	hdrp->incompat_features = le32toh(hdrp->incompat_features);
	hdrp->ro_compat_features = le32toh(hdrp->ro_compat_features);
	hdrp->crtime = le64toh(hdrp->crtime);
	hdrp->checksum = le64toh(hdrp->checksum);

	/* and to be valid, the fields must checksum correctly */
	if (!util_checksum(hdrp, sizeof (*hdrp), &hdrp->checksum, 0)) {
		LOG(3, "invalid checksum");
		return 0;
	}

	LOG(3, "valid header, signature \"%s\"", hdrp->signature);
	return 1;
}

/*
 * util_range_ro -- set a memory range read-only
 */
int
util_range_ro(void *addr, size_t len)
{
	LOG(3, "addr %p len %zu", addr, len);

	uintptr_t uptr;
	int retval;

	/*
	 * mprotect requires addr to be a multiple of pagesize, so
	 * adjust addr and len to represent the full 4k chunks
	 * covering the given range.
	 */

	/* increase len by the amount we gain when we round addr down */
	len += (uintptr_t)addr & (Pagesize - 1);

	/* round addr down to page boundary */
	uptr = (uintptr_t)addr & ~(Pagesize - 1);

	if ((retval = mprotect((void *)uptr, len, PROT_READ)) < 0)
		LOG(1, "!mprotect: PROT_READ");

	return retval;
}

/*
 * util_range_rw -- set a memory range read-write
 */
int
util_range_rw(void *addr, size_t len)
{
	LOG(3, "addr %p len %zu", addr, len);

	uintptr_t uptr;
	int retval;

	/*
	 * mprotect requires addr to be a multiple of pagesize, so
	 * adjust addr and len to represent the full 4k chunks
	 * covering the given range.
	 */

	/* increase len by the amount we gain when we round addr down */
	len += (uintptr_t)addr & (Pagesize - 1);

	/* round addr down to page boundary */
	uptr = (uintptr_t)addr & ~(Pagesize - 1);

	if ((retval = mprotect((void *)uptr, len, PROT_READ|PROT_WRITE)) < 0)
		LOG(1, "!mprotect: PROT_READ|PROT_WRITE");

	return retval;
}

/*
 * util_range_none -- set a memory range for no access allowed
 */
int
util_range_none(void *addr, size_t len)
{
	LOG(3, "addr %p len %zu", addr, len);

	uintptr_t uptr;
	int retval;

	/*
	 * mprotect requires addr to be a multiple of pagesize, so
	 * adjust addr and len to represent the full 4k chunks
	 * covering the given range.
	 */

	/* increase len by the amount we gain when we round addr down */
	len += (uintptr_t)addr & (Pagesize - 1);

	/* round addr down to page boundary */
	uptr = (uintptr_t)addr & ~(Pagesize - 1);

	if ((retval = mprotect((void *)uptr, len, PROT_NONE)) < 0)
		LOG(1, "!mprotect: PROT_NONE");

	return retval;
}

/*
 * util_feature_check -- check features masks
 */
int
util_feature_check(struct pool_hdr *hdrp, uint32_t incompat,
			uint32_t ro_compat, uint32_t compat)
{
	LOG(3, "hdrp %p incompat %#x ro_compat %#x compat %#x",
			hdrp, incompat, ro_compat, compat);

#define	GET_NOT_MASKED_BITS(x, mask) ((x) & ~(mask))

	uint32_t ubits;	/* unsupported bits */

	/* check incompatible ("must support") features */
	ubits = GET_NOT_MASKED_BITS(hdrp->incompat_features, incompat);
	if (ubits) {
		LOG(1, "unsafe to continue due to unknown incompat "\
							"features: %#x", ubits);
		errno = EINVAL;
		return -1;
	}

	/* check RO-compatible features (force RO if unsupported) */
	ubits = GET_NOT_MASKED_BITS(hdrp->ro_compat_features, ro_compat);
	if (ubits) {
		LOG(1, "switching to read-only mode due to unknown ro_compat "\
							"features: %#x", ubits);
		return 0;
	}

	/* check compatible ("may") features */
	ubits = GET_NOT_MASKED_BITS(hdrp->compat_features, compat);
	if (ubits) {
		LOG(3, "ignoring unknown compat features: %#x", ubits);
	}

#undef	GET_NOT_MASKED_BITS

	return 1;
}
