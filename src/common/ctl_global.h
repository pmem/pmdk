/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

/*
 * ctl_global.h -- definitions for the global CTL namespace
 */

#ifndef PMDK_CTL_GLOBAL_H
#define PMDK_CTL_GLOBAL_H 1

#ifdef __cplusplus
extern "C" {
#endif

extern void ctl_prefault_register(void);
extern void ctl_sds_register(void);
extern void ctl_fallocate_register(void);
extern void ctl_cow_register(void);

static inline void
ctl_global_register(void)
{
	ctl_prefault_register();
	ctl_sds_register();
	ctl_fallocate_register();
	ctl_cow_register();
}

#ifdef __cplusplus
}
#endif

#endif
