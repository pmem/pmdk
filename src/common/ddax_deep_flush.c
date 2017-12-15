/*
 * Copyright 2017, Intel Corporation
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
 * ddaxe_deep_flush.c -- Internal utility functions for flushing
 * a memory range residing on a DAX device.
 * Currently only used on Linux.
 */

#define _GNU_SOURCE


#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "os.h"
#include "file.h"
#include "out.h"
#include "ddax_deep_flush.h"
#include "libpmem.h"

#define DAX_REGION_ID_LEN 4

/*
 * ddax_region_find - looks for region number for specified dev_id
 */
int
ddax_region_find(os_dev_t dev_id)
{

	LOG(2, "ddax_region_find %lu", (unsigned long)dev_id);
#ifndef _WIN32
	int dax_reg_id_fd;
	char dax_region_path[PATH_MAX];
	char reg_id[DAX_REGION_ID_LEN + 1];

	snprintf(dax_region_path, PATH_MAX,
		"/sys/dev/char/%u:%u/device/dax_region/id",
		major(dev_id), minor(dev_id));

	if ((dax_reg_id_fd = os_open(dax_region_path, O_RDONLY)) < 0) {
		ERR("open(\"%s\", O_PATH", dax_region_path);
		return -1;
	}

	ssize_t len = pread(dax_reg_id_fd, reg_id, sizeof(int), 0);

	if (len <= 1 || len > DAX_REGION_ID_LEN + 1) {
		ERR("pread(%d, %p, %lu, 0)", dax_reg_id_fd, reg_id,
		sizeof(int));
		goto err;
	}

	os_close(dax_reg_id_fd);
	return atoi(reg_id);

err:
	os_close(dax_reg_id_fd);
	return -1;
#else
	return -1;
#endif
}

/*
 * ddax_deep_flush_select - check if deep flush request is for pmem or other
 */
int
ddax_deep_flush_select(const void *addr, size_t len, struct pool_set *set)
{
	LOG(2, "ddax_deep_flush_select addr %p len %zu set %p", addr, len, set);

	if (set == NULL) {
		if (pmem_deep_flush(addr, len)) {
			ERR("!pmem_deep_flush(%p, %lu)", addr, len);
			return -1;
		}
	} else {
		if (ddax_pool_set_deep_flush(addr, len, set)) {
			ERR("!ddax_pool_set_deep_flush(%p, %lu, %p)",
				addr, len, set);
			return -1;
		}
	}

	return 0;
}

/*
 * ddax_pool_set_deep_flush -- perform deep flush on parts on dev dax from range
 */
int
ddax_pool_set_deep_flush(const void *addr, size_t len, struct pool_set *set)
{
	LOG(2, "ddax_pool_set_deep_flush addr %p len %zu set %p",
		addr, len, set);

	return 0;
}

/*
 * ddax_deep_flush_final -- perform final deep flush on given deep_flush fd
 */
int
ddax_deep_flush_final(int region_id)
{
	LOG(2, "ddax_deep_flush_final %d", region_id);

	char deep_flush_path[PATH_MAX];
	int deep_flush_fd;

	snprintf(deep_flush_path, PATH_MAX,
		"/sys/bus/nd/devices/region%d/deep_flush", region_id);

	if ((deep_flush_fd = os_open(deep_flush_path, O_RDWR)) < 0) {
		ERR("os_open(\"%s\", O_RDWR", deep_flush_path);
		return -1;
	}

	if (write(deep_flush_fd, "1", 1)) {
		int oerrno = errno;
		os_close(deep_flush_fd);
		errno = oerrno;
		return -1;
	}

	os_close(deep_flush_fd);
	return 0;
}
