// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2024, Intel Corporation */

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
#include "alloc.h"
#include "libpmem2.h"

int Mmap_no_random;
void *Mmap_hint;
static os_rwlock_t Mmap_list_lock;

static PMDK_SORTEDQ_HEAD(map_list_head, map_tracker) Mmap_list =
		PMDK_SORTEDQ_HEAD_INITIALIZER(Mmap_list);

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
			CORE_LOG_WARNING("Invalid PMEM_MMAP_HINT");
		} else if (os_access(OS_MAPFILE, R_OK)) {
			CORE_LOG_WARNING("No /proc, PMEM_MMAP_HINT ignored");
		} else {
			Mmap_hint = (void *)val;
			Mmap_no_random = 1;
			LOG(3, "PMEM_MMAP_HINT set to %p", Mmap_hint);
		}
	}
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
util_map(int fd, os_off_t off, size_t len, int flags, int rdonly,
	size_t req_align, int *map_sync)
{
	LOG(3, "fd %d len %zu flags %d rdonly %d req_align %zu map_sync %p",
			fd, len, flags, rdonly, req_align, map_sync);

	void *base;
	void *addr = util_map_hint(len, req_align);
	if (addr == MAP_FAILED) {
		CORE_LOG_ERROR("cannot find a contiguous region of given size");
		return NULL;
	}

	if (req_align)
		ASSERTeq((uintptr_t)addr % req_align, 0);

	int proto = rdonly ? PROT_READ : PROT_READ|PROT_WRITE;
	base = util_map_sync(addr, len, proto, flags, fd, off, map_sync);
	if (base == MAP_FAILED) {
		ERR_W_ERRNO("mmap %zu bytes", len);
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
		ERR_W_ERRNO("munmap");

	return retval;
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
		ERR_W_ERRNO("mprotect: PROT_READ");

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
		ERR_W_ERRNO("mprotect: PROT_READ|PROT_WRITE");

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
		ERR_W_ERRNO("mprotect: PROT_NONE");

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

	PMDK_SORTEDQ_FOREACH(mt, &Mmap_list, entry) {
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
	if (util_range_find((uintptr_t)addr, len) != NULL) {
		ERR_WO_ERRNO(
		"duplicated persistent memory range; presumably unmapped with munmap() instead of pmem_unmap(): addr %p len %zu",
			addr, len);
		errno = ENOMEM;
		return -1;
	}

	struct map_tracker *mt;
	mt  = Malloc(sizeof(struct map_tracker));
	if (mt == NULL) {
		ERR_W_ERRNO("Malloc");
		return -1;
	}

	mt->base_addr = (uintptr_t)addr;
	mt->end_addr = mt->base_addr + len;
	mt->type = type;
	if (type == PMEM_DEV_DAX) {
		unsigned region_id;
		int ret = util_ddax_region_find(path, &region_id);
		if (ret < 0) {
			ERR_WO_ERRNO("Cannot find DAX device region id");
			return -1;
		}
		mt->region_id = region_id;
	}

	util_rwlock_wrlock(&Mmap_list_lock);

	PMDK_SORTEDQ_INSERT(&Mmap_list, mt, entry, struct map_tracker,
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
	if (addr == end || addr % Mmap_align != 0 || end % Mmap_align != 0) {
		ERR_WO_ERRNO(
		"invalid munmap length, must be non-zero and page aligned");
		return -1;
	}

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
			ERR_W_ERRNO("Malloc");
			goto err;
		}

		mtb->base_addr = mt->base_addr;
		mtb->end_addr = addr;
		mtb->region_id = mt->region_id;
		mtb->type = mt->type;
	}

	if (end < mt->end_addr) {
		/* case #1/3 */
		/* new mapping at the end */
		mte = Malloc(sizeof(struct map_tracker));
		if (mte == NULL) {
			ERR_W_ERRNO("Malloc");
			goto err;
		}

		mte->base_addr = end;
		mte->end_addr = mt->end_addr;
		mte->region_id = mt->region_id;
		mte->type = mt->type;
	}

	PMDK_SORTEDQ_REMOVE(&Mmap_list, mt, entry);

	if (mtb) {
		PMDK_SORTEDQ_INSERT(&Mmap_list, mtb, entry,
				struct map_tracker, util_range_comparer);
	}

	if (mte) {
		PMDK_SORTEDQ_INSERT(&Mmap_list, mte, entry,
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
 * util_range_unregister -- remove a memory range
 * from map tracking list
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

	/*
	 * Changes in the map tracker list must match the underlying behavior.
	 *
	 * $ man 2 mmap:
	 *	The address addr must be a multiple of the page size (but length
	 *	need not be). All pages containing a part of the indicated range
	 *	are unmapped.
	 *
	 * This means that we must align the length to the page size.
	 */
	len = PAGE_ALIGNED_UP_SIZE(len);

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
 * util_range_is_pmem -- return true if entire range
 * is persistent memory
 */
int
util_range_is_pmem(const void *addrp, size_t len)
{
	LOG(10, "addr %p len %zu", addrp, len);

	uintptr_t addr = (uintptr_t)addrp;
	int retval = 1;

	util_rwlock_rdlock(&Mmap_list_lock);

	do {
		struct map_tracker *mt = util_range_find(addr, len);
		if (mt == NULL) {
			LOG(4, "address not found 0x%016" PRIxPTR, addr);
			retval = 0;
			break;
		}

		LOG(10, "range found - begin 0x%016" PRIxPTR
				" end 0x%016" PRIxPTR,
				mt->base_addr, mt->end_addr);

		if (mt->base_addr > addr) {
			LOG(10, "base address doesn't match: "
				"0x%" PRIxPTR " > 0x%" PRIxPTR,
					mt->base_addr, addr);
			retval = 0;
			break;
		}

		uintptr_t map_len = mt->end_addr - addr;
		if (map_len > len)
			map_len = len;
		len -= map_len;
		addr += map_len;
	} while (len > 0);

	util_rwlock_unlock(&Mmap_list_lock);

	return retval;
}
