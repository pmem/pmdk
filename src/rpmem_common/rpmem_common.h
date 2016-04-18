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
 * rpmem_common.h -- common definitions for librpmem and rpmemd
 */

/*
 * Values for SO_KEEPALIVE socket option
 */
/*
 * The time (in seconds) the connection needs to remain idle
 * before starting keepalive.
 */
#define RPMEM_TCP_KEEPIDLE	1

/*
 * The time (in seconds) between keepalive probes.
 */
#define RPMEM_TCP_KEEPINTVL	1

#include <sys/socket.h>

/*
 * rpmem_err -- error codes
 */
enum rpmem_err {
	RPMEM_SUCCESS		= 0,
	RPMEM_ERR_BADPROTO	= 1,
	RPMEM_ERR_BADNAME	= 2,
	RPMEM_ERR_BADSIZE	= 3,
	RPMEM_ERR_BADNLANES	= 4,
	RPMEM_ERR_BADPROVIDER	= 5,
	RPMEM_ERR_FATAL		= 6,
	RPMEM_ERR_FATAL_CONN	= 7,
	RPMEM_ERR_BUSY		= 8,
	RPMEM_ERR_EXISTS	= 9,
	RPMEM_ERR_PROVNOSUP	= 10,
	RPMEM_ERR_NOEXIST	= 11,
	RPMEM_ERR_NOACCESS	= 12,

	MAX_RPMEM_ERR,
};

/*
 * rpmem_persist_method -- remote persist operation method
 */
enum rpmem_persist_method {
	RPMEM_PM_GPSPM	= 1,	/* General Purpose Server Persistency Method */
	RPMEM_PM_APM	= 2,	/* Appliance Persistency Method */

	MAX_RPMEM_PM,
};

/*
 * rpmem_provider -- supported providers
 */
enum rpmem_provider {
	RPMEM_PROV_UNKNOWN = 0,
	RPMEM_PROV_LIBFABRIC_VERBS	= 1,
	RPMEM_PROV_LIBFABRIC_SOCKETS	= 2,

	MAX_RPMEM_PROV,
};

enum rpmem_provider rpmem_provider_from_str(const char *str);
const char *rpmem_provider_to_str(enum rpmem_provider provider);

/*
 * rpmem_req_attr -- arguments for open/create request
 */
struct rpmem_req_attr {
	size_t pool_size;
	unsigned nlanes;
	enum rpmem_provider provider;
	const char *pool_desc;
};

/*
 * rpmem_resp_attr -- return arguments from open/create request
 */
struct rpmem_resp_attr {
	unsigned short port;
	uint64_t rkey;
	uint64_t raddr;
	unsigned nlanes;
	enum rpmem_persist_method persist_method;
};

int rpmem_obc_send(int sockfd, const void *buf, size_t len);
int rpmem_obc_recv(int sockfd, void *buf, size_t len);
int rpmem_obc_keepalive(int fd);
const char *rpmem_get_ip_str(const struct sockaddr *addr);
int rpmem_target_split(const char *target, char **user,
		char **node, char **service);
int rpmem_xread(int fd, void *buf, size_t len);
int rpmem_xwrite(int fd, const void *buf, size_t len);
int rpmem_xsend(int fd, const void *buf, size_t len, int flags);
int rpmem_xrecv(int fd, void *buf, size_t len, int flags);
