// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * deep_sync_linux.c -- deeep_sync functionality
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "deep_sync.h"
#include "libpmem2.h"
#include "map.h"
#include "os.h"
#include "out.h"
#include "persist.h"
#include "pmem2_utils.h"

/*
 * pmem2_deep_sync_write -- perform write to deep_flush file
 * on given region_id
 */
int
pmem2_deep_sync_write(int region_id)
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
		ERR("!os_open(\"%s\", O_WRONLY)", deep_flush_path);
		return -1;
	}

	if (write(deep_flush_fd, "1", 1) != 1) {
		ERR("!write(%d, \"1\")", deep_flush_fd);
		int oerrno = errno;
		os_close(deep_flush_fd);
		errno = oerrno;
		return -1;
	}

	os_close(deep_flush_fd);
	return 0;
}

/*
 * pmem2_sync_cacheline -- reads file type for map and check
 * if it is device dax or reg file, depend on file type
 * performs proper sync operation
 */
int
pmem2_sync_cacheline(struct pmem2_map *map, void *ptr, size_t size)
{
	enum pmem2_file_type type;
	int ret = pmem2_get_type_from_stat(map->map_st, &type);
	if (ret)
		return ret;

	if (type == PMEM2_FTYPE_REG) {
		size_t len = MIN(Pagesize, size);
		ret = pmem2_flush_file_buffers_os(map, ptr, len, 1);
		if (ret) {
			LOG(1, "cannot flush buffers addr %p len %zu",
				ptr, len);
			return PMEM2_E_ERRNO;
		}
	}
	if (type == PMEM2_FTYPE_DEVDAX) {
		int region_id = pmem2_device_dax_region_find(map->map_st);
		if (region_id < 0) {
			LOG(1, "cannot find region id for stat %p",
				map->map_st);
			return PMEM2_E_ERRNO;
		}
		if (pmem2_deep_sync_write(region_id)) {
			LOG(1, "cannot write to deep_flush file for region %d",
				region_id);
			return PMEM2_E_ERRNO;
		}
	}

	return 0;
}
