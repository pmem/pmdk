// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2020, Intel Corporation */

/*
 * os_deep_linux.c -- Linux abstraction layer
 */

#define _GNU_SOURCE

#include <inttypes.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "out.h"
#include "os.h"
#include "mmap.h"
#include "file.h"
#include "libpmem.h"
#include "os_deep.h"

/*
 * os_deep_flush_write -- (internal) perform write to deep_flush file
 * on given region_id
 */
static int
os_deep_flush_write(int region_id)
{
	LOG(3, "region_id %d", region_id);

	char deep_flush_path[PATH_MAX];
	int deep_flush_fd;

	if (util_snprintf(deep_flush_path, PATH_MAX,
		"/sys/bus/nd/devices/region%d/deep_flush", region_id) < 0) {
		ERR("!snprintf");
		return -1;
	}

	if ((deep_flush_fd = os_open(deep_flush_path, O_WRONLY)) < 0) {
		LOG(1, "!os_open(\"%s\", O_WRONLY)", deep_flush_path);
		return -1;
	}

	if (write(deep_flush_fd, "1", 1) != 1) {
		LOG(1, "!write(%d, \"1\")", deep_flush_fd);
		int oerrno = errno;
		os_close(deep_flush_fd);
		errno = oerrno;
		return -1;
	}

	os_close(deep_flush_fd);
	return 0;
}

/*
 * os_deep_type -- (internal) perform deep operation based on a pmem
 * mapping type
 */
static int
os_deep_type(const struct map_tracker *mt, void *addr, size_t len)
{
	LOG(15, "mt %p addr %p len %zu", mt, addr, len);

	switch (mt->type) {
	case PMEM_DEV_DAX:
		pmem_drain();

		if (os_deep_flush_write(mt->region_id) < 0) {
			if (errno == ENOENT) {
				errno = ENOTSUP;
				LOG(1, "!deep_flush not supported");
			} else {
				LOG(2, "cannot write to deep_flush"
					"in region %d", mt->region_id);
			}
			return -1;
		}
		return 0;
	case PMEM_MAP_SYNC:
		return pmem_msync(addr, len);
	default:
		ASSERT(0);
		return -1;
	}
}

/*
 * os_range_deep_common -- perform deep action of given address range
 */
int
os_range_deep_common(uintptr_t addr, size_t len)
{
	LOG(3, "addr 0x%016" PRIxPTR " len %zu", addr, len);

	while (len != 0) {
		const struct map_tracker *mt = util_range_find(addr, len);

		/* no more overlapping track regions or NOT a device DAX */
		if (mt == NULL) {
			LOG(15, "pmem_msync addr %p, len %lu",
				(void *)addr, len);
			return pmem_msync((void *)addr, len);
		}
		/*
		 * For range that intersects with the found mapping
		 * write to (Device DAX) deep_flush file.
		 * Call msync for the non-intersecting part.
		 */
		if (mt->base_addr > addr) {
			size_t curr_len = mt->base_addr - addr;
			if (curr_len > len)
				curr_len = len;
			if (pmem_msync((void *)addr, curr_len) != 0)
				return -1;
			len -= curr_len;
			if (len == 0)
				return 0;
			addr = mt->base_addr;
		}
		size_t mt_in_len = mt->end_addr - addr;
		size_t persist_len = MIN(len, mt_in_len);

		if (os_deep_type(mt, (void *)addr, persist_len))
			return -1;

		if (mt->end_addr >= addr + len)
			return 0;

		len -= mt_in_len;
		addr = mt->end_addr;
	}
	return 0;
}

/*
 * os_part_deep_common -- common function to handle both
 * deep_persist and deep_drain part flush cases.
 */
int
os_part_deep_common(struct pool_replica *rep, unsigned partidx, void *addr,
			size_t len, int flush)
{
	LOG(3, "part %p part %d addr %p len %lu flush %d",
		rep, partidx, addr, len, flush);

	if (!rep->is_pmem) {
		/*
		 * In case of part on non-pmem call msync on the range
		 * to deep flush the data. Deep drain is empty as all
		 * data is msynced to persistence.
		 */

		if (!flush)
			return 0;

		if (pmem_msync(addr, len)) {
			LOG(1, "pmem_msync(%p, %lu)", addr, len);
			return -1;
		}
		return 0;
	}
	struct pool_set_part part = rep->part[partidx];
	/* Call deep flush if it was requested */
	if (flush) {
		LOG(15, "pmem_deep_flush addr %p, len %lu", addr, len);
		pmem_deep_flush(addr, len);
	}
	/*
	 * Before deep drain call normal drain to ensure that data
	 * is at least in WPQ.
	 */
	pmem_drain();

	if (part.is_dev_dax) {
		/*
		 * During deep_drain for part on device DAX search for
		 * device region id, and perform WPQ flush on found
		 * device DAX region.
		 */
		int region_id = util_ddax_region_find(part.path);

		if (region_id < 0) {
			if (errno == ENOENT) {
				errno = ENOTSUP;
				LOG(1, "!deep_flush not supported");
			} else {
				LOG(1, "invalid dax_region id %d", region_id);
			}
			return -1;
		}

		if (os_deep_flush_write(region_id)) {
			LOG(1, "ddax_deep_flush_write(%d)",
				region_id);
			return -1;
		}
	} else {
		/*
		 * For deep_drain on normal pmem it is enough to
		 * call msync on one page.
		 */
		if (pmem_msync(addr, MIN(Pagesize, len))) {
			LOG(1, "pmem_msync(%p, %lu)", addr, len);
			return -1;
		}
	}
	return 0;
}
