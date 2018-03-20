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
 * mmap.c -- mmap utilities
 */

#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "file.h"
#include "queue.h"
#include "mmap.h"
#include "sys_util.h"
#include "os.h"

int Mmap_no_random;
void *Mmap_hint;
os_rwlock_t Mmap_list_lock;

struct map_list_head Mmap_list = SORTEDQ_HEAD_INITIALIZER(Mmap_list);

#if defined(_WIN32) && (NTDDI_VERSION >= NTDDI_WIN10_RS1)
PQVM Func_qvmi = NULL;
#endif

/*
 * util_mmap_init -- initialize the mmap utils
 *
 * This is called from the library initialization code.
 */
void
util_mmap_init(void)
{
	LOG(3, NULL);

	util_rwlock_init(&Mmap_list_lock);

	/*
	 * For testing, allow overriding the default mmap() hint address.
	 * If hint address is defined, it also disables address randomization.
	 */
	char *e = os_getenv("PMEM_MMAP_HINT");
	if (e) {
		char *endp;
		errno = 0;
		unsigned long long val = strtoull(e, &endp, 16);

		if (errno || endp == e) {
			LOG(2, "Invalid PMEM_MMAP_HINT");
		} else if (os_access(OS_MAPFILE, R_OK)) {
			LOG(2, "No /proc, PMEM_MMAP_HINT ignored");
		} else {
			Mmap_hint = (void *)val;
			Mmap_no_random = 1;
			LOG(3, "PMEM_MMAP_HINT set to %p", Mmap_hint);
		}
	}

#if defined(_WIN32) && (NTDDI_VERSION >= NTDDI_WIN10_RS1)
	Func_qvmi = (PQVM)GetProcAddress(
			GetModuleHandle(TEXT("KernelBase.dll")),
			"QueryVirtualMemoryInformation");
#endif

}

/*
 * util_mmap_fini -- clean up the mmap utils
 *
 * This is called before process stop.
 */
void
util_mmap_fini(void)
{
	LOG(3, NULL);

	util_rwlock_destroy(&Mmap_list_lock);
}

/*
 * util_map -- memory map a file
 *
 * This is just a convenience function that calls mmap() with the
 * appropriate arguments and includes our trace points.
 */
void *
util_map(int fd, size_t len, int flags, int rdonly, size_t req_align,
	enum pmem_map_type *mtype)
{
	LOG(3, "fd %d len %zu flags %d rdonly %d req_align %zu mtype %p",
			fd, len, flags, rdonly, req_align, mtype);

	void *base;
	void *addr = util_map_hint(len, req_align);
	if (addr == MAP_FAILED) {
		ERR("cannot find a contiguous region of given size");
		return NULL;
	}

	if (req_align)
		ASSERTeq((uintptr_t)addr % req_align, 0);

