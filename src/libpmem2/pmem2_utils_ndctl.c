// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2024, Intel Corporation */

#include <errno.h>
#include <ndctl/libndctl.h>

#include "libpmem2.h"
#include "out.h"
#include "pmem2_utils.h"
#include "region_namespace_ndctl.h"
#include "source.h"

/*
 * pmem2_device_dax_alignment -- checks the alignment of a given
 * dax device from given source
 */
int
pmem2_device_dax_alignment(const struct pmem2_source *src, size_t *alignment)
{
	int ret = 0;
	size_t size = 0;
	struct ndctl_ctx *ctx;
	struct ndctl_namespace *ndns;

	errno = ndctl_new(&ctx) * (-1);
	if (errno) {
		ERR_W_ERRNO("ndctl_new");
		return PMEM2_E_ERRNO;
	}

	ret = pmem2_region_namespace(ctx, src, NULL, &ndns);
	if (ret) {
		CORE_LOG_ERROR("getting region and namespace failed");
		goto end;
	}

	struct ndctl_dax *dax = ndctl_namespace_get_dax(ndns);

	if (dax)
		size = ndctl_dax_get_align(dax);
	else
		ret = PMEM2_E_INVALID_ALIGNMENT_FORMAT;

end:
	ndctl_unref(ctx);

	*alignment = size;
	LOG(4, "device alignment %zu", *alignment);

	return ret;
}

/*
 * pmem2_device_dax_size -- checks the size of a given
 * dax device from given source structure
 */
int
pmem2_device_dax_size(const struct pmem2_source *src, size_t *size)
{
	int ret = 0;
	struct ndctl_ctx *ctx;
	struct ndctl_namespace *ndns;

	errno = ndctl_new(&ctx) * (-1);
	if (errno) {
		ERR_W_ERRNO("ndctl_new");
		return PMEM2_E_ERRNO;
	}

	ret = pmem2_region_namespace(ctx, src, NULL, &ndns);
	if (ret) {
		CORE_LOG_ERROR("getting region and namespace failed");
		goto end;
	}

	struct ndctl_dax *dax = ndctl_namespace_get_dax(ndns);

	if (dax) {
		*size = ndctl_dax_get_size(dax);
	} else {
		ret = PMEM2_E_DAX_REGION_NOT_FOUND;
		ERR_WO_ERRNO(
			"Issue while reading Device Dax size - cannot find dax region");
	}

end:
	ndctl_unref(ctx);
	LOG(4, "device size %zu", *size);

	return ret;
}
