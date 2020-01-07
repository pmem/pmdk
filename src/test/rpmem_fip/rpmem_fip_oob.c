// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2017, Intel Corporation */

/*
 * rpmem_fip_oob.c -- simple oob connection implementation for exchanging
 * required RDMA related data
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#include "rpmem_common.h"
#include "rpmem_proto.h"
#include "rpmem_fip_oob.h"
#include "rpmem_ssh.h"
#include "unittest.h"
#include "rpmem_util.h"
#include "os.h"

#define CMD_BUFF_SIZE	4096
static const char *rpmem_cmd;

/*
 * set_rpmem_cmd -- set RPMEM_CMD variable
 */
void
set_rpmem_cmd(const char *fmt, ...)
{
	static char cmd_buff[CMD_BUFF_SIZE];

	if (!rpmem_cmd) {
		char *cmd = os_getenv(RPMEM_CMD_ENV);
		UT_ASSERTne(cmd, NULL);
		rpmem_cmd = STRDUP(cmd);
	}

	ssize_t ret;
	size_t cnt = 0;

	va_list ap;
	va_start(ap, fmt);
	ret = snprintf(&cmd_buff[cnt], CMD_BUFF_SIZE - cnt,
			"%s ", rpmem_cmd);
	UT_ASSERT(ret > 0);
	cnt += (size_t)ret;

	ret = vsnprintf(&cmd_buff[cnt], CMD_BUFF_SIZE - cnt, fmt, ap);
	UT_ASSERT(ret > 0);
	cnt += (size_t)ret;

	va_end(ap);

	ret = os_setenv(RPMEM_CMD_ENV, cmd_buff, 1);
	UT_ASSERTeq(ret, 0);

	/*
	 * Rpmem has internal RPMEM_CMD variable copy and it is assumed
	 * RPMEMD_CMD will not change its value during execution. To refresh the
	 * internal copy it must be destroyed and a instance must be initialized
	 * manually.
	 */
	rpmem_util_cmds_fini();
	rpmem_util_cmds_init();
}

/*
 * client_exchange -- connect to remote host and exchange required information
 */
client_t *
client_exchange(struct rpmem_target_info *info,
	unsigned nlanes,
	enum rpmem_provider provider,
	struct rpmem_resp_attr *resp)
{
	struct rpmem_ssh *ssh = rpmem_ssh_open(info);
	UT_ASSERTne(ssh, NULL);

	int ret;

	ret = rpmem_ssh_send(ssh, &nlanes, sizeof(nlanes));
	UT_ASSERTeq(ret, 0);

	ret = rpmem_ssh_send(ssh, &provider, sizeof(provider));
	UT_ASSERTeq(ret, 0);

	ret = rpmem_ssh_recv(ssh, resp, sizeof(*resp));
	UT_ASSERTeq(ret, 0);

	return ssh;
}

/*
 * client_close_begin -- begin closing connection
 */
void
client_close_begin(client_t *c)
{
	int cmd = 1;
	int ret;

	ret = rpmem_ssh_send(c, &cmd, sizeof(cmd));
	UT_ASSERTeq(ret, 0);

	ret = rpmem_ssh_recv(c, &cmd, sizeof(cmd));
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(cmd, 0);
}

/*
 * client_close_end -- end closing connection
 */
void
client_close_end(client_t *c)
{
	rpmem_ssh_close(c);
}

/*
 * server_exchange_begin -- accept a connection and read required information
 */
void
server_exchange_begin(unsigned *lanes, enum rpmem_provider *provider,
	char **addr)
{
	UT_ASSERTne(addr, NULL);

	char *conn = rpmem_get_ssh_conn_addr();
	UT_ASSERTne(conn, NULL);

	*addr = strdup(conn);
	UT_ASSERTne(*addr, NULL);

	uint32_t status = 0;

	WRITE(STDOUT_FILENO, &status, sizeof(status));
	READ(STDIN_FILENO, lanes, sizeof(*lanes));
	READ(STDIN_FILENO, provider, sizeof(*provider));
}

/*
 * server_exchange_end -- send response to client
 */
void
server_exchange_end(struct rpmem_resp_attr resp)
{
	WRITE(STDOUT_FILENO, &resp, sizeof(resp));
}

/*
 * server_close_begin -- wait for close command
 */
void
server_close_begin(void)
{
	int cmd = 0;

	READ(STDIN_FILENO, &cmd, sizeof(cmd));
	UT_ASSERTeq(cmd, 1);
}

/*
 * server_close_end -- send close response and wait for disconnect
 */
void
server_close_end(void)
{
	int cmd = 0;

	WRITE(STDOUT_FILENO, &cmd, sizeof(cmd));
}
