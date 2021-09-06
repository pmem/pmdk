/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

/*
 * check.h -- internal definitions for logic performing check
 */

#ifndef CHECK_H
#define CHECK_H

#ifdef __cplusplus
extern "C" {
#endif

int check_init(PMEMpoolcheck *ppc);
struct check_status *check_step(PMEMpoolcheck *ppc);
void check_fini(PMEMpoolcheck *ppc);

int check_is_end(struct check_data *data);
struct pmempool_check_status *check_status_get(struct check_status *status);

#ifdef _WIN32
void convert_status_cache(PMEMpoolcheck *ppc, char *buf, size_t size);
#endif

#ifdef __cplusplus
}
#endif

#endif
