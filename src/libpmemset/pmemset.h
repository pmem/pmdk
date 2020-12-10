/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020, Intel Corporation */

/*
 * pmemset.h -- internal definitions for libpmemset
 */
#ifndef PMEMSET_H
#define PMEMSET_H

#include "libpmemset.h"
#include "part.h"
#ifdef __cplusplus
extern "C" {
#endif

#define PMEMSET_MAJOR_VERSION 0
#define PMEMSET_MINOR_VERSION 0

#define PMEMSET_LOG_PREFIX "libpmemset"
#define PMEMSET_LOG_LEVEL_VAR "PMEMSET_LOG_LEVEL"
#define PMEMSET_LOG_FILE_VAR "PMEMSET_LOG_FILE"

struct pmemset_config *pmemset_get_pmemset_config(struct pmemset *set);

struct pmemset_part_descriptor pmemset_get_previous_part_descriptor(
		struct pmemset *set);

void pmemset_set_previous_part_descriptor(struct pmemset *set, void *addr,
		size_t size);

#ifdef __cplusplus
}
#endif

#endif
