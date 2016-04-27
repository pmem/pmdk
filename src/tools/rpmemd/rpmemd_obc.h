/*
 * Copyright 2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * rpmemd_obc.h -- rpmemd out-of-band connection declarations
 */

#include <sys/types.h>
#include <sys/socket.h>

struct rpmemd_obc;
struct rpmemd_obc_client;

struct rpmemd_obc_client_requests {
	int (*create)(struct rpmemd_obc_client *client, void *arg,
			const struct rpmem_req_attr *req,
			const struct rpmem_pool_attr *pool_attr);
	int (*open)(struct rpmemd_obc_client *client, void *arg,
			const struct rpmem_req_attr *req);
	int (*close)(struct rpmemd_obc_client *client, void *arg);
	int (*remove)(struct rpmemd_obc_client *client, void *arg,
			const char *pool_desc);
};

struct rpmemd_obc *rpmemd_obc_init(void);
void rpmemd_obc_fini(struct rpmemd_obc *rpdc);

int rpmemd_obc_listen(struct rpmemd_obc *rpdc, int backlog,
		const char *node, const char *service);
int rpmemd_obc_close(struct rpmemd_obc *rpdc);

struct rpmemd_obc_client *rpmemd_obc_accept(struct rpmemd_obc *rpdc);
void rpmemd_obc_client_fini(struct rpmemd_obc_client *client);

int rpmemd_obc_client_close(struct rpmemd_obc_client *client);
int rpmemd_obc_client_process(struct rpmemd_obc_client *client,
		struct rpmemd_obc_client_requests *req_cb, void *arg);
int rpmemd_obc_client_is_connected(struct rpmemd_obc_client *client);
int rpmemd_obc_client_getname(struct rpmemd_obc_client *client,
		struct sockaddr *addr, socklen_t *addrlen);
int rpmemd_obc_client_getpeer(struct rpmemd_obc_client *client,
		struct sockaddr *addr, socklen_t *addrlen);

int rpmemd_obc_client_create_resp(struct rpmemd_obc_client *client,
		int status, const struct rpmem_resp_attr *res);
int rpmemd_obc_client_open_resp(struct rpmemd_obc_client *client,
		int status, const struct rpmem_resp_attr *res,
		const struct rpmem_pool_attr *pool_attr);
int rpmemd_obc_client_close_resp(struct rpmemd_obc_client *client,
		int status);
int rpmemd_obc_client_remove_resp(struct rpmemd_obc_client *client,
		int status);
