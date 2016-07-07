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
 * rpmem_ssh.c -- rpmem ssh transport layer source file
 */

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "util.h"
#include "out.h"
#include "rpmem_ssh.h"
#include "rpmem_cmd.h"
#include "rpmem_common.h"
#include "rpmem_util.h"
#include "base64.h"

#define ERR_BUFF_SIZE	4096

static char error_str[ERR_BUFF_SIZE];

struct rpmem_ssh {
	struct rpmem_cmd *cmd;
	char *node;		/* target node */
	char *service;		/* target node service */
};

/*
 * get_ssh -- return ssh command name
 */
static const char *
get_ssh(void)
{
	char *cmd = getenv(RPMEM_SSH_ENV);
	if (!cmd)
		cmd = RPMEM_DEF_SSH;

	return cmd;
}

/*
 * get_cmd -- return command name
 */
static const char *
get_cmd(void)
{
	char *cmd = getenv(RPMEM_CMD_ENV);
	if (!cmd)
		cmd = RPMEM_DEF_CMD;

	return cmd;
}

/*
 * rpmem_ssh_open -- open ssh connection with specified node
 */
struct rpmem_ssh *
rpmem_ssh_open(const char *node, const char *service)
{
	struct rpmem_ssh *rps = calloc(1, sizeof(*rps));
	if (!rps)
		goto err_zalloc;

	rps->node = strdup(node);
	if (!rps->node)
		goto err_strdup_node;

	if (service) {
		rps->service = strdup(service);
		if (!rps->service)
			goto err_strdup_service;
	}

	rps->cmd = rpmem_cmd_init();
	if (!rps->cmd)
		goto err_cmd_init;
	int ret = rpmem_cmd_push(rps->cmd, get_ssh());
	if (ret)
		goto err_push;

	if (rps->service) {
		/* port number is optional */
		ret = rpmem_cmd_push(rps->cmd, "-p");
		if (ret)
			goto err_push;
		ret = rpmem_cmd_push(rps->cmd, rps->service);
		if (ret)
			goto err_push;
	}

	/* XXX add support for IPv6 */
	ret = rpmem_cmd_push(rps->cmd, "-4");
	if (ret)
		goto err_push;

	/* fail if password required for authentication */
	ret = rpmem_cmd_push(rps->cmd, "-oBatchMode=yes");
	if (ret)
		goto err_push;

	ret = rpmem_cmd_push(rps->cmd, rps->node);
	if (ret)
		goto err_push;

	ret = rpmem_cmd_push(rps->cmd, get_cmd());
	if (ret)
		goto err_push;

	ret = rpmem_cmd_run(rps->cmd);
	if (ret)
		goto err_run;

	/*
	 * Read initial status from invoked command.
	 * This is for synchronization purposes and to make it possible
	 * to inform client that command's initialization failed.
	 */
	int32_t status;
	ret = rpmem_ssh_recv(rps, &status, sizeof(status));
	if (ret) {
		if (ret == 1 || errno == ECONNRESET)
			ERR("%s", rpmem_ssh_strerror(rps));
		else
			ERR("!%s", rps->node);
		goto err_recv_status;
	}

	if (status) {
		ERR("%s: unexpected status received -- '%d'",
				rps->node, status);
		goto err_status;
	}

	return rps;
err_status:
err_recv_status:
err_run:
	rpmem_cmd_term(rps->cmd);
	rpmem_cmd_wait(rps->cmd, NULL);
err_push:
	rpmem_cmd_fini(rps->cmd);
err_cmd_init:
	free(rps->service);
err_strdup_service:
	free(rps->node);
err_strdup_node:
	free(rps);
err_zalloc:
	return NULL;
}

/*
 * rpmem_ssh_close -- close ssh connection
 */
int
rpmem_ssh_close(struct rpmem_ssh *rps)
{
	int ret;

	rpmem_cmd_term(rps->cmd);
	rpmem_cmd_wait(rps->cmd, &ret);
	rpmem_cmd_fini(rps->cmd);
	free(rps->node);
	free(rps->service);
	free(rps);

	if (WIFEXITED(ret))
		return 0;
	if (WIFSIGNALED(ret)) {
		ERR("signal received -- %d", WTERMSIG(ret));
		return ret;
	}

	ERR("exit status -- %d", WEXITSTATUS(ret));

	return ret;
}

/*
 * rpmem_ssh_send -- send data using ssh transport layer
 *
 * The data is encoded using base64.
 */
int
rpmem_ssh_send(struct rpmem_ssh *rps, const void *buff, size_t len)
{
	size_t b64_len;
	void *b64_buff = base64_buff(len, &b64_len);
	if (!b64_buff)
		return -1;

	int ret = base64_encode(buff, len, b64_buff, b64_len);
	if (ret)
		return ret;

	ret = rpmem_xwrite(rps->cmd->fd_in, b64_buff, b64_len, MSG_NOSIGNAL);
	if (ret == 1) {
		errno = ECONNRESET;
	} else if (ret < 0) {
		if (errno == EPIPE)
			errno = ECONNRESET;
	}

	free(b64_buff);

	return ret;
}

/*
 * rpmem_ssh_recv -- receive data using ssh transport layer
 *
 * The received data is decoded using base64.
 */
int
rpmem_ssh_recv(struct rpmem_ssh *rps, void *buff, size_t len)
{
	size_t b64_len;
	void *b64_buff = base64_buff(len, &b64_len);
	if (!b64_buff)
		return -1;

	int ret = rpmem_xread(rps->cmd->fd_out, b64_buff,
			b64_len, MSG_NOSIGNAL);
	if (ret == 1) {
		errno = ECONNRESET;
	} else if (ret < 0) {
		if (errno == EPIPE)
			errno = ECONNRESET;
	} else {
		ret = base64_decode(b64_buff, b64_len, buff, len);
	}

	free(b64_buff);

	return ret;
}

/*
 * rpmem_ssh_monitor -- check connection state of ssh
 *
 * Return value:
 * 0  - disconnected
 * 1  - connected
 * <0 - error
 */
int
rpmem_ssh_monitor(struct rpmem_ssh *rps, int nonblock)
{
	uint32_t buff;
	int flags = MSG_PEEK;
	if (nonblock)
		flags |= MSG_DONTWAIT;

	int ret = rpmem_xread(rps->cmd->fd_out, &buff, sizeof(buff), flags);

	if (!ret) {
		RPMEM_LOG(ERR, "unexpected data received");
		errno = EPROTO;
		return -1;
	}

	if (ret < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 1;
		else
			return ret;
	}

	return 0;
}

/*
 * rpmem_ssh_strerror -- read error using stderr channel
 */
const char *
rpmem_ssh_strerror(struct rpmem_ssh *rps)
{
	ssize_t ret = read(rps->cmd->fd_err, error_str, ERR_BUFF_SIZE);
	if (ret < 0)
		return "reading error string failed";

	if (ret == 0) {
		if (errno) {
			snprintf(error_str, ERR_BUFF_SIZE,
				"%s: %s", rps->node, strerror(errno));
		} else {
			snprintf(error_str, ERR_BUFF_SIZE,
				"%s: unknown error", rps->node);
		}

		return error_str;
	}

	/* get rid of new line and carriage return chars */
	char *cr = strchr(error_str, '\r');
	if (cr)
		*cr = '\0';

	char *nl = strchr(error_str, '\n');
	if (nl)
		*nl = '\0';

	return error_str;
}
