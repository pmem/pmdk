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
#include <libpmemblk/btt_async.h>
#include <libminiasync/vdm.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pmemblk_write_async_data {
	PMEMblkpool *pbp;
	void *buf;
	long long blockno;

	struct {
		struct btt_write_async_future btt_write_fut;
		int btt_write_started;
		unsigned lane;
	} internal;
};

struct pmemblk_write_async_output {
	int return_value;
};

FUTURE(pmemblk_write_async_fut, struct pmemblk_write_async_data,
		struct pmemblk_write_async_output);

struct pmemblk_write_async_fut pmemblk_write_async(PMEMblkpool *pbp, void *buf,
		long long blockno);
#endif

#ifdef __cplusplus
}
#endif
#endif	/* libpmemblk/async.h */
