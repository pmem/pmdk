/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2022, Intel Corporation */

/*
 * libpmemblk/async.h -- definitions of libpmemblk entry points for async
 * operations
 *
 * See libpmemblk(7) for details.
 */

#ifndef LIBPMEMBLK_ASYNC_H
#define LIBPMEMBLK_ASYNC_H 1

#ifdef PMEMBLK_USE_MINIASYNC

#include <libpmemblk/base.h>
#include <libminiasync/vdm.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pmemblk_write_async_data {
	PMEMblkpool *pbp;
	void *buf;
	long long blockno;
	struct vdm *vdm;
};

struct pmemblk_write_async_output {
	void *dest;
};

FUTURE(pmemblk_write_async_fut, struct pmemblk_write_async_data,
		struct pmemblk_write_async_output);

struct pmemblk_write_async_fut pmemblk_write_async(PMEMblkpool *pbp, void *buf,
		long long blockno, struct vdm *vdm);
#endif

#ifdef __cplusplus
}
#endif
#endif	/* libpmemblk/async.h */
