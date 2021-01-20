/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020-2021, Intel Corporation */

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

#ifdef __cplusplus
}
#endif

#endif
