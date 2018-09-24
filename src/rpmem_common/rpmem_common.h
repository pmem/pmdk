/*
 * Copyright 2016-2018, Intel Corporation
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

#ifndef RPMEM_COMMON_H
#define RPMEM_COMMON_H 1

/*
 * Values for SO_KEEPALIVE socket option
 */

#define RPMEM_CMD_ENV	"RPMEM_CMD"
#define RPMEM_SSH_ENV	"RPMEM_SSH"
#define RPMEM_DEF_CMD	"rpmemd"
#define RPMEM_DEF_SSH	"ssh"
#define RPMEM_PROV_SOCKET_ENV	"RPMEM_ENABLE_SOCKETS"
#define RPMEM_PROV_VERBS_ENV	"RPMEM_ENABLE_VERBS"
#define RPMEM_MAX_NLANES_ENV	"RPMEM_MAX_NLANES"
#define RPMEM_ACCEPT_TIMEOUT 30000
#define RPMEM_CONNECT_TIMEOUT 30000
#define RPMEM_MONITOR_TIMEOUT 1000

#include <stdint.h>
#include <sys/socket.h>
#include <netdb.h>

#ifdef __cplusplus
extern "C" {
#endif

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
	RPMEM_ERR_POOL_CFG	= 13,

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

const char *rpmem_persist_method_to_str(enum rpmem_persist_method pm);

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
	size_t buff_size;
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

#define RPMEM_HAS_USER		0x1
#define RPMEM_HAS_SERVICE	0x2
#define RPMEM_FLAGS_USE_IPV4	0x4
#define RPMEM_MAX_USER		(32 + 1)   /* see useradd(8) + 1 for '\0' */
#define RPMEM_MAX_NODE		(255 + 1)  /* see gethostname(2) + 1 for '\0' */
#define RPMEM_MAX_SERVICE	(NI_MAXSERV + 1)  /* + 1 for '\0' */
#define RPMEM_HDR_SIZE		4096
#define RPMEM_CLOSE_FLAGS_REMOVE 0x1
#define RPMEM_DEF_BUFF_SIZE	8192

struct rpmem_target_info {
	char user[RPMEM_MAX_USER];
	char node[RPMEM_MAX_NODE];
	char service[RPMEM_MAX_SERVICE];
	unsigned flags;
};

extern unsigned Rpmem_max_nlanes;
extern int Rpmem_fork_unsafe;

int rpmem_b64_write(int sockfd, const void *buf, size_t len, int flags);
int rpmem_b64_read(int sockfd, void *buf, size_t len, int flags);
const char *rpmem_get_ip_str(const struct sockaddr *addr);
struct rpmem_target_info *rpmem_target_parse(const char *target);
void rpmem_target_free(struct rpmem_target_info *info);
int rpmem_xwrite(int fd, const void *buf, size_t len, int flags);
int rpmem_xread(int fd, void *buf, size_t len, int flags);
char *rpmem_get_ssh_conn_addr(void);

#ifdef __cplusplus
}
#endif

#endif
