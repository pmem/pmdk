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

/* TODO: Move those ifdefs out of the file */
#ifdef PMEMBLK_USE_MINIASYNC

#include <libpmemblk/base.h>
#include <libpmemblk/btt_async.h>
#include <libminiasync/vdm.h>

#ifdef __cplusplus
extern "C" {
#endif

/* START of pmemblk_read_async future */
enum pmemblk_read_stages{
	PMEMBLK_READ_INITIALIZED = 0,
	PMEMBLK_READ_WAITING_FOR_LANE = 1,
	PMEMBLK_READ_IN_PROGRESS = 2,
	PMEMBLK_READ_COMPLETE = 20,
};
struct pmemblk_read_async_future_data {
    PMEMblkpool *pbp;
    void *buf;
    long long blockno;

    int stage;
    struct {
	struct btt_read_async_future btt_read_fut;
	unsigned lane;
    } internal;
};

struct pmemblk_read_async_future_output {
    int return_value;
};

FUTURE(pmemblk_read_async_future, struct pmemblk_read_async_future_data,
	struct pmemblk_read_async_future_output);

struct pmemblk_read_async_future pmemblk_read_async(PMEMblkpool *pbp, void *buf,
	long long blockno);
/* END of pmemblk_read_async future */

/* START of pmemblk_write_async future */
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
/* END of pmemblk_write_async future */
#endif

#ifdef __cplusplus
}
#endif
#endif	/* libpmemblk/async.h */
