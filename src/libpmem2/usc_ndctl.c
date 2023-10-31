// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2024, Intel Corporation */

/*
 * usc_ndctl.c -- pmem2 usc function for platforms using ndctl
 */
#include <ndctl/libndctl.h>
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

int
pmem2_source_device_usc(const struct pmem2_source *src, uint64_t *usc)
{
	LOG(3, "type %d, uid %p", src->type, usc);
	PMEM2_ERR_CLR();

	if (src->type == PMEM2_SOURCE_ANON) {
		ERR_WO_ERRNO(
			"Anonymous source does not support unsafe shutdown count");
		return PMEM2_E_NOSUPP;
	}

	ASSERTeq(src->type, PMEM2_SOURCE_FD);

	struct ndctl_ctx *ctx;
	int ret = PMEM2_E_NOSUPP;
	*usc = 0;

	errno = ndctl_new(&ctx) * (-1);
	if (errno) {
		ERR_W_ERRNO("ndctl_new");
		return PMEM2_E_ERRNO;
	}

	struct ndctl_region *region = NULL;
	ret = pmem2_region_namespace(ctx, src, &region, NULL);

	if (ret < 0)
		goto err;

	ret = PMEM2_E_NOSUPP;

	if (region == NULL) {
		ERR_WO_ERRNO(
			"Unsafe shutdown count is not supported for this source");
		goto err;
	}

	struct ndctl_dimm *dimm;

	ndctl_dimm_foreach_in_region(region, dimm) {
		long long dimm_usc = ndctl_dimm_get_dirty_shutdown(dimm);
		if (dimm_usc < 0) {
			ret = PMEM2_E_NOSUPP;
			ERR_WO_ERRNO(
				"Unsafe shutdown count is not supported for this source");
			goto err;
		}
		*usc += (unsigned long long)dimm_usc;
	}

	ret = 0;

err:
	ndctl_unref(ctx);
	return ret;
}

int
pmem2_source_device_id(const struct pmem2_source *src, char *id, size_t *len)
{
	PMEM2_ERR_CLR();

	struct ndctl_ctx *ctx;
	struct ndctl_dimm *dimm;
	int ret;
	struct ndctl_region *region = NULL;
	const char *dimm_uid;

	if (src->type == PMEM2_SOURCE_ANON) {
		ERR_WO_ERRNO("Anonymous source does not have device id");
		return PMEM2_E_NOSUPP;
	}

	ASSERTeq(src->type, PMEM2_SOURCE_FD);

	errno = ndctl_new(&ctx) * (-1);
	if (errno) {
		ERR_W_ERRNO("ndctl_new");
		return PMEM2_E_ERRNO;
	}

	size_t len_base = 1; /* '\0' */

	ret = pmem2_region_namespace(ctx, src, &region, NULL);

	if (ret < 0)
		goto err;

	if (region == NULL) {
		ret = PMEM2_E_NOSUPP;
		goto err;
	}

	if (id == NULL) {
		ndctl_dimm_foreach_in_region(region, dimm) {
			dimm_uid = ndctl_dimm_get_unique_id(dimm);
			if (dimm_uid == NULL) {
				ret = PMEM2_E_NOSUPP;
				goto err;
			}
			len_base += strlen(ndctl_dimm_get_unique_id(dimm));
		}
		goto end;
	}

	size_t count = 1;
	ndctl_dimm_foreach_in_region(region, dimm) {
		dimm_uid = ndctl_dimm_get_unique_id(dimm);
		if (dimm_uid == NULL) {
			ret = PMEM2_E_NOSUPP;
			goto err;
		}
		count += strlen(dimm_uid);
		if (count > *len) {
			ret = PMEM2_E_BUFFER_TOO_SMALL;
			goto err;
		}
		strncat(id, dimm_uid, *len);
	}

end:
	ret = 0;
	if (id == NULL)
		*len = len_base;
err:
	ndctl_unref(ctx);
	return ret;
}
