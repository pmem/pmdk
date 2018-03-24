/*
 * Copyright 2014-2018, Intel Corporation
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
 * mmap_posix.c -- memory-mapped files for Posix
 */

#include <stdio.h>
#include <sys/mman.h>
#include <sys/param.h>
#include "mmap.h"
#include "out.h"
#include "os.h"

#define PROCMAXLEN 2048 /* maximum expected line length in /proc files */

char *Mmap_mapfile = OS_MAPFILE; /* Should be modified only for testing */

#ifdef __FreeBSD__
static const char *sscanf_os = "%p %p";
#else
static const char *sscanf_os = "%p-%p";
#endif

/*
 * util_map_hint_unused -- use /proc to determine a hint address for mmap()
 *
 * This is a helper function for util_map_hint().
 * It opens up /proc/self/maps and looks for the first unused address
 * in the process address space that is:
 * - greater or equal 'minaddr' argument,
 * - large enough to hold range of given length,
 * - aligned to the specified unit.
 *
 * Asking for aligned address like this will allow the DAX code to use large
 * mappings.  It is not an error if mmap() ignores the hint and chooses
 * different address.
 */
char *
util_map_hint_unused(void *minaddr, size_t len, size_t align)
{
	LOG(3, "minaddr %p len %zu align %zu", minaddr, len, align);
	ASSERT(align > 0);

	FILE *fp;
	if ((fp = os_fopen(Mmap_mapfile, "r")) == NULL) {
		ERR("!%s", Mmap_mapfile);
		return MAP_FAILED;
	}

	char line[PROCMAXLEN];	/* for fgets() */
	char *lo = NULL;	/* beginning of current range in maps file */
	char *hi = NULL;	/* end of current range in maps file */
	char *raddr = minaddr;	/* ignore regions below 'minaddr' */

	if (raddr == NULL)
		raddr += Pagesize;

	raddr = (char *)roundup((uintptr_t)raddr, align);

	while (fgets(line, PROCMAXLEN, fp) != NULL) {
		/* check for range line */
		if (sscanf(line, sscanf_os, &lo, &hi) == 2) {
			LOG(4, "%p-%p", lo, hi);
			if (lo > raddr) {
				if ((uintptr_t)(lo - raddr) >= len) {
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
				raddr = (char *)roundup((uintptr_t)hi, align);
				LOG(4, "nearest aligned addr %p", raddr);
			}

			if (raddr == NULL) {
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
		raddr = MAP_FAILED;
	}

	fclose(fp);

	LOG(3, "returning %p", raddr);
	return raddr;
}

/*
 * util_map_hint -- determine hint address for mmap()
 *
 * If PMEM_MMAP_HINT environment variable is not set, we let the system to pick
 * the randomized mapping address.  Otherwise, a user-defined hint address
 * is used.
 *
 * ALSR in 64-bit Linux kernel uses 28-bit of randomness for mmap
 * (bit positions 12-39), which means the base mapping address is randomized
 * within [0..1024GB] range, with 4KB granularity.  Assuming additional
 * 1GB alignment, it results in 1024 possible locations.
 *
 * Configuring the hint address via PMEM_MMAP_HINT environment variable
 * disables address randomization.  In such case, the function will search for
 * the first unused, properly aligned region of given size, above the specified
 * address.
 */
char *
util_map_hint(size_t len, size_t req_align)
{
	LOG(3, "len %zu req_align %zu", len, req_align);

	char *hint_addr = MAP_FAILED;

	/* choose the desired alignment based on the requested length */
	size_t align = util_map_hint_align(len, req_align);

	if (Mmap_no_random) {
		LOG(4, "user-defined hint %p", Mmap_hint);
		hint_addr = util_map_hint_unused(Mmap_hint, len, align);
	} else {
		/*
		 * Create dummy mapping to find an unused region of given size.
		 * Request for increased size for later address alignment.
		 * Use MAP_PRIVATE with read-only access to simulate
		 * zero cost for overcommit accounting.  Note: MAP_NORESERVE
		 * flag is ignored if overcommit is disabled (mode 2).
		 */
		char *addr = mmap(NULL, len + align, PROT_READ,
					MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
		if (addr != MAP_FAILED) {
			LOG(4, "system choice %p", addr);
			hint_addr = (char *)roundup((uintptr_t)addr, align);
			munmap(addr, len + align);
		}
	}
	LOG(4, "hint %p", hint_addr);

	return hint_addr;
}

/*
 * util_map_sync -- memory map given file into memory, if MAP_SHARED flag is
 * provided it attempts to use MAP_SYNC flag. Otherwise it fallbacks to
 * mmap(2).
 */
void *
util_map_sync(void *addr, size_t len, int proto, int flags, int fd,
	os_off_t offset, int *map_sync)
{
	LOG(15, "addr %p len %zu proto %x flags %x fd %d offset %ld "
		"map_sync %p", addr, len, proto, flags, fd, offset, map_sync);

	if (map_sync)
		*map_sync = 0;

	/* if map_sync is NULL do not even try to mmap with MAP_SYNC flag */
	if (!map_sync || flags & MAP_PRIVATE)
		return mmap(addr, len, proto, flags, fd, offset);

	/* MAP_SHARED */
	void *ret = mmap(addr, len, proto,
			flags | MAP_SHARED_VALIDATE | MAP_SYNC,
			fd, offset);
	if (ret != MAP_FAILED) {
		LOG(4, "mmap with MAP_SYNC succeeded");
		*map_sync = 1;
		return ret;
	}

	if (errno == EINVAL || errno == ENOTSUP) {
		LOG(4, "mmap with MAP_SYNC not supported");
		return mmap(addr, len, proto, flags, fd, offset);
	}

	/* other error */
	return MAP_FAILED;
}
