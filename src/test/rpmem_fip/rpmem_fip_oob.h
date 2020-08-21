/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

/*
 * rpmem_fip_sock.h -- simple oob connection implementation for exchanging
 * required RDMA related data
 */

#include <stdint.h>
#include <netinet/in.h>

typedef struct rpmem_ssh client_t;

client_t *client_exchange(struct rpmem_target_info *info,
		unsigned nlanes,
		enum rpmem_provider provider,
		struct rpmem_resp_attr *resp);
void client_close_begin(client_t *c);
void client_close_end(client_t *c);

void server_exchange_begin(unsigned *lanes, enum rpmem_provider *provider,
		char **addr);
void server_exchange_end(struct rpmem_resp_attr resp);

void server_close_begin(void);
void server_close_end(void);

void set_rpmem_cmd(const char *fmt, ...);
