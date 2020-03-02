// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2020, Intel Corporation */

/*
 * rpmem_cmd.c -- simple interface for running an executable in child process
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#include "util.h"
#include "out.h"
#include "os.h"
#include "rpmem_common.h"
#include "rpmem_util.h"
#include "rpmem_cmd.h"

/*
 * rpmem_cmd_init -- initialize command
 */
struct rpmem_cmd *
rpmem_cmd_init(void)
{
	struct rpmem_cmd *cmd = calloc(1, sizeof(*cmd));
	if (!cmd) {
		RPMEM_LOG(ERR, "allocating command buffer");
		goto err_alloc_cmd;
	}

	return cmd;
err_alloc_cmd:
	return NULL;
}

/*
 * rpmem_cmd_fini -- deinitialize command
 */
void
rpmem_cmd_fini(struct rpmem_cmd *cmd)
{
	for (int i = 0; i < cmd->args.argc; i++)
		free(cmd->args.argv[i]);
	free(cmd->args.argv);
	free(cmd);
}

/*
 * rpmem_cmd_push -- push back command's argument
 */
int
rpmem_cmd_push(struct rpmem_cmd *cmd, const char *arg)
{
	size_t argv_count = (size_t)cmd->args.argc + 2;
	char **argv = realloc(cmd->args.argv, argv_count * sizeof(char *));
	if (!argv) {
		RPMEM_LOG(ERR, "reallocating command argv");
		goto err_realloc;
	}

	cmd->args.argv = argv;

	char *arg_dup = strdup(arg);
	if (!arg_dup) {
		RPMEM_LOG(ERR, "allocating argument");
		goto err_strdup;
	}

	cmd->args.argv[cmd->args.argc] = arg_dup;
	cmd->args.argc++;
	cmd->args.argv[cmd->args.argc] = NULL;

	return 0;
err_strdup:
err_realloc:
	return -1;
}

/*
 * rpmem_cmd_log -- print executing command
 */
static void
rpmem_cmd_log(struct rpmem_cmd *cmd)
{
	RPMEM_ASSERT(cmd->args.argc > 0);

	size_t size = 0;
	for (int i = 0; i < cmd->args.argc; i++) {
		size += strlen(cmd->args.argv[i]) + 1;
	}

	char *buff = malloc(size);
	if (!buff) {
		RPMEM_LOG(ERR, "allocating log buffer for command");
		return;
	}

	size_t pos = 0;

	for (int i = 0; pos < size && i < cmd->args.argc; i++) {
		int ret = util_snprintf(&buff[pos], size - pos, "%s%s",
				cmd->args.argv[i], i == cmd->args.argc - 1 ?
				"" : " ");
		if (ret < 0) {
			RPMEM_LOG(ERR, "!snprintf");
			goto out;
		}

		pos += (size_t)ret;
	}

	RPMEM_LOG(INFO, "executing command '%s'", buff);

out:
	free(buff);
}

/*
 * rpmem_cmd_run -- run command and connect with stdin, stdout and stderr
 * using unix sockets.
 *
 * The communication with child process is done via socketpairs on
 * stdin, stdout and stderr. The socketpairs are used instead of pipes
 * because reading from disconnected pipe causes a SIGPIPE signal.
 * When using socketpair it is possible to read data using recv(3)
 * function with MSG_NOSIGNAL flag, which doesn't send a signal.
 */
int
rpmem_cmd_run(struct rpmem_cmd *cmd)
{
	int fd_in[2];
	int fd_out[2];
	int fd_err[2];

	rpmem_cmd_log(cmd);

	/* socketpair for stdin */
	int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fd_in);
	if (ret < 0) {
		RPMEM_LOG(ERR, "creating pipe for stdin");
		goto err_pipe_in;
	}

	/* parent process stdin socket */
	cmd->fd_in = fd_in[1];

	/* socketpair for stdout */
	ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fd_out);
	if (ret < 0) {
		RPMEM_LOG(ERR, "creating pipe for stdout");
		goto err_pipe_out;
	}

	/* parent process stdout socket */
	cmd->fd_out = fd_out[0];

	/* socketpair for stderr */
	ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fd_err);
	if (ret < 0) {
		RPMEM_LOG(ERR, "creating pipe for stderr");
		goto err_pipe_err;
	}

	/* socketpair for stderr */
	cmd->fd_err = fd_err[0];

	cmd->pid = fork();

	if (cmd->pid == -1) {
		RPMEM_LOG(ERR, "forking command");
		goto err_fork;
	}

	if (!cmd->pid) {
		dup2(fd_in[0], 0);
		dup2(fd_out[1], 1);
		dup2(fd_err[1], 2);

		execvp(cmd->args.argv[0], cmd->args.argv);
		exit(EXIT_FAILURE);
	}

	os_close(fd_in[0]);
	os_close(fd_out[1]);
	os_close(fd_err[1]);

	return 0;
err_fork:
	os_close(fd_err[0]);
	os_close(fd_err[1]);
err_pipe_err:
	os_close(fd_out[0]);
	os_close(fd_out[1]);
err_pipe_out:
	os_close(fd_in[0]);
	os_close(fd_in[1]);
err_pipe_in:
	return -1;
}

/*
 * rpmem_cmd_wait -- wait for process to change state
 */
int
rpmem_cmd_wait(struct rpmem_cmd *cmd, int *status)
{
	if (cmd->pid <= 0) {
		RPMEM_LOG(ERR, "wrong PID: %i", cmd->pid);
		errno = EINVAL;
		return -1;
	}

	if (waitpid(cmd->pid, status, 0) != cmd->pid) {
		RPMEM_LOG(ERR, "!waitpid failed");
		return -1;
	}

	return 0;
}

/*
 * rpmem_cmd_term -- close child process's unix sockets
 */
void
rpmem_cmd_term(struct rpmem_cmd *cmd)
{
	os_close(cmd->fd_in);
	os_close(cmd->fd_out);
	os_close(cmd->fd_err);

	RPMEM_ASSERT(cmd->pid > 0);
}
