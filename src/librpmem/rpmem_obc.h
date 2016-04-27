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
 * rpmem_obc.h -- rpmem out-of-band connection client header file
 */

#include <sys/types.h>
#include <sys/socket.h>

#include "librpmem.h"

struct rpmem_obc;

struct rpmem_obc *rpmem_obc_init(void);
void rpmem_obc_fini(struct rpmem_obc *rpc);

int rpmem_obc_connect(struct rpmem_obc *rpc, const char *target);
int rpmem_obc_disconnect(struct rpmem_obc *rpc);
int rpmem_obc_get_addr(struct rpmem_obc *rpc,
		struct sockaddr *addr, socklen_t *addrlen);

int rpmem_obc_monitor(struct rpmem_obc *rpc, int nonblock);

int rpmem_obc_create(struct rpmem_obc *rpc,
		const struct rpmem_req_attr *req,
		struct rpmem_resp_attr *res,
		const struct rpmem_pool_attr *pool_attr);
int rpmem_obc_open(struct rpmem_obc *rpc,
		const struct rpmem_req_attr *req,
		struct rpmem_resp_attr *res,
		struct rpmem_pool_attr *pool_attr);
int rpmem_obc_remove(struct rpmem_obc *rpc, const char *pool_desc);
int rpmem_obc_close(struct rpmem_obc *rpc);
