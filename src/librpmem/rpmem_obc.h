// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * rpmem_obc.h -- rpmem out-of-band connection client header file
 */

#ifndef RPMEM_OBC_H
#define RPMEM_OBC_H 1

#include <sys/types.h>
#include <sys/socket.h>

#include "librpmem.h"

#ifdef __cplusplus
extern "C" {
#endif

struct rpmem_obc;

struct rpmem_obc *rpmem_obc_init(void);
void rpmem_obc_fini(struct rpmem_obc *rpc);

int rpmem_obc_connect(struct rpmem_obc *rpc,
		const struct rpmem_target_info *info);
int rpmem_obc_disconnect(struct rpmem_obc *rpc);

int rpmem_obc_monitor(struct rpmem_obc *rpc, int nonblock);

int rpmem_obc_create(struct rpmem_obc *rpc,
		const struct rpmem_req_attr *req,
		struct rpmem_resp_attr *res,
		const struct rpmem_pool_attr *pool_attr);
int rpmem_obc_open(struct rpmem_obc *rpc,
		const struct rpmem_req_attr *req,
		struct rpmem_resp_attr *res,
		struct rpmem_pool_attr *pool_attr);
int rpmem_obc_set_attr(struct rpmem_obc *rpc,
		const struct rpmem_pool_attr *pool_attr);
int rpmem_obc_close(struct rpmem_obc *rpc, int flags);

#ifdef __cplusplus
}
#endif

#endif
