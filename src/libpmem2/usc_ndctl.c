// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * usc_ndctl.c -- pmem2 usc function for platforms using ndctl
 */
#include <ndctl/libndctl.h>
#include <ndctl/libdaxctl.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <fcntl.h>

#include "config.h"
#include "file.h"
#include "libpmem2.h"
#include "os.h"
#include "out.h"
#include "pmem2_utils.h"
#include "source.h"

/*
 * usc_match_devdax -- (internal) returns 1 if the devdax matches
 *                         with the given file, 0 if it doesn't match,
 *                         and -1 in case of error.
 */
static int
usc_match_devdax(const os_stat_t *st, const char *devname)
{
	LOG(3, "st %p devname %s", st, devname);

	if (*devname == '\0')
		return 0;

	char path[PATH_MAX];
	os_stat_t stat;

	if (util_snprintf(path, PATH_MAX, "/dev/%s", devname) < 0) {
		ERR("!snprintf");
		return -1;
	}

	if (os_stat(path, &stat)) {
		ERR("!stat %s", path);
		return -1;
	}

	if (st->st_rdev == stat.st_rdev) {
		LOG(4, "found matching device: %s", path);
		return 1;
	}

	LOG(10, "skipping not matching device: %s", path);
	return 0;
}

#define BUFF_LENGTH 64

/*
 * usc_match_fsdax -- (internal) returns 1 if the device matches
 *                         with the given file, 0 if it doesn't match,
 *                         and -1 in case of error.
 */
static int
usc_match_fsdax(const os_stat_t *st, const char *devname)
{
	LOG(3, "st %p devname %s", st, devname);

	if (*devname == '\0')
		return 0;

	char path[PATH_MAX];
	char dev_id[BUFF_LENGTH];

	if (util_snprintf(path, PATH_MAX, "/sys/block/%s/dev", devname) < 0) {
		ERR("!snprintf");
		return -1;
	}

	if (util_snprintf(dev_id, BUFF_LENGTH, "%d:%d",
			major(st->st_dev), minor(st->st_dev)) < 0) {
		ERR("!snprintf");
		return -1;
	}

	int fd = os_open(path, O_RDONLY);
	if (fd < 0) {
		ERR("!open \"%s\"", path);
		return -1;
	}

	char buff[BUFF_LENGTH];
	ssize_t nread = read(fd, buff, BUFF_LENGTH);
	if (nread < 0) {
		ERR("!read");
		os_close(fd);
		return -1;
	}

	os_close(fd);

	if (nread == 0) {
		ERR("%s is empty", path);
		return -1;
	}

	if (buff[nread - 1] != '\n') {
		ERR("%s doesn't end with new line", path);
		return -1;
	}

	buff[nread - 1] = '\0';

	if (strcmp(buff, dev_id) == 0) {
		LOG(4, "found matching device: %s", path);
		return 1;
	}

	LOG(10, "skipping not matching device: %s", path);
	return 0;
}

#define FOREACH_BUS_REGION_NAMESPACE(ctx, bus, region, ndns)	\
	ndctl_bus_foreach(ctx, bus)				\
	ndctl_region_foreach(bus, region)			\
	ndctl_namespace_foreach(region, ndns)			\

/*
 * usc_region_namespace -- (internal) returns the region
 *                             (and optionally the namespace)
 *                             where the given file is located
 */
static int
usc_region_namespace(struct ndctl_ctx *ctx, const os_stat_t *st,
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
				return -1;
			}
			struct daxctl_dev *dev;
			daxctl_dev_foreach(dax_region, dev) {
				devname = daxctl_dev_get_devname(dev);
				int ret = usc_match_devdax(st, devname);
				if (ret < 0)
					return ret;

				if (ret) {
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

			int ret = usc_match_fsdax(st, devname);
			if (ret < 0)
				return ret;

			if (ret) {
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
 * usc_interleave_set -- (internal) returns set of dimms
 *                           where the pool file is located
 */
static struct ndctl_interleave_set *
usc_interleave_set(struct ndctl_ctx *ctx, const os_stat_t *st)
{
	LOG(3, "ctx %p stat %p", ctx, st);

	struct ndctl_region *region = NULL;

	if (usc_region_namespace(ctx, st, &region, NULL))
		return NULL;

	return region ? ndctl_region_get_interleave_set(region) : NULL;
}

int
pmem2_source_device_usc(const struct pmem2_source *src, uint64_t *usc)
{
	LOG(3, "fd %d, uid %p", src->fd, usc);

	os_stat_t st;
	struct ndctl_ctx *ctx;
	int ret = -1;
	*usc = 0;

	if (os_fstat(src->fd, &st)) {
		ERR("!stat %d", src->fd);
		return PMEM2_E_ERRNO;
	}

	if (ndctl_new(&ctx)) {
		ERR("!ndctl_new");
		return PMEM2_E_ERRNO;
	}

	struct ndctl_interleave_set *iset =
		usc_interleave_set(ctx, &st);

	if (iset == NULL)
		goto out;

	struct ndctl_dimm *dimm;

	ndctl_dimm_foreach_in_interleave_set(iset, dimm) {
		long long dimm_usc = ndctl_dimm_get_dirty_shutdown(dimm);
		if (dimm_usc < 0) {
			ERR("!ndctl_dimm_get_dirty_shutdown");
			ret = PMEM2_E_ERRNO;
			goto err;
		}
		*usc += (unsigned long long)dimm_usc;
	}
out:
	ret = 0;
err:
	ndctl_unref(ctx);
	return ret;
}

int
pmem2_source_device_id(const struct pmem2_source *src, char *id, size_t *len)
{
	os_stat_t st;

	struct ndctl_ctx *ctx;
	struct ndctl_interleave_set *set;
	struct ndctl_dimm *dimm;
	int ret = 0;

	if (os_fstat(src->fd, &st)) {
		ERR("!stat %d", src->fd);
		return PMEM2_E_ERRNO;
	}

	if (ndctl_new(&ctx)) {
		ERR("!ndctl_new");
		return PMEM2_E_ERRNO;
	}

	if (id == NULL) {
		*len = 1; /* '\0' */
	}

	set = usc_interleave_set(ctx, &st);
	if (set == NULL)
		goto end;

	if (id == NULL) {
		ndctl_dimm_foreach_in_interleave_set(set, dimm) {
			*len += strlen(ndctl_dimm_get_unique_id(dimm));
		}
		goto end;
	}

	size_t count = 1;
	ndctl_dimm_foreach_in_interleave_set(set, dimm) {
		const char *dimm_uid = ndctl_dimm_get_unique_id(dimm);
		count += strlen(dimm_uid);
		if (count > *len) {
			ret = PMEM2_E_BUFFER_TOO_SMALL;
			goto end;
		}
		strncat(id, dimm_uid, *len);
	}
end:
	ndctl_unref(ctx);
	return ret;
}
