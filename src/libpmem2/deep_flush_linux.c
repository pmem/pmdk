// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * deep_flush_linux.c -- deep_flush functionality
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "deep_flush.h"
#include "libpmem2.h"
#include "map.h"
#include "os.h"
#include "out.h"
#include "persist.h"
#include "pmem2_utils.h"
#include "region_namespace.h"

/*
 * pmem2_deep_flush_write -- perform write to deep_flush file
 * on given region_id
 */
int
pmem2_deep_flush_write(unsigned region_id)
{
	LOG(3, "region_id %d", region_id);

	char deep_flush_path[PATH_MAX];
	int deep_flush_fd;

	if (util_snprintf(deep_flush_path, PATH_MAX,
		"/sys/bus/nd/devices/region%u/deep_flush", region_id) < 0) {
		ERR("!snprintf");
		return PMEM2_E_ERRNO;
	}

	if ((deep_flush_fd = os_open(deep_flush_path, O_WRONLY)) < 0) {
		LOG(1, "Cannot open deep_flush file %s", deep_flush_path);
		return 0;
	}

	if (write(deep_flush_fd, "1", 1) != 1)
		LOG(1, "Cannot write to deep_flush file %d", deep_flush_fd);

	os_close(deep_flush_fd);
	return 0;
}

/*
 * pmem2_deep_flush_dax -- reads file type for map and check
 * if it is device dax or reg file, depend on file type
 * performs proper flush operation
 */
int
pmem2_deep_flush_dax(struct pmem2_map *map)
{
	enum pmem2_file_type type;
	int ret = pmem2_get_type_from_stat(&map->src_fd_st, &type);
	if (ret)
		return ret;

	if (type == PMEM2_FTYPE_REG) {
		size_t len = Pagesize;
		ret = pmem2_flush_file_buffers_os(map, map->addr, len, 0);
		if (ret) {
			LOG(1, "cannot flush buffers addr %p len %zu",
				map->addr, len);
			return ret;
		}
	} else if (type == PMEM2_FTYPE_DEVDAX) {
		unsigned region_id;
		int ret = pmem2_get_region_id(&map->src_fd_st,
				&region_id);
		if (ret < 0) {
			LOG(1, "cannot find region id for stat %p",
				&map->src_fd_st);
			return ret;
		}
		ret = pmem2_deep_flush_write(region_id);
		if (ret) {
			LOG(1, "cannot write to deep_flush file for region %d",
				region_id);
			return ret;
		}
	} else {
		ASSERT(0);
	}

	return 0;
}
