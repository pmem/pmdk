/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2022, Intel Corporation */

/*
 * libpmemblk/async.h -- definitions of libpmemblk entry points for async
 * operations
 *
 * See libpmemblk(7) for details.
 */

#ifndef BTT_ASYNC_H
#define BTT_ASYNC_H 1

#ifdef PMEMBLK_USE_MINIASYNC

#include <libpmemblk/base.h>
#include <libminiasync/vdm.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Asynchronous callbacks */

/* START of nsread_async */
struct nsread_async_future_data {
    void *ns,
    unsigned lane,
    void *buf,
    size_t count,
    uint64_t off,

    struct vdm_operation op;
    struct vdm *vdm;
};

struct nsread_async_future_output {
    int output;
};

FUTURE(nsread_async_future, struct nsread_async_future_data,
		struct nsread_async_future_output);
/* END of nsread_async */

/* START of nswrite_async */
struct nswrite_async_future_data {
    void *ns,
    unsigned lane,
    void *buf,
    size_t count,
    uint64_t off

    struct vdm_operation op;
    struct vdm *vdm;
};

struct nswrite_async_future_output {
    int output;
};

FUTURE(nswrite_async_future, struct nswrite_async_future_data,
		struct nswrite_async_future_output);
/* END of nswrite_async */

/* TODO: Could be in a private header!!! */
struct ns_callback_async {
	struct nsread_async_future (*nsread)(void *ns, unsigned lane,
		void *buf, size_t count, uint64_t off, struct vdm *vdm);
	struct nswrite_async_future (*nswrite)(void *ns, unsigned lane,
		void *buf, size_t count, uint64_t off, struct vdm *vdm);
	/* TODO: finish the list!!! */
	int (*nszero)(void *ns, unsigned lane, size_t count, uint64_t off);
	ssize_t (*nsmap)(void *ns, unsigned lane, void **addrp,
			size_t len, uint64_t off);
	void (*nssync)(void *ns, unsigned lane, void *addr, size_t len);

	int ns_is_zeroed;
}

/* BTT futures */

/* START of btt_read_async */
struct btt_read_async_future_data {
    struct btt *bttp,
    unsigned lane,
    uint64_t lba,
    void *buf

    int stage;
    struct vdm *vdm;
};

struct btt_read_async_future_output {
    int output;
};

FUTURE(btt_read_async_future, struct btt_read_async_future_data,
		struct btt_read_async_future_output);

struct btt_read_async_future btt_read_async(struct btt *bttp, unsigned lane,
	uint64_t lba, void *buf, struct vdm *vdm);
/* END of btt_read_async */

/* START of btt_write_async */
struct btt_write_async_future_data {
    struct btt *bttp,
    unsigned lane,
    uint64_t lba,
    void *buf

    struct vdm *vdm;
};

struct btt_write_async_future_output {
    int output;
};

FUTURE(btt_write_async_future, struct btt_write_async_future_data,
		struct btt_write_async_future_output);

struct btt_write_async_future btt_write_async(struct btt *bttp, unsigned lane,
	uint64_t lba, void *buf, struct vdm *vdm);
/* END of btt_write_async */

#endif

#ifdef __cplusplus
}
#endif
#endif	/* libpmemblk/btt_async.h */
