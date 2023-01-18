/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2023, Intel Corporation */

/*
 * pmemcompat.h -- compatibility layer for libpmem* libraries
 */

#ifndef PMEMCOMPAT_H
#define PMEMCOMPAT_H

/* for backward compatibility */
#ifdef NVML_UTF8_API
#pragma message( "NVML_UTF8_API macro is obsolete, please use PMDK_UTF8_API instead." )
#ifndef PMDK_UTF8_API
#define PMDK_UTF8_API
#endif
#endif

struct iovec {
	void  *iov_base;
	size_t iov_len;
};

typedef int mode_t;

#endif
