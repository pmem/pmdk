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

#include <libminiasync/vdm.h>

#ifdef __cplusplus
extern "C" {
#endif

struct btt_get_free_block_data {
	struct btt *bttp;
	unsigned lane;
	uint64_t lba;
};

struct btt_get_free_block_output {
	void *block;
	size_t lbasize;
};

FUTURE(btt_get_free_block_fut, struct btt_get_free_block_data,
		struct btt_get_free_block_output);

struct blk_lane_enter_data {
	PMEMblkpool *pbp;
};

struct blk_lane_enter_output {
	unsigned int lane;
};

FUTURE(blk_lane_enter_fut, struct blk_lane_enter_data,
		struct blk_lane_enter_output);

struct blk_lane_exit_data {
	PMEMblkpool *pbp;
	unsigned int lane;
};

struct blk_lane_exit_output {
	uint64_t unused; /* Avoid compiled empty struct error */
};

FUTURE(blk_lane_exit_fut, struct blk_lane_exit_data,
		struct blk_lane_exit_output);

struct blk_pre_block_write_data {
	PMEMblkpool *pbp;
	void *block;
	size_t lbasize;
};

struct blk_pre_block_write_output {
	uint64_t unused; /* Avoid compiled empty struct error */
};

FUTURE(blk_pre_block_write_fut, struct blk_pre_block_write_data,
		struct blk_pre_block_write_output);

struct blk_post_block_write_data {
	PMEMblkpool *pbp;
	void *block;
	size_t lbasize;
};

struct blk_post_block_write_output {
	uint64_t unused; /* Avoid compiled empty struct error */
};

FUTURE(blk_post_block_write_fut, struct blk_post_block_write_data,
		struct blk_post_block_write_output);

struct blk_write_data {
	FUTURE_CHAIN_ENTRY(struct btt_get_free_block_fut, get_free_block);
	FUTURE_CHAIN_ENTRY(struct blk_pre_block_write_fut, pre_write);
	FUTURE_CHAIN_ENTRY(struct vdm_operation_future, write);
	FUTURE_CHAIN_ENTRY_LAST(struct blk_post_block_write_fut, post_write);
};

struct blk_write_output {
	void *dest;
};

FUTURE(blk_write_fut, struct blk_write_data, struct blk_write_output);

struct pmemblk_write_async_data {
	FUTURE_CHAIN_ENTRY(struct blk_lane_enter_fut, lane_enter);
	FUTURE_CHAIN_ENTRY(struct blk_write_fut, write);
	FUTURE_CHAIN_ENTRY_LAST(struct blk_lane_exit_fut, lane_exit);
};

struct pmemblk_write_async_output {
	void *dest;
};

FUTURE(pmemblk_write_async_fut, struct pmemblk_write_async_data,
		struct pmemblk_write_async_output);

struct pmemblk_write_async_fut pmemblk_write_async(PMEMblkpool *pbp, void *buf,
		long long blockno, struct vdm *vdm);

struct pmemblk_write_async_fut pmemblk_write_async(PMEMblkpool *pbp, void *buf,
		long long blockno, struct vdm *vdm);
#endif

#ifdef __cplusplus
}
#endif
#endif	/* libpmemblk/async.h */
