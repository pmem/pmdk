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
 * util_linux.c -- general utilities with OS-specific implementation
 */

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <link.h>

#include "util.h"
#include "out.h"

#define PROCMAXLEN 2048 /* maximum expected line length in /proc files */

#define MEGABYTE ((uintptr_t)1 << 20)
#define GIGABYTE ((uintptr_t)1 << 30)

extern int Mmap_no_random;
extern void *Mmap_hint;

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
	if ((fp = fopen("/proc/self/maps", "r")) == NULL) {
		ERR("!/proc/self/maps");
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
		if (sscanf(line, "%p-%p", &lo, &hi) == 2) {
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

	char *addr;

	/*
	 * Choose the desired alignment based on the requested length.
	 * Use 2MB/1GB page alignment only if the mapping length is at least
	 * twice as big as the page size.
	 */
	size_t align = Pagesize;
	if (req_align)
		align = req_align;
	else if (len >= 2 * GIGABYTE)
		align = GIGABYTE;
	else if (len >= 4 * MEGABYTE)
		align = 2 * MEGABYTE;

	if (Mmap_no_random) {
		LOG(4, "user-defined hint %p", (void *)Mmap_hint);
		addr = util_map_hint_unused((void *)Mmap_hint, len, align);
	} else {
		/*
		 * Create dummy mapping to find an unused region of given size.
		 * Request for increased size for later address alignment.
		 * Use MAP_PRIVATE with read-only access to simulate
		 * zero cost for overcommit accounting.  Note: MAP_NORESERVE
		 * flag is ignored if overcommit is disabled (mode 2).
		 */
		addr = mmap(NULL, len + align, PROT_READ,
					MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
		if (addr != MAP_FAILED) {
			LOG(4, "system choice %p", addr);
			munmap(addr, len + align);
			addr = (char *)roundup((uintptr_t)addr, align);
		}
	}
	LOG(4, "hint %p", addr);

	return addr;
}

/*
 * util_tmpfile --  (internal) create the temporary file
 */
int
util_tmpfile(const char *dir, const char *templ)
{
	LOG(3, "dir \"%s\" template \"%s\"", dir, templ);

	int oerrno;
	int fd = -1;

	char *fullname = alloca(strlen(dir) + sizeof(templ));

	(void) strcpy(fullname, dir);
	(void) strcat(fullname, templ);

	sigset_t set, oldset;
	sigfillset(&set);
	(void) sigprocmask(SIG_BLOCK, &set, &oldset);

	mode_t prev_umask = umask(S_IRWXG | S_IRWXO);

	fd = mkstemp(fullname);

	umask(prev_umask);

	if (fd < 0) {
		ERR("!mkstemp");
		goto err;
	}

	(void) unlink(fullname);
	(void) sigprocmask(SIG_SETMASK, &oldset, NULL);
	LOG(3, "unlinked file is \"%s\"", fullname);

	return fd;

err:
	oerrno = errno;
	(void) sigprocmask(SIG_SETMASK, &oldset, NULL);
	if (fd != -1)
		(void) close(fd);
	errno = oerrno;
	return -1;
}

/*
 * util_get_arch_flags -- get architecture identification flags
 */
int
util_get_arch_flags(struct arch_flags *arch_flags)
{
	char *path = "/proc/self/exe";
	int fd;
	ElfW(Ehdr) elf;
	int ret = 0;

	memset(arch_flags, 0, sizeof(*arch_flags));

	if ((fd = open(path, O_RDONLY)) < 0) {
		ERR("!open %s", path);
		ret = -1;
		goto out;
	}

	if (read(fd, &elf, sizeof(elf)) != sizeof(elf)) {
		ERR("!read %s", path);
		ret = -1;
		goto out_close;
	}

	if (elf.e_ident[EI_MAG0] != ELFMAG0 ||
	    elf.e_ident[EI_MAG1] != ELFMAG1 ||
	    elf.e_ident[EI_MAG2] != ELFMAG2 ||
	    elf.e_ident[EI_MAG3] != ELFMAG3) {
		ERR("invalid ELF magic");
		ret = -1;
		goto out_close;
	}

	arch_flags->e_machine = elf.e_machine;
	arch_flags->ei_class = elf.e_ident[EI_CLASS];
	arch_flags->ei_data = elf.e_ident[EI_DATA];
	arch_flags->alignment_desc = alignment_desc();

out_close:
	close(fd);
out:
	return ret;
}

/*
 * util_is_absolute_path -- check if the path is an absolute one
 */
int
util_is_absolute_path(const char *path)
{
	LOG(3, "path: %s", path);

	if (path[0] == DIR_SEPARATOR)
		return 1;
	else
		return 0;
}