	int proto = rdonly ? PROT_READ : PROT_READ|PROT_WRITE;
	base = util_map_sync(addr, len, proto, flags, fd, 0, mtype);
	if (base == MAP_FAILED) {
		ERR("!mmap %zu bytes", len);
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
		ERR("!munmap");

	return retval;
}

/*
 * util_map_tmpfile -- reserve space in an unlinked file and memory-map it
 *
 * size must be multiple of page size.
 */
void *
util_map_tmpfile(const char *dir, size_t size, size_t req_align)
{
	int oerrno;

	if (((os_off_t)size) < 0) {
		ERR("invalid size (%zu) for os_off_t", size);
		errno = EFBIG;
		return NULL;
	}

	int fd = util_tmpfile(dir, OS_DIR_SEP_STR "vmem.XXXXXX", O_EXCL);
	if (fd == -1) {
		LOG(2, "cannot create temporary file in dir %s", dir);
		goto err;
	}

	if ((errno = os_posix_fallocate(fd, 0, (os_off_t)size)) != 0) {
		ERR("!posix_fallocate");
		goto err;
	}

	void *base;
	if ((base = util_map(fd, size, MAP_SHARED,
			0, req_align, NULL)) == NULL) {
		LOG(2, "cannot mmap temporary file");
		goto err;
	}

	(void) os_close(fd);
	return base;

err:
	oerrno = errno;
	if (fd != -1)
		(void) os_close(fd);
	errno = oerrno;
	return NULL;
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
		ERR("!mprotect: PROT_READ");

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
		ERR("!mprotect: PROT_READ|PROT_WRITE");

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
		ERR("!mprotect: PROT_NONE");

	return retval;
}

/*
 * util_range_comparer -- (internal) compares the two mapping trackers
 */
static intptr_t
util_range_comparer(struct map_tracker *a, struct map_tracker *b)
{
	return ((intptr_t)a->base_addr - (intptr_t)b->base_addr);
}

/*
 * util_range_find_unlocked -- (internal) find the map tracker
 * for given address range
 *
 * Returns the first entry at least partially overlapping given range.
 * It's up to the caller to check whether the entry exactly matches the range,
 * or if the range spans multiple entries.
 */
static struct map_tracker *
util_range_find_unlocked(uintptr_t addr, size_t len)
{
	LOG(10, "addr 0x%016" PRIxPTR " len %zu", addr, len);

	uintptr_t end = addr + len;

	struct map_tracker *mt;

	SORTEDQ_FOREACH(mt, &Mmap_list, entry) {
		if (addr < mt->end_addr &&
		    (addr >= mt->base_addr || end > mt->base_addr))
			goto out;

		/* break if there is no chance to find matching entry */
		if (addr < mt->base_addr)
			break;
	}
	mt = NULL;

out:
	return mt;
}

/*
 * util_range_find -- find the map tracker for given address range
 * the same as util_range_find_unlocked but locked
 */
struct map_tracker *
util_range_find(uintptr_t addr, size_t len)
{
	LOG(10, "addr 0x%016" PRIxPTR " len %zu", addr, len);

	util_rwlock_rdlock(&Mmap_list_lock);

	struct map_tracker *mt = util_range_find_unlocked(addr, len);

	util_rwlock_unlock(&Mmap_list_lock);
	return mt;
}

/*
 * util_range_register -- add a memory range into a map tracking list
 */
int
util_range_register(const void *addr, size_t len, const char *path,
	enum pmem_map_type type)
{
	LOG(3, "addr %p len %zu path %s type %d", addr, len, path, type);

	/* check if not tracked already */
	ASSERTeq(util_range_find((uintptr_t)addr, len), NULL);

	struct map_tracker *mt;
	mt  = Zalloc(sizeof(struct map_tracker));
	if (mt == NULL) {
		ERR("!Zalloc");
		return -1;
	}

	mt->base_addr = (uintptr_t)addr;
	mt->end_addr = mt->base_addr + len;
	mt->type = type;
	if (type == PMEM_DEV_DAX)
		mt->region_id = util_ddax_region_find(path);

	util_rwlock_wrlock(&Mmap_list_lock);

	SORTEDQ_INSERT(&Mmap_list, mt, entry, struct map_tracker,
			util_range_comparer);

	util_rwlock_unlock(&Mmap_list_lock);

