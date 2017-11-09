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
 * ddax_range_deep_flush.c -- mmap utilities
 */


#define _GNU_SOURCE

#include <ndctl/libndctl.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ddax_range_deep_flush.h"

static int
acquire_region_fd(int slash_dev, int regs_dir_fd, struct ndctl_ctx *ctx,
			dev_t dev_id)
{
	struct ndctl_bus *bus;
	ndctl_bus_foreach(ctx, bus) {
		struct ndctl_region *region;
		ndctl_region_foreach(bus, region) {
			const char *reg_devname;
			reg_devname = ndctl_region_get_devname(region);
			if (reg_devname == NULL)
				return -1;

			struct ndctl_dax *dax;
			ndctl_dax_foreach(region, dax) {
				const char *devname;

				devname = ndctl_dax_get_devname(dax);
				if (devname == NULL)
					return -1;
				struct stat s;
				if (fstatat(slash_dev, devname, &s, 0) != 0)
					return -1;
				if (s.st_rdev == dev_id)
					return openat(regs_dir_fd, reg_devname,
							O_PATH);
			}
		}
	}

	errno = EINVAL;
	return -1;
}

static int
acquire_deep_flush_fd(dev_t dev_id)
{
	int retval = -1;
	struct ndctl_ctx *ctx = NULL;
	int regs_dir = -1;
	int region_fd = -1;
	int slash_dev = -1;

	if ((slash_dev = open("/dev", O_PATH)) < 0)
		goto done;
	if ((regs_dir = open("/sys/bus/nd/devices", O_PATH)) < 0)
		goto done;
	if (ndctl_new(&ctx) != 0)
		goto done;
	region_fd =
	    acquire_region_fd(slash_dev, regs_dir, ctx, dev_id);
	if (region_fd <= 0)
		retval = openat(region_fd, "deep_flush", O_RDWR);

	int oerrno;
done:
	oerrno = errno;

	if (slash_dev >= 0)
		close(slash_dev);
	if (region_fd >= 0)
		close(region_fd);
	if (regs_dir >= 0)
		close(regs_dir);
	if (ctx != NULL)
		ndctl_unref(ctx);

	errno = oerrno;
	return retval;
}

/*
 * ddax_range_deep_flush -- perform deep flush of given address range,
 * which must be part of dax device.
 */
int
ddax_range_deep_flush(dev_t dev_id)
{
	int deep_flush_fd = acquire_deep_flush_fd(dev_id);
	if (deep_flush_fd < 0)
		return -1;

	if (write(deep_flush_fd, "1", 1)) {
		int oerrno = errno;
		close(deep_flush_fd);
		errno = oerrno;
		return -1;
	}

	close(deep_flush_fd);

	return 0;
}

