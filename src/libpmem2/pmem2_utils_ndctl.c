// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

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
		ERR("!ndctl_new");
		return PMEM2_E_ERRNO;
	}

	ret = pmem2_region_namespace(ctx, src, NULL, &ndns);
	if (ret) {
		LOG(1, "getting region and namespace failed");
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