	return 0;
}

/*
 * util_range_split -- (internal) remove or split a map tracking entry
 */
static int
util_range_split(struct map_tracker *mt, const void *addrp, const void *endp)
{
	LOG(3, "begin %p end %p", addrp, endp);

	uintptr_t addr = (uintptr_t)addrp;
	uintptr_t end = (uintptr_t)endp;
	ASSERTne(mt, NULL);
	ASSERTeq(addr % Mmap_align, 0);
	ASSERTeq(end % Mmap_align, 0);

	struct map_tracker *mtb = NULL;
	struct map_tracker *mte = NULL;

	/*
	 * 1)    b    e           b     e
	 *    xxxxxxxxxxxxx => xxx.......xxxx  -  mtb+mte
	 * 2)       b     e           b     e
	 *    xxxxxxxxxxxxx => xxxxxxx.......  -  mtb
	 * 3) b     e          b      e
	 *    xxxxxxxxxxxxx => ........xxxxxx  -  mte
	 * 4) b           e    b            e
	 *    xxxxxxxxxxxxx => ..............  -  <none>
	 */

	if (addr > mt->base_addr) {
		/* case #1/2 */
		/* new mapping at the beginning */
		mtb = Malloc(sizeof(struct map_tracker));
		if (mtb == NULL) {
			ERR("!Malloc");
			goto err;
		}

		mtb->base_addr = mt->base_addr;
		mtb->end_addr = (uintptr_t)addr;
		mtb->region_id = mt->region_id;
		mtb->type = mt->type;
	}

	if (end < mt->end_addr) {
		/* case #1/3 */
		/* new mapping at the end */
		mte = Malloc(sizeof(struct map_tracker));
		if (mte == NULL) {
			ERR("!Malloc");
			goto err;
		}

		mte->base_addr = end;
		mte->end_addr = mt->end_addr;
		mte->region_id = mt->region_id;
		mte->type = mt->type;
	}

	SORTEDQ_REMOVE(&Mmap_list, mt, entry);

	if (mtb) {
		SORTEDQ_INSERT(&Mmap_list, mtb, entry,
				struct map_tracker, util_range_comparer);
	}

	if (mte) {
		SORTEDQ_INSERT(&Mmap_list, mte, entry,
				struct map_tracker, util_range_comparer);
	}

	/* free entry for the original mapping */
	Free(mt);
	return 0;

err:
	Free(mtb);
	Free(mte);
	return -1;
}

/*
 * util_range_unregister -- remove a memory range from map tracking list
 *
 * Remove the region between [begin,end].  If it's in a middle of the existing
 * mapping, it results in two new map trackers.
 */
int
util_range_unregister(const void *addr, size_t len)
{
	LOG(3, "addr %p len %zu", addr, len);

	int ret = 0;

	util_rwlock_wrlock(&Mmap_list_lock);

	void *end = (char *)addr + len;

	/* XXX optimize the loop */
	struct map_tracker *mt;
	while ((mt = util_range_find_unlocked((uintptr_t)addr, len)) != NULL) {
		if (util_range_split(mt, addr, end) != 0) {
			ret = -1;
			break;
		}
	}

	util_rwlock_unlock(&Mmap_list_lock);
	return ret;
}

/*
 * util_range_is_pmem -- return true if entire range is persistent memory
 */
int
util_range_is_pmem(const void *addr, size_t len)
{
	LOG(10, "addr %p len %zu", addr, len);

	if (len > UINTPTR_MAX - (uintptr_t)addr) {
		len = UINTPTR_MAX - (uintptr_t)addr;
		LOG(4, "limit len to %zu to not get beyond address space", len);
	}

	uintptr_t begin = (uintptr_t)addr;
	uintptr_t end = (uintptr_t)addr + len;

	LOG(4, "begin 0x%016" PRIxPTR " end 0x%016" PRIxPTR, begin, end);

	int retval = -1;

	util_rwlock_rdlock(&Mmap_list_lock);

	struct map_tracker *mt;
	SORTEDQ_FOREACH(mt, &Mmap_list, entry) {
		if (mt->end_addr <= begin) {
			LOG(4, "skipping all mapped ranges before given range");
			continue;
		}
		if (mt->base_addr >= end) {
			LOG(4, "ignoring all mapped ranges beyond given range");
			break;
		}

		if (begin < mt->base_addr) {
			LOG(4, "untracked address");
			retval = 0;
			break;
		}

#ifdef _WIN32
		/*
		 * This is only needed for Windows, where all the mappings
		 * are tracked. On Linux, only DAX mappings are on the list.
		 */
		if (mt->type != PMEM_MAP_DAX) {
			LOG(4, "tracked range [%p, %p) is not direct mapped",
				mt->base_addr, mt->end_addr);
			retval = 0;
			break;
		}

		/*
		 * If there is a gap between the given region that we process
		 * currently and the mapped region in our tracking list, we
		 * need to process the gap by taking the long route of asking
		 * MM for each page in that range.
		 */
		if (begin < mt->base_addr &&
		    !util_is_direct_mapped(begin, mt->base_addr)) {
			LOG(4, "untracked range [%p, %p) is not direct mapped",
				begin, mt->base_addr);
			retval = 0;
			break;
		}
#endif

		retval = 1;

		/* push our begin to reflect what we have already processed */
		begin = mt->end_addr;
	}

#ifdef _WIN32
	/*
	 * If we still have a range to verify, check with MM if the entire
	 * region is direct mapped.
	 */
	if (begin < end && !util_is_direct_mapped(begin, end)) {
		LOG(4, "untracked end range [%p, %p) is not direct mapped",
			begin, end);
		retval = 0;
	}
#else
	/*
	 * If there is something left, treat it as non-PMEM.
	 */
	if (begin < end)
		retval = 0;
#endif

	util_rwlock_unlock(&Mmap_list_lock);

	/* if range was not found on the mapping list, assume it's not pmem */
	if (retval == -1)
		return 0;

	return retval;
}
