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
#include "region_namespace_ndctl.h"

/*
 * usc_interleave_set -- (internal) returns set of dimms
 *                       where the pool file is located
 */
static struct ndctl_interleave_set *
usc_interleave_set(struct ndctl_ctx *ctx, const os_stat_t *st)
{
	LOG(3, "ctx %p stat %p", ctx, st);

	struct ndctl_region *region = NULL;

	if (pmem2_region_namespace(ctx, st, &region, NULL))
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
