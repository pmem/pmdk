// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * region_namespace_ndctl.c -- common ndctl functions
 */

#include <ndctl/libndctl.h>
#include <ndctl/libdaxctl.h>
#include <sys/sysmacros.h>
#include <fcntl.h>

#include "libpmem2.h"
#include "pmem2_utils.h"

#include "region_namespace_ndctl.h"
#include "region_namespace.h"
#include "out.h"

/*
 * ndctl_match_devdax -- (internal) returns 0 if the devdax matches
 *                       with the given file, 1 if it doesn't match,
 *                       and a negative value in case of an error.
 */
static int
ndctl_match_devdax(const os_stat_t *st, const char *devname)
{
	LOG(3, "st %p devname %s", st, devname);

	if (*devname == '\0')
		return 1;

	char path[PATH_MAX];
	os_stat_t stat;

	if (util_snprintf(path, PATH_MAX, "/dev/%s", devname) < 0) {
		ERR("!snprintf");
		return PMEM2_E_ERRNO;
	}

	if (os_stat(path, &stat)) {
		ERR("!stat %s", path);
		return PMEM2_E_ERRNO;
	}

	if (st->st_rdev != stat.st_rdev) {
		LOG(10, "skipping not matching device: %s", path);
		return 1;
	}

	LOG(4, "found matching device: %s", path);

	return 0;
}

#define BUFF_LENGTH 64

/*
 * ndctl_match_fsdax -- (internal) returns 0 if the device matches
 *                      with the given file, 1 if it doesn't match,
 *                      and a negative value in case of an error.
 */
static int
ndctl_match_fsdax(const os_stat_t *st, const char *devname)
{
	LOG(3, "st %p devname %s", st, devname);

	if (*devname == '\0')
		return 1;

	char path[PATH_MAX];
	char dev_id[BUFF_LENGTH];

	if (util_snprintf(path, PATH_MAX, "/sys/block/%s/dev", devname) < 0) {
		ERR("!snprintf");
		return PMEM2_E_ERRNO;
	}

	if (util_snprintf(dev_id, BUFF_LENGTH, "%d:%d",
			major(st->st_dev), minor(st->st_dev)) < 0) {
		ERR("!snprintf");
		return PMEM2_E_ERRNO;
	}

	int fd = os_open(path, O_RDONLY);
	if (fd < 0) {
		ERR("!open \"%s\"", path);
		return PMEM2_E_ERRNO;
	}

	char buff[BUFF_LENGTH];
	ssize_t nread = read(fd, buff, BUFF_LENGTH);
	if (nread < 0) {
		ERR("!read");
		int oerrno = errno; /* save the errno */
		os_close(fd);
		errno = oerrno;
		return PMEM2_E_ERRNO;
	}

	os_close(fd);

	if (nread == 0) {
		ERR("%s is empty", path);
		return PMEM2_E_INVALID_DEV_FORMAT;
	}

	if (buff[nread - 1] != '\n') {
		ERR("%s doesn't end with new line", path);
		return PMEM2_E_INVALID_DEV_FORMAT;
	}

	buff[nread - 1] = '\0';

	if (strcmp(buff, dev_id) != 0) {
		LOG(10, "skipping not matching device: %s", path);
		return 1;
	}

	LOG(4, "found matching device: %s", path);

	return 0;
}

/*
 * pmem2_region_namespace -- returns the region
 *                           (and optionally the namespace)
 *                           where the given file is located
 */
int
pmem2_region_namespace(struct ndctl_ctx *ctx, const os_stat_t *st,
			struct ndctl_region **pregion,
			struct ndctl_namespace **pndns)
{
	LOG(3, "ctx %p stat %p pregion %p pnamespace %p",
		ctx, st, pregion, pndns);

	struct ndctl_bus *bus;
	struct ndctl_region *region;
	struct ndctl_namespace *ndns;

	ASSERTne(pregion, NULL);
	*pregion = NULL;

	if (pndns)
		*pndns = NULL;

	enum pmem2_file_type type;

	int ret = pmem2_get_type_from_stat(st, &type);
	if (ret)
		return ret;

	if (type == PMEM2_FTYPE_DIR) {
		ERR("cannot check region or namespace of a directory");
		return PMEM2_E_INVALID_FILE_TYPE;
	}

	FOREACH_BUS_REGION_NAMESPACE(ctx, bus, region, ndns) {
		struct ndctl_btt *btt;
		struct ndctl_dax *dax = NULL;
		struct ndctl_pfn *pfn;
		const char *devname;

		if ((dax = ndctl_namespace_get_dax(ndns))) {
			if (type == PMEM2_FTYPE_REG)
				continue;
			ASSERTeq(type, PMEM2_FTYPE_DEVDAX);

			struct daxctl_region *dax_region;
			dax_region = ndctl_dax_get_daxctl_region(dax);
			if (!dax_region) {
				ERR("!cannot find dax region");
				return PMEM2_E_DAX_REGION_NOT_FOUND;
			}
			struct daxctl_dev *dev;
			daxctl_dev_foreach(dax_region, dev) {
				devname = daxctl_dev_get_devname(dev);
				int ret = ndctl_match_devdax(st, devname);
				if (ret < 0)
					return ret;

				if (ret == 0) {
					*pregion = region;
					if (pndns)
						*pndns = ndns;

					return 0;
				}
			}

		} else {
			if (type == PMEM2_FTYPE_DEVDAX)
				continue;
			ASSERTeq(type, PMEM2_FTYPE_REG);

			if ((btt = ndctl_namespace_get_btt(ndns))) {
				devname = ndctl_btt_get_block_device(btt);
			} else if ((pfn = ndctl_namespace_get_pfn(ndns))) {
				devname = ndctl_pfn_get_block_device(pfn);
			} else {
				devname =
					ndctl_namespace_get_block_device(ndns);
			}

			int ret = ndctl_match_fsdax(st, devname);
			if (ret < 0)
				return ret;

			if (ret == 0) {
				*pregion = region;
				if (pndns)
					*pndns = ndns;

				return 0;
			}
		}
	}

	LOG(10, "did not found any matching device");

	return 0;
}

/*
 * pmem2_region_get_id -- returns the region id
 */
int
pmem2_get_region_id(const os_stat_t *st, unsigned *region_id)
{
	LOG(3, "st %p region_id %p", st, region_id);

	struct ndctl_region *region;
	struct ndctl_namespace *ndns;
	struct ndctl_ctx *ctx;

	errno = ndctl_new(&ctx) * (-1);
	if (errno) {
		ERR("!ndctl_new");
		return PMEM2_E_ERRNO;
	}

	int rv = pmem2_region_namespace(ctx, st, &region, &ndns);
	if (rv) {
		LOG(1, "getting region and namespace failed");
		goto end;
	}

	if (!region) {
		ERR("unknown region");
		rv = PMEM2_E_DAX_REGION_NOT_FOUND;
		goto end;
	}

	*region_id = ndctl_region_get_id(region);

end:
	ndctl_unref(ctx);
	return rv;
}
