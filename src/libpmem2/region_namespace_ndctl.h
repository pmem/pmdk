/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2017-2023, Intel Corporation */

/*
 * region_namespace_ndctl.h -- internal definitions for libpmem2
 *                             common ndctl functions
 */

#ifndef PMDK_REGION_NAMESPACE_NDCTL_H
#define PMDK_REGION_NAMESPACE_NDCTL_H 1

#include "os.h"

#ifdef __cplusplus
extern "C" {
#endif

int pmem2_region_namespace(struct ndctl_ctx *ctx,
			const struct pmem2_source *src,
			struct ndctl_region **pregion,
			struct ndctl_namespace **pndns);

#ifdef __cplusplus
}
#endif

#endif /* PMDK_REGION_NAMESPACE_NDCTL_H */
