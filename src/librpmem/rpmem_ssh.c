/*
 * Copyright 2016-2019, Intel Corporation
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
#include <sys/types.h>
#include <sys/wait.h>

#include "util.h"
#include "os.h"
#include "out.h"
#include "rpmem_common.h"
#include "rpmem_ssh.h"
#include "rpmem_cmd.h"
#include "rpmem_util.h"

#define ERR_BUFF_LEN	4095

/* +1 in order to be sure it is always null-terminated */
static char error_str[ERR_BUFF_LEN + 1];

struct rpmem_ssh {
	struct rpmem_cmd *cmd;
};

/*
 * get_ssh -- return ssh command name
 */
static const char *
get_ssh(void)
{
	char *cmd = os_getenv(RPMEM_SSH_ENV);
	if (!cmd)
		cmd = RPMEM_DEF_SSH;

	return cmd;
}

/*
 * get_user_at_node -- returns string containing user@node
 */
static char *
get_user_at_node(const struct rpmem_target_info *info)
{
	char *user_at_node = NULL;

	if (info->flags & RPMEM_HAS_USER) {
		size_t ulen = strlen(info->user);
		size_t nlen = strlen(info->node);
		size_t len = ulen + 1 + nlen + 1;
		user_at_node = malloc(len);
		if (!user_at_node)
			goto err_malloc;
		int ret = snprintf(user_at_node, len, "%s@%s",
				info->user, info->node);
		if (ret < 0 || (size_t)ret + 1 != len)
			goto err_printf;
	} else {
		user_at_node = strdup(info->node);
		if (!user_at_node)
			goto err_malloc;
	}

	return user_at_node;
err_printf:
	free(user_at_node);
err_malloc:
	return NULL;
}

/*
 * get_cmd -- return an RPMEM_CMD with appended list of arguments
 */
static char *
get_cmd(const char **argv)
{
	const char *env_cmd = rpmem_util_cmd_get();
	char *cmd = strdup(env_cmd);
	if (!cmd)
		return NULL;

	size_t cmd_len = strlen(cmd) + 1;

	const char *arg;
	while ((arg = *argv++) != NULL) {
		size_t len = strlen(arg);
		size_t new_cmd_len = cmd_len + len + 1;
		char *tmp = realloc(cmd, new_cmd_len);
		if (!tmp)
			goto err;

		cmd = tmp;

		/* append the argument to the command */
		cmd[cmd_len - 1] = ' ';
		memcpy(&cmd[cmd_len], arg, len);
		cmd[cmd_len + len] = '\0';

		cmd_len = new_cmd_len;
	}

	return cmd;
err:
	free(cmd);
	return NULL;
}

/*
 * valist_to_argv -- convert va_list to argv array
 */
static const char **
valist_to_argv(va_list args)
{
	const char **argv = malloc(sizeof(const char *));
	if (!argv)
		return NULL;

	argv[0] = NULL;
	size_t nargs = 0;

	const char *arg;
	while ((arg = va_arg(args, const char *)) != NULL) {
		nargs++;
		const char **tmp = realloc(argv,
				(nargs + 1) * sizeof(const char *));
		if (!tmp)
			goto err;

		argv = tmp;
		argv[nargs - 1] = arg;
		argv[nargs] = NULL;
	}

	return argv;
err:
	free(argv);
	return NULL;
}

/*
 * rpmem_ssh_execv -- open ssh connection and run $RPMEMD_CMD with
 * additional NULL-terminated list of arguments.
 */
struct rpmem_ssh *
rpmem_ssh_execv(const struct rpmem_target_info *info, const char **argv)
{
	struct rpmem_ssh *rps = calloc(1, sizeof(*rps));
	if (!rps)
		goto err_zalloc;

	char *user_at_node = get_user_at_node(info);
	if (!user_at_node)
		goto err_user_node;

	rps->cmd = rpmem_cmd_init();
	if (!rps->cmd)
		goto err_cmd_init;

	char *cmd = get_cmd(argv);
	if (!cmd)
		goto err_cmd;

	int ret = rpmem_cmd_push(rps->cmd, get_ssh());
	if (ret)
		goto err_push;

	if (info->flags & RPMEM_HAS_SERVICE) {
		/* port number is optional */
		ret = rpmem_cmd_push(rps->cmd, "-p");
		if (ret)
			goto err_push;
		ret = rpmem_cmd_push(rps->cmd, info->service);
		if (ret)
			goto err_push;
	}

	/*
	 * Disable allocating pseudo-terminal in order to transfer binary
	 * data safely.
	 */
	ret = rpmem_cmd_push(rps->cmd, "-T");
	if (ret)
		goto err_push;

	if (info->flags & RPMEM_FLAGS_USE_IPV4) {
		ret = rpmem_cmd_push(rps->cmd, "-4");
		if (ret)
			goto err_push;
	}

	/* fail if password required for authentication */
	ret = rpmem_cmd_push(rps->cmd, "-oBatchMode=yes");
	if (ret)
		goto err_push;

	ret = rpmem_cmd_push(rps->cmd, user_at_node);
	if (ret)
		goto err_push;

	ret = rpmem_cmd_push(rps->cmd, cmd);
	if (ret)
		goto err_push;

	ret = rpmem_cmd_run(rps->cmd);
	if (ret)
		goto err_run;

	free(user_at_node);
	free(cmd);

	return rps;
err_run:
err_push:
	free(cmd);
err_cmd:
	rpmem_cmd_fini(rps->cmd);
err_cmd_init:
	free(user_at_node);
err_user_node:
	free(rps);
err_zalloc:
	return NULL;
}

