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
#include "os.h"
#include "out.h"

/*
 * pmem2_deep_sync_write --  perform write to deep_flush file
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
