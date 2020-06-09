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
	char rbuf[2];

	if (util_snprintf(deep_flush_path, PATH_MAX,
		"/sys/bus/nd/devices/region%u/deep_flush", region_id) < 0) {
		ERR("!snprintf");
		return PMEM2_E_ERRNO;
	}

	if ((deep_flush_fd = os_open(deep_flush_path, O_RDONLY)) < 0) {
		LOG(1, "!os_open(\"%s\", O_RDONLY)", deep_flush_path);
		return 0;
	}

	if (read(deep_flush_fd, rbuf, sizeof(rbuf)) != 2) {
		LOG(1, "!read(%d)", deep_flush_fd);
		goto end;
	}

	if (rbuf[0] == '0' && rbuf[1] == '\n') {
		LOG(3, "Deep flushing not needed");
		goto end;
	}

	os_close(deep_flush_fd);

	if ((deep_flush_fd = os_open(deep_flush_path, O_WRONLY)) < 0) {
		LOG(1, "Cannot open deep_flush file %s to write",
			deep_flush_path);
		return 0;
	}

	if (write(deep_flush_fd, "1", 1) != 1) {
		LOG(1, "Cannot write to deep_flush file %d", deep_flush_fd);
		goto end;
	}

end:
	os_close(deep_flush_fd);
	return 0;
}

/*
 * pmem2_deep_flush_dax -- reads file type for map and check
 * if it is device dax or reg file, depend on file type
 * performs proper flush operation
 */
int
pmem2_deep_flush_dax(struct pmem2_map *map, void *ptr, size_t size)
{
	int ret;
	enum pmem2_file_type type = map->source.value.ftype;

	if (type == PMEM2_FTYPE_REG) {
		ret = pmem2_flush_file_buffers_os(map, ptr, size, 0);
		if (ret) {
			LOG(1, "cannot flush buffers addr %p len %zu",
					ptr, size);
			return ret;
		}
	} else if (type == PMEM2_FTYPE_DEVDAX) {
		unsigned region_id;
		int ret = pmem2_get_region_id(&map->source, &region_id);
		if (ret < 0) {
			LOG(1, "cannot find region id for dev %lu",
				map->source.value.st_rdev);
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