/*
 * rpmem_ssh_exec -- open ssh connection and run $RPMEMD_CMD with
 * additional NULL-terminated list of arguments.
 */
struct rpmem_ssh *
rpmem_ssh_exec(const struct rpmem_target_info *info, ...)
{
	struct rpmem_ssh *ssh;

	va_list args;
	va_start(args, info);

	const char **argv = valist_to_argv(args);
	if (argv)
		ssh = rpmem_ssh_execv(info, argv);
	else
		ssh = NULL;

	va_end(args);

	free(argv);

	return ssh;
}

/*
 * rpmem_ssh_open -- open ssh connection with specified node and wait for status
 */
struct rpmem_ssh *
rpmem_ssh_open(const struct rpmem_target_info *info)
{
	struct rpmem_ssh *ssh = rpmem_ssh_exec(info, NULL);
	if (!ssh)
		return NULL;

	/*
	 * Read initial status from invoked command.
	 * This is for synchronization purposes and to make it possible
	 * to inform client that command's initialization failed.
	 */
	int32_t status;
	int ret = rpmem_ssh_recv(ssh, &status, sizeof(status));
	if (ret) {
		if (ret == 1 || errno == ECONNRESET)
			ERR("%s", rpmem_ssh_strerror(ssh, errno));
		else
			ERR("!%s", info->node);
		goto err_recv_status;
	}

	if (status) {
		ERR("%s: unexpected status received -- '%d'",
				info->node, status);
		errno = status;
		goto err_status;
	}

	RPMEM_LOG(INFO, "received status: %u", status);

	return ssh;
err_recv_status:
err_status:
	rpmem_ssh_close(ssh);
	return NULL;
}

/*
 * rpmem_ssh_close -- close ssh connection
 */
int
rpmem_ssh_close(struct rpmem_ssh *rps)
{
	int ret, rv;

	rpmem_cmd_term(rps->cmd);
	rv = rpmem_cmd_wait(rps->cmd, &ret);
	if (rv)
		return rv;

	rpmem_cmd_fini(rps->cmd);
	free(rps);

	if (WIFEXITED(ret))
		return WEXITSTATUS(ret);

	if (WIFSIGNALED(ret)) {
		ERR("signal received -- %d", WTERMSIG(ret));
		return -1;
	}

	ERR("exit status -- %d", WEXITSTATUS(ret));

	return -1;
}

/*
 * rpmem_ssh_send -- send data using ssh transport layer
 *
 * The data is encoded using base64.
 */
int
rpmem_ssh_send(struct rpmem_ssh *rps, const void *buff, size_t len)
{
	int ret = rpmem_xwrite(rps->cmd->fd_in, buff, len, MSG_NOSIGNAL);
	if (ret == 1) {
		errno = ECONNRESET;
	} else if (ret < 0) {
		if (errno == EPIPE)
			errno = ECONNRESET;
	}

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
	int ret = rpmem_xread(rps->cmd->fd_out, buff,
			len, MSG_NOSIGNAL);
	if (ret == 1) {
		errno = ECONNRESET;
	} else if (ret < 0) {
		if (errno == EPIPE)
			errno = ECONNRESET;
	}

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
rpmem_ssh_strerror(struct rpmem_ssh *rps, int oerrno)
{
	size_t len = 0;
	ssize_t ret;
	while ((ret = read(rps->cmd->fd_err, error_str + len,
			ERR_BUFF_LEN - len))) {
		if (ret < 0)
			return "reading error string failed";

		len += (size_t)ret;
	}
	error_str[len] = '\0';

	if (len == 0) {
		if (oerrno) {
			char buff[UTIL_MAX_ERR_MSG];
			util_strerror(oerrno, buff, UTIL_MAX_ERR_MSG);
			ret = snprintf(error_str, ERR_BUFF_LEN,
				"%s", buff);
			if (ret < 0)
				return "reading error string failed";
		} else {
			ret = snprintf(error_str, ERR_BUFF_LEN,
				"unknown error");
			if (ret < 0)
				return "reading error string failed";
		}
	} else {
		/* get rid of new line and carriage return chars */
		char *cr = strchr(error_str, '\r');
		if (cr)
			*cr = '\0';

		char *nl = strchr(error_str, '\n');
		if (nl)
			*nl = '\0';
	}

	return error_str;
}
