/*
 * Copyright 2016-2017, Intel Corporation
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
	int (*close)(struct rpmemd_obc *obc, void *arg);
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
