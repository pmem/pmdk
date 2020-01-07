// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * rpmemd_obc.h -- rpmemd out-of-band connection declarations
 */
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

struct rpmemd_obc;

struct rpmemd_obc_requests {
	int (*create)(struct rpmemd_obc *obc, void *arg,
			const struct rpmem_req_attr *req,
			const struct rpmem_pool_attr *pool_attr);
	int (*open)(struct rpmemd_obc *obc, void *arg,
			const struct rpmem_req_attr *req);
	int (*close)(struct rpmemd_obc *obc, void *arg, int flags);
	int (*set_attr)(struct rpmemd_obc *obc, void *arg,
			const struct rpmem_pool_attr *pool_attr);
};

struct rpmemd_obc *rpmemd_obc_init(int fd_in, int fd_out);
void rpmemd_obc_fini(struct rpmemd_obc *obc);

int rpmemd_obc_status(struct rpmemd_obc *obc, uint32_t status);

int rpmemd_obc_process(struct rpmemd_obc *obc,
		struct rpmemd_obc_requests *req_cb, void *arg);

int rpmemd_obc_create_resp(struct rpmemd_obc *obc,
		int status, const struct rpmem_resp_attr *res);
int rpmemd_obc_open_resp(struct rpmemd_obc *obc,
		int status, const struct rpmem_resp_attr *res,
		const struct rpmem_pool_attr *pool_attr);
int rpmemd_obc_set_attr_resp(struct rpmemd_obc *obc, int status);
int rpmemd_obc_close_resp(struct rpmemd_obc *obc,
		int status);
